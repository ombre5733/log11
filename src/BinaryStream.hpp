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

#ifndef LOG11_BINARYSTREAM_HPP
#define LOG11_BINARYSTREAM_HPP

#include "_config.hpp"
#include "BinarySink.hpp"
#include "Utility.hpp"

#include <cstdint>
#include <type_traits>
#include <utility>

#ifdef LOG11_USE_WEOS
#include <weos/utility.hpp>
#endif // LOG11_USE_WEOS


namespace log11
{
namespace log11_detail
{

template <typename T>
class Serdes;

} // namespace log11_detail

class BinaryStream
{
public:
    explicit
    BinaryStream(BinarySinkBase& sink);

    BinaryStream(const BinaryStream&) = delete;
    BinaryStream& operator=(const BinaryStream&) = delete;

    //TODO: BinaryStream(BinaryStream&& rhs);
    //TODO: BinaryStream& operator=(BinaryStream&& rhs);

    BinaryStream& operator<<(bool value);

    BinaryStream& operator<<(char ch);

    BinaryStream& operator<<(signed char value);
    BinaryStream& operator<<(unsigned char value);
    BinaryStream& operator<<(short value);
    BinaryStream& operator<<(unsigned short value);
    BinaryStream& operator<<(int value);
    BinaryStream& operator<<(unsigned int value);
    BinaryStream& operator<<(long value);
    BinaryStream& operator<<(unsigned long value);
    BinaryStream& operator<<(long long value);
    BinaryStream& operator<<(unsigned long long value);

    BinaryStream& operator<<(float value);
    BinaryStream& operator<<(double value);
    BinaryStream& operator<<(long double value);

    BinaryStream& operator<<(const void* value);

    BinaryStream& operator<<(const char* str);
    BinaryStream& operator<<(Immutable<const char*> str);
    BinaryStream& operator<<(const log11_detail::SplitString& str);

    void beginStruct(std::uint32_t id);
    void endStruct(std::uint32_t id);

    void writeSignedEnum(std::uint32_t id, long long value);
    void writeUnsignedEnum(std::uint32_t id, unsigned long long value);

private:
    BinarySinkBase* m_sink;


    template <typename T>
    void doWrite(T value, std::false_type)
    {
        *this << value;
    }

    template <typename T>
    void doWrite(T&& value, std::true_type)
    {
        // TODO
        //log11_detail::TextForwarderSink sink(*this);
        //TextStream chainedStream(sink);
        //log11_detail::selectCustomWrite(
        //            chainedStream, std::move(value),
        //            typename log11_detail::has_member_write<T>::type(),
        //            typename log11_detail::has_free_write<T>::type(),
        //            typename log11_detail::has_binarystream_shift_operator<T>::type());
    }


    template <typename T>
    friend class log11_detail::Serdes;
};

} // namespace log11

#endif // LOG11_BINARYSTREAM_HPP
