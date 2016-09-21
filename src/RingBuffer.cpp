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

#include <cstring>


using namespace log11;

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

auto RingBuffer::claim(unsigned numElements) -> Range
{
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

    return Range(claimEnd - numElements, numElements);
}

auto RingBuffer::tryClaim(unsigned minNumElements, unsigned maxNumElements) -> Range
{
    if (minNumElements > m_size)
        minNumElements = m_size;
    if (maxNumElements > m_size)
        maxNumElements = m_size;

    unsigned claimBegin = m_claimed;
    int free;
    do
    {
        // Determine the number of available elements.
        free = m_consumed - (claimBegin - m_size);
        if (free < int(minNumElements))
            return Range(0, 0);
        if (free >= int(maxNumElements))
            free = maxNumElements;
    } while (!m_claimed.compare_exchange_weak(claimBegin, claimBegin + free));

    return Range(claimBegin, free);
}

void RingBuffer::publish(const Range& range)
{
    for (;;)
    {
        // Fast path: Assume that this producer can publish its range.
        unsigned expected = range.begin;
        while (expected == range.begin)
        {
            if (m_published.compare_exchange_strong(expected, range.begin + range.length))
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
    m_producerProgress.expect(m_published, range.begin);
    // Publish the given range.
    m_producerProgress.notify(m_published, range.begin + range.length);
}

void RingBuffer::tryPublish(const Range& range)
{
    // Squeeze the range into an unsigned.
    unsigned modBegin = range.begin % m_size;
    unsigned compressedRange = (modBegin << (sizeof(unsigned) * 4)) + range.length;

    for (;;)
    {
        // Fast path: Assume that this producer can publish its range.
        unsigned expected = range.begin;
        while (expected == range.begin)
        {
            if (m_published.compare_exchange_strong(expected, range.begin + range.length))
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



auto RingBuffer::wait() noexcept -> Range
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
    return Range(m_consumed, m_published - m_consumed);
}

void RingBuffer::consume(unsigned numElements) noexcept
{
    m_consumerProgress.notify(m_consumed, m_consumed + numElements);
}



void* RingBuffer::operator[](unsigned index) noexcept
{
    return static_cast<char*>(m_data) + index % m_size;
}

auto RingBuffer::read(const Range& range, void* dest, unsigned size) const -> Range
{
    if (size > range.length)
        size = range.length;
    if (!size)
        return range;

    unsigned begin = range.begin % m_size;
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

    return Range(range.begin + size, range.length - size);
}

auto RingBuffer::write(const void* source, const Range& range, unsigned size) -> Range
{
    if (size > range.length)
        size = range.length;
    if (!size)
        return range;

    unsigned begin = range.begin % m_size;
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

    return Range(begin + size, range.length - size);
}

auto RingBuffer::unwrap(const Range& range) const noexcept -> std::pair<Slice, Slice>
{
    unsigned begin = range.begin % m_size;
    unsigned restSize = m_size - begin;
    if (range.length <= restSize)
    {
        return std::pair<Slice, Slice>(
                    Slice(static_cast<char*>(m_data) + begin, range.length),
                    Slice(nullptr, 0));
    }
    else
    {
        return std::pair<Slice, Slice>(
                    Slice(static_cast<char*>(m_data) + begin, restSize),
                    Slice(m_data, range.length - restSize));
    }
}
