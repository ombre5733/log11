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

#include "ringbuffer.hpp"

#include <cstring>


using namespace log11;

RingBuffer::RingBuffer(unsigned elementSize, unsigned size)
    : m_elementSize(elementSize),
      m_totalNumElements((size + elementSize - 1) / elementSize),
      m_claimed(0),
      m_published(0),
      m_consumed(0)
{
    m_data = ::operator new(m_elementSize * m_totalNumElements);
}

RingBuffer::~RingBuffer()
{
    ::operator delete(m_data);
}

auto RingBuffer::claim(unsigned numElements) -> Range
{
    if (numElements > m_totalNumElements)
        numElements = m_totalNumElements;

    // Claim a sequence of elements.
    unsigned claimEnd = m_claimed += numElements;
    // Wait until the claimed elements are free.
    unsigned consumerThreshold = claimEnd - m_totalNumElements;
    while (int(m_consumed - consumerThreshold) < 0)
    {
        // Wait until the consumer has made progress.
        // TODO: Make this faster.
        LOG11_STD::mutex dummy;
        LOG11_STD::unique_lock<LOG11_STD::mutex> dummyLock(dummy);
        m_progressSignal.wait(dummyLock, [&] {
            return int(m_consumed - consumerThreshold) >= 0;
        });
    }

    return Range(claimEnd - numElements, numElements);
}

auto RingBuffer::tryClaim(unsigned numElements, bool allowTruncation) -> Range
{
    if (numElements > m_totalNumElements)
        numElements = m_totalNumElements;

    unsigned claimBegin = m_claimed;
    int free;
    do
    {
        // Determine the number of available elements.
        free = m_consumed - (claimBegin - m_totalNumElements);
        if (free >= int(numElements))
            free = numElements;
        else if (free <= 0 || !allowTruncation)
            return Range(0, 0);
    } while (!m_claimed.compare_exchange_weak(claimBegin, claimBegin + free));

    return Range(claimBegin, free);
}

void RingBuffer::publish(const Range& range)
{
    if (m_published != range.begin)
    {
        // Wait until the other producers have made progress.
        // TODO: Make this faster.
        LOG11_STD::mutex dummy;
        LOG11_STD::unique_lock<LOG11_STD::mutex> dummyLock(dummy);
        m_progressSignal.wait(dummyLock, [&] {
            return m_published == range.begin;
        });
    }

    m_published = range.begin + range.length;
    m_progressSignal.notify_all();
}

auto RingBuffer::available() const noexcept -> Range
{
    int free = m_published - m_consumed;
    while (free <= 0)
    {
        // Wait until a producer has made progress.
        LOG11_STD::mutex dummy;
        LOG11_STD::unique_lock<LOG11_STD::mutex> dummyLock(dummy);
        m_progressSignal.wait(dummyLock, [&] () -> bool {
            free = m_published - m_consumed;
            return free > 0;
        });
    }
    return Range(m_consumed, free);
}

void RingBuffer::consume(unsigned numEntries) noexcept
{
    m_consumed += numEntries;
    m_progressSignal.notify_all();
}


void* RingBuffer::operator[](unsigned index) noexcept
{
    return static_cast<char*>(m_data) + (index % m_totalNumElements) * m_elementSize;
}


auto RingBuffer::byteRange(const Range& range) const noexcept -> ByteRange
{
    unsigned begin = (range.begin % m_totalNumElements) * m_elementSize;
    return ByteRange(begin, range.length * m_elementSize);
}

auto RingBuffer::read(const ByteRange& range, void* dest, unsigned size) const -> ByteRange
{
    if (size > range.length)
        size = range.length;
    if (!size)
        return range;

    unsigned begin = range.begin % (m_totalNumElements * m_elementSize);
    unsigned restSize = m_totalNumElements * m_elementSize - begin;
    if (size <= restSize)
    {
        std::memcpy(dest, static_cast<const char*>(m_data) + begin, size);
    }
    else
    {
        std::memcpy(dest, static_cast<const char*>(m_data) + begin, restSize);
        std::memcpy(static_cast<char*>(dest) + restSize, m_data, size - restSize);
    }

    return ByteRange(range.begin + size, range.length - size);
}

auto RingBuffer::write(const void* source, const ByteRange& range, unsigned size) -> ByteRange
{
    if (size > range.length)
        size = range.length;
    if (!size)
        return range;

    unsigned begin = range.begin % (m_totalNumElements * m_elementSize);
    unsigned restSize = m_totalNumElements * m_elementSize - begin;
    if (size <= restSize)
    {
        std::memcpy(static_cast<char*>(m_data) + begin, source, size);
    }
    else
    {
        std::memcpy(static_cast<char*>(m_data) + begin, source, restSize);
        std::memcpy(m_data, static_cast<const char*>(source) + restSize, size - restSize);
    }

    return ByteRange(begin + size, range.length - size);
}
