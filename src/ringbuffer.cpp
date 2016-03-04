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

#include <chrono>
#include <thread>


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
    // TODO: limit numElements to m_size

    // Claim a sequence of elements.
    unsigned claimEnd = m_claimed += numElements;
    // Check if the claimed elements have been consumed already.
    unsigned consumerThreshold = claimEnd - m_totalNumElements;
    while (int(consumerThreshold - m_consumed) > 0)
    {
        // Wait until the consumer has made progress.
        // TODO: Implement a better strategy.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    return Range(claimEnd - numElements, numElements);
}

auto RingBuffer::tryClaim(unsigned numElements, bool allowTruncation) -> Range
{
    // TODO: limit numElements to m_size

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
        // TODO: Implement a better strategy.
        std::this_thread::sleep_for(std::chrono::milliseconds(1));
    }

    m_published = range.begin + range.length;
}

auto RingBuffer::available() const -> Range
{
    unsigned consumeStart = m_consumed;
    int free = m_published - m_consumed;
    return Range(consumeStart, free > 0 ? free : 0);
}

void RingBuffer::consumeTo(unsigned index)
{
    m_consumed = index + 1;
}

void* RingBuffer::operator[](unsigned index)
{
    return static_cast<char*>(m_data) + (index % m_totalNumElements) * m_elementSize;
}
