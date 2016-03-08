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

#ifndef LOG11_RINGBUFFER_HPP
#define LOG11_RINGBUFFER_HPP

#include <atomic>
#include <condition_variable>


class RingBuffer
{
public:
    struct Range
    {
        Range(unsigned b, unsigned l) noexcept
            : begin(b), length(l)
        {
        }

        unsigned begin;
        unsigned length;
    };

    struct ByteRange
    {
        ByteRange(unsigned b, unsigned l)
            : begin(b), length(l)
        {
        }

        unsigned begin;
        unsigned length;
    };

    //! Creates a ring buffer whose slots are of size \p elementSize. The
    //! total amount of memory occupied by the ring buffer is \p size (rounded
    //! up to the next multiple of \p elementSize).
    RingBuffer(unsigned elementSize, unsigned size);

    //! Destroys the ring buffer.
    ~RingBuffer();

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    //! Claims \p numElements slots from the ring buffer. The caller is
    //! blocked until the slots are free.
    Range claim(unsigned numElements);

    //! Tries to claim \p numElements slots from the ring buffer without
    //! blocking the caller. If the buffer is full, an empty range is
    //! returned. When the number of available elements is less than
    //! \p numElements, the behaviour depends on \p allowTruncation: If
    //! the flag is set, a range smaller than \p numElements may be returned
    //! otherwise, an empty range is returned.
    Range tryClaim(unsigned numElements, bool allowTruncation);

    //! Publishes the slots specified by the \p range. The range must have
    //! been claimed before publishing. The caller will be blocked until
    //! all prior producers have published their slots.
    void publish(const Range& range);

    //! Returns the range of slots which can be consumed.
    Range available() const noexcept;

    //! Consumes \p numEntries slots.
    void consume(unsigned numEntries) noexcept;


    //! Returns a pointer to the \p index-th slot.
    void* operator[](unsigned index) noexcept;

    ByteRange byteRange(const Range& range) const noexcept;

    ByteRange read(const ByteRange& range, void* dest, unsigned size) const;

    ByteRange write(const void* source, const ByteRange& range, unsigned size);

private:
    //! The ring buffer's data.
    void* m_data;
    //! The size of one slot in the buffer.
    unsigned m_elementSize;
    //! The total number of elements in the buffer.
    unsigned m_totalNumElements;

    //! Points past the last claimed slot.
    std::atomic<unsigned> m_claimed;
    //! Points past the last published slot.
    std::atomic<unsigned> m_published;
    //! Points past the last consumed slot.
    std::atomic<unsigned> m_consumed;

    //! A signal which indicates progress in the producers or consumer.
    mutable std::condition_variable m_progressSignal;
};

#endif // LOG11_RINGBUFFER_HPP
