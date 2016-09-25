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

#ifndef LOG11_SERDES_HPP
#define LOG11_SERDES_HPP

#include "RingBuffer.hpp"
#include "TextStream.hpp"

#include <cstddef>
#include <cstdint>
#include <type_traits>


namespace log11
{
namespace log11_detail
{

class SerdesBase
{
public:
    virtual
    ~SerdesBase();

    //virtual
    //RingBuffer::Block skip(RingBuffer& buffer,
    //                       RingBuffer::Block range) const noexcept = 0;

    virtual
    bool deserialize(RingBuffer::Stream& inStream,
                     TextStream& outStream) const noexcept = 0;

    //virtual
    //void deserialize(RingBuffer& buffer, RingBuffer::Range range, BinaryStream& stream) const noexcept = 0;
};

template <typename T>
class Serdes : public SerdesBase
{
public:
    Serdes(const Serdes&) = delete;
    Serdes& operator=(const Serdes&) = delete;

    static
    Serdes* instance()
    {
        static Serdes serdes;
        return &serdes;
    }

    static
    std::size_t requiredSize(const T&) noexcept
    {
        return sizeof(T);
    }

    static
    bool serialize(RingBuffer::Stream& stream, const T& value) noexcept
    {
        return stream.write(&value, sizeof(T));
    }

    //virtual
    //RingBuffer::Block skip(RingBuffer&, RingBuffer::Block range) const noexcept override
    //{
    //    // TODO: Bounds check
    //    if (range.m_length >= sizeof(T))
    //    {
    //        range.m_begin += sizeof(T);
    //        range.m_length -= sizeof(T);
    //    }
    //    else
    //    {
    //        range.m_length = 0;
    //    }
    //    return range;
    //}

    virtual
    bool deserialize(RingBuffer::Stream& inStream,
                     TextStream& outStream) const noexcept override
    {
        T temp;
        if (inStream.read(&temp, sizeof(T)))
        {
            outStream.doPrint(std::move(temp), log11_detail::is_custom<T>());
            return true;
        }
        else
        {
            return false;
        }
    }

    //virtual
    //void deserialize(RingBuffer& buffer, RingBuffer::Range range,
    //                 BinaryStream& stream) const noexcept override
    //{
    //    if (range.length >= sizeof(T))
    //    {
    //        T temp;
    //        buffer.read(range, &temp, sizeof(T));
    //        // TODO
    //    }
    //    // TODO: return new range?
    //}

private:
    Serdes() = default;
};

} // namespace log11_detail
} // namespace log11

#endif // LOG11_SERDES_HPP
