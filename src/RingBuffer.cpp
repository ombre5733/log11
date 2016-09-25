/*******************************************************************************
  Copyright (c) 2016, Manuel Freiberger
  All rights reserved.

  Redistribution and use in source and binary forms, with or without
  modification, are permitted provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright notice, this
    list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above copyright notice,
    this list of conditions and the following disclaimer in the documentation
    and/or other materials provided with the distribution.

  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
  AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
  IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
  DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE
  FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
  DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
  SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
  CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY,
  OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
  OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*******************************************************************************/

#include "RingBuffer.hpp"

#if defined(LOG11_USE_WEOS)
#include <weos/iterator.hpp>
#endif // LOG11_USE_WEOS

#include <cstdint>
#include <cstring>


using namespace log11;
using namespace std;

// ----=====================================================================----
//     RingBuffer::Stream
// ----=====================================================================----

RingBuffer::Stream::Stream(
        RingBuffer& buffer, unsigned begin, unsigned length) noexcept
    : m_buffer(buffer),
      m_begin(begin),
      m_length(length)
{
}

bool RingBuffer::Stream::read(void* dest, unsigned size) noexcept
{
    if (size <= m_length)
    {
        m_buffer.read(m_begin, dest, size);
        m_begin += size;
        m_length -= size;
        return true;
    }
    else
    {
        m_length = 0;
        return false;
    }
}

bool RingBuffer::Stream::write(const void* source, unsigned size) noexcept
{
    if (size <= m_length)
    {
        m_buffer.write(m_begin, source, size);
        m_begin += size;
        m_length -= size;
        return true;
    }
    else
    {
        m_length = 0;
        return false;
    }
}

// ----=====================================================================----
//     RingBuffer::Block
// ----=====================================================================----

RingBuffer::Stream RingBuffer::Block::stream(RingBuffer& buffer) noexcept
{
    return RingBuffer::Stream(buffer,
                              m_begin + header_size, m_length - header_size);
}

// ----=====================================================================----
//     RingBuffer
// ----=====================================================================----

RingBuffer::RingBuffer(unsigned exponent)
    : m_size(1u << exponent),
      m_claimed(0),
      m_published(0),
      m_consumed(0),
      m_stashCount(0)
{
    for (auto& stashed : m_stash)
        stashed = 0;

    m_data = ::operator new(m_size);
}

RingBuffer::~RingBuffer()
{
    ::operator delete(m_data);
}

auto RingBuffer::claim(unsigned numElements) -> Block
{
    numElements += 2;
    if (numElements > m_size)
        numElements = m_size;

    // Claim a sequence of elements.
    unsigned claimEnd = m_claimed += numElements;
    // Wait until the claimed elements are free (the consumer has made enough
    // progress).
    unsigned consumerThreshold = claimEnd - m_size;
    if (int(m_consumed - consumerThreshold) < 0)
    {
        m_consumerProgress.expect(
                    m_consumed,
                    [&] { return int(m_consumed - consumerThreshold) >= 0; });
    }

    unsigned claimBegin = claimEnd - numElements;
    *reinterpret_cast<uint16_t*>(data(claimBegin)) = numElements;
    return Block(claimBegin, numElements);
}

auto RingBuffer::tryClaim(unsigned minNumElements, unsigned maxNumElements) -> Block
{
    minNumElements += 2;
    if (minNumElements > m_size)
        minNumElements = m_size;
    maxNumElements += 2;
    if (maxNumElements > m_size)
        maxNumElements = m_size;

    unsigned claimBegin = m_claimed;
    int free;
    do
    {
        // Determine the number of available elements.
        free = m_consumed - (claimBegin - m_size);
        if (free < int(minNumElements))
            return Block(0, Block::header_size);
        if (free >= int(maxNumElements))
            free = maxNumElements;
    } while (!m_claimed.compare_exchange_weak(claimBegin, claimBegin + free));

    *reinterpret_cast<uint16_t*>(data(claimBegin)) = free;
    return Block(claimBegin, free);
}

void RingBuffer::publish(const Block& block)
{
    for (;;)
    {
        // Fast path: Assume that this producer can publish its range.
        unsigned expected = block.m_begin;
        while (expected == block.m_begin)
        {
            if (m_published.compare_exchange_strong(expected, block.m_begin + block.m_length))
            {
                m_producerProgress.notify(m_published, [] {});
                return;
            }
        }

        // Try to make progress by applying a stashed range.
        if (m_stashCount == 0 || !applyStash())
            break;
    }

    // Wait until the other producers have made progress.
    m_producerProgress.expect(m_published, block.m_begin);
    // Publish the given range.
    m_producerProgress.notify(m_published, block.m_begin + block.m_length);
}

