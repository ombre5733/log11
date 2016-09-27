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
#include "String.hpp"

#include <atomic>
#include <condition_variable>
#include <utility>

#ifdef LOG11_USE_WEOS
#include <weos/synchronic.hpp>
#else
#include "_synchronic.hpp"
#endif // LOG11_USE_WEOS

namespace log11
{

class RingBuffer
{
public:
    class Stream
    {
    public:
        Stream(RingBuffer& buffer, unsigned begin, unsigned length) noexcept;

        bool read(void* dest, unsigned size) noexcept;

        bool write(const void* source, unsigned size) noexcept;

        bool readString(log11_detail::SplitString& view, unsigned size) noexcept;

        bool writeString(const void* source, unsigned size) noexcept;

    private:
        RingBuffer& m_buffer;
        unsigned m_begin;
        unsigned m_length;
    };

    class Block
    {
        static constexpr unsigned header_size = 2;

    public:
        Block() = default;

        constexpr
        Block(unsigned b, unsigned l) noexcept
            : m_begin(b),
              m_length(l)
        {
        }

        constexpr
        unsigned length() const noexcept
        {
            return m_length - header_size;
        }

        Stream stream(RingBuffer& buffer) noexcept;

    private:
        unsigned m_begin;
        unsigned m_length;

        friend class RingBuffer;
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
    Block claim(unsigned numElements);

    //! Tries to claim between \p minNumElements and \p maxNumElements (both
    //! sides inclusive) elements from the buffer. If less than
    //! \p minNumElements elements are available, an empty block is returned.
    Block tryClaim(unsigned minNumElements, unsigned maxNumElements);

    //! Publishes the \p block of elements. The block must have
    //! been claimed before publishing. The caller will be blocked until
    //! all prior producers have published their slots.
    void publish(const Block& block);

    //! Tries to publish a \p block of elements. If blocks are published
    //! out of order, they will be stashed.
    void tryPublish(const Block& block);

    // Consumer interface

    //! Waits until there is a range available for consumption.
    //! Returns the range of elements which can be consumed.
    Block wait() noexcept;

    //! Consumes the \p block of elements.
    void consume(Block block) noexcept;

    // Data access

    //! Returns a pointer to the \p index-th element.
    void* data(unsigned index) noexcept;

    void read(unsigned begin, void* dest, unsigned size) const noexcept;

    void write(unsigned begin, const void* source, unsigned size) noexcept;

    void unwrap(unsigned begin, log11_detail::SplitString& view, unsigned size) const noexcept;

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
