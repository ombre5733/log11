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

#include "_config.hpp"

#include <atomic>
#include <condition_variable>
#include <utility>

#if defined(LOG11_USE_WEOS) && !defined(FREM_GEN_RUN)
#include <weos/synchronic.hpp>
#else
#include "_synchronic.hpp"
#endif // LOG11_USE_WEOS

namespace log11
{

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

    struct Slice
    {
        Slice(void* p, unsigned l)
            : pointer(p), length(l)
        {
        }

        void* pointer;
        unsigned length;
    };



    //! Creates a ring buffer whose size is <tt>2^exponent</tt>.
    explicit
    RingBuffer(unsigned exponent);

    //! Destroys the ring buffer.
    ~RingBuffer();

    RingBuffer(const RingBuffer&) = delete;
    RingBuffer& operator=(const RingBuffer&) = delete;

    // Producer interface

    //! Claims \p numElements elements from the ring buffer. The caller is
    //! blocked until the elements are free.
    Range claim(unsigned numElements);

    //! Tries to claim between \p minNumElements and \p maxNumElements (both
    //! sides inclusive) elements from the buffer. If less than
    //! \p minNumElements elements are available, an empty range is returned.
    Range tryClaim(unsigned minNumElements, unsigned maxNumElements);

    //! Publishes the elements specified by the \p range. The range must have
    //! been claimed before publishing. The caller will be blocked until
    //! all prior producers have published their slots.
    void publish(const Range& range);

    //! Tries to publish the elements specified by the \p range. If the ranges
    //! are published out of order, they will be stashed.
    void tryPublish(const Range& range);

    // Consumer interface

    //! Waits until there is a range available for consumption.
    //! Returns the range of elements which can be consumed.
    Range wait() noexcept;

    //! Consumes \p numElements elements.
    void consume(unsigned numElements) noexcept;

    // Data access

    //! Returns a pointer to the \p index-th slot.
    void* operator[](unsigned index) noexcept;

    ByteRange byteRange(const Range& range) const noexcept;

    ByteRange read(const ByteRange& range, void* dest, unsigned size) const;

    ByteRange write(const void* source, const ByteRange& range, unsigned size);

    std::pair<Slice, Slice> unwrap(const ByteRange& range) const noexcept;

private:
    //! The ring buffer's data.
    void* m_data;
    //! The size of the ring buffer.
    unsigned m_size;

    //! Points past the last claimed slot.
    std::atomic<unsigned> m_claimed;
    //! Points past the last published slot.
    std::atomic<unsigned> m_published;
    //! Points past the last consumed slot.
    std::atomic<unsigned> m_consumed;

    //! Stashed ranges planned to be published.
    std::atomic<unsigned> m_stash[8];
    //! The number of stashed ranges.
    std::atomic<unsigned> m_stashCount;

#if defined(LOG11_USE_WEOS) && !defined(FREM_GEN_RUN)
    using synchronic = weos::synchronic<unsigned>;
#else
    using synchronic = log11_detail::synchronic<unsigned>;
#endif

    mutable synchronic m_consumerProgress;
    mutable synchronic m_producerProgress;


    bool applyStash() noexcept;
};

} // namespace log11

#endif // LOG11_RINGBUFFER_HPP