void RingBuffer::tryPublish(const Block& block)
{
    // Squeeze the range into an unsigned.
    unsigned modBegin = block.m_begin % m_size;
    unsigned compressedRange = (modBegin << (sizeof(unsigned) * 4)) + block.m_length;

    for (;;)
    {
        // Fast path: Assume that this producer can publish its range.
        unsigned expected = block.m_begin;
        while (expected == block.m_begin)
        {
            if (m_published.compare_exchange_strong(expected, block.m_begin + block.m_length))
            {
                m_producerProgress.notify(m_published, [] {});
                return;
            }
        }

        // Try to make progress by applying a stashed range.
        if (m_stashCount == 0 || !applyStash())
            break;
    }

    // Save the compressed range to the stash.
    for (auto& stashed : m_stash)
    {
        unsigned expected = 0;
        while (expected == 0)
        {
            if (stashed.compare_exchange_strong(expected, compressedRange))
            {
                ++m_stashCount;
                return;
            }
        }
    }

    // All stash slots have been taken.
    std::terminate();
}

bool RingBuffer::applyStash() noexcept
{
    static constexpr unsigned length_mask = (1 << (sizeof(unsigned) * 4)) - 1;

    bool madeProgress = false;
    for (auto& stashed : m_stash)
    {
        unsigned compressedRange = stashed;
        if (compressedRange == 0)
            continue;

        unsigned published = m_published;
        while (compressedRange >> (sizeof(unsigned) * 4) == published % m_size)
        {
            unsigned newEnd = published + (compressedRange & length_mask);
            if (m_published.compare_exchange_strong(published, newEnd))
            {
                stashed = 0;
                --m_stashCount;
                madeProgress = true;
                break;
            }
        }
    }

    if (madeProgress)
        m_producerProgress.notify(m_published, [] {});

    return madeProgress;
}



auto RingBuffer::wait() noexcept -> Block
{
    while (m_stashCount != 0 && applyStash())
    {
    }

    if (int(m_published - m_consumed) <= 0)
    {
        // Wait until the producers have made progress.
        m_producerProgress.expect(
                    m_published,
                    [&] { return int(m_published - m_consumed) > 0; });
    }
    unsigned consumeBegin = m_consumed;
    return Block(consumeBegin,
                 *reinterpret_cast<uint16_t*>(data(consumeBegin)));
}

void RingBuffer::consume(Block block) noexcept
{
    m_consumerProgress.notify(m_consumed, m_consumed + block.m_length);
}



void* RingBuffer::data(unsigned index) noexcept
{
    return static_cast<char*>(m_data) + index % m_size;
}

void RingBuffer::read(unsigned begin, void* dest, unsigned size) const noexcept
{
    begin %= m_size;
    unsigned restSize = m_size - begin;
    if (size <= restSize)
    {
        std::memcpy(dest, static_cast<const char*>(m_data) + begin, size);
    }
    else
    {
        std::memcpy(dest, static_cast<const char*>(m_data) + begin, restSize);
        std::memcpy(static_cast<char*>(dest) + restSize, m_data, size - restSize);
    }
}

void RingBuffer::write(unsigned begin, const void* source, unsigned size) noexcept
{
    begin %= m_size;
    unsigned restSize = m_size - begin;
    if (size <= restSize)
    {
        std::memcpy(static_cast<char*>(m_data) + begin, source, size);
    }
    else
    {
        std::memcpy(static_cast<char*>(m_data) + begin, source, restSize);
        std::memcpy(m_data, static_cast<const char*>(source) + restSize, size - restSize);
    }
}

auto RingBuffer::unwrap(const Block& range) const noexcept -> std::pair<Slice, Slice>
{
    unsigned begin = range.m_begin % m_size;
    unsigned restSize = m_size - begin;
    if (range.m_length <= restSize)
    {
        return std::pair<Slice, Slice>(
                    Slice(static_cast<char*>(m_data) + begin, range.m_length),
                    Slice(nullptr, 0));
    }
    else
    {
        return std::pair<Slice, Slice>(
                    Slice(static_cast<char*>(m_data) + begin, restSize),
                    Slice(m_data, range.m_length - restSize));
    }
}
