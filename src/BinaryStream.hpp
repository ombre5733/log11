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

#include "Config.hpp"
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
class BinaryStream;
class SplitStringView;

template <typename T>
struct TypeInfo;

namespace log11_detail
{
class SerdesOptions;

template <typename T>
class Serdes;



template <typename T>
struct NoTypeTagGetterDefined;

template <typename T>
struct try_get_tagid_from_typeinfo
{
    template <typename U>
    static constexpr auto test(U*)
        -> decltype(log11::TypeInfo<U>::tag(), std::true_type());

    template <typename U>
    static constexpr std::false_type test(...);

    using can_call = decltype(test<T>(0));

    template <typename U>
    static
    std::uint32_t get(can_call)
    {
        return log11::TypeInfo<T>::tag();
    }

    template <typename U>
    static
    std::uint32_t get(not_t<can_call>)
    {
        NoTypeTagGetterDefined<T> dummy;
        ((void)dummy);
        return 0;
    }
};

//! This dummy class has no implementation. It is used to signal that a
//! user-defined type lacks a proper stream output function because
//! log11::TypeInfo<T> is undefined.
template <typename T>
struct NoBinaryStreamOutputFunctionDefined;

template <typename T>
struct try_typeinfo_binarystream_write
{
    template <typename U>
    static constexpr auto test(U*)
        -> decltype(log11::TypeInfo<U>::write(std::declval<log11::BinaryStream&>(), std::declval<U&>()), std::true_type());

    template <typename U>
    static constexpr std::false_type test(...);

    using can_call = decltype(test<T>(0));

    template <typename U>
    static
    void f(log11::BinaryStream& stream, U&& value, can_call)
    {
        log11::TypeInfo<T>::write(stream, value);
    }

    template <typename U>
    static
    void f(log11::BinaryStream&, U&&, not_t<can_call>)
    {
        NoBinaryStreamOutputFunctionDefined<T> dummy;
        (void)dummy;
    }
};

} // namespace log11_detail

class BinaryStream
{
public:
    explicit
    BinaryStream(BinarySinkBase& sink, log11_detail::SerdesOptions& opt);

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
    BinaryStream& operator<<(const SplitStringView& str);

    template <typename T,
              typename = std::enable_if_t<!log11_detail::is_builtin<T>::value>>
    BinaryStream& operator<<(T&& value);

    void writeSignedEnum(std::uint32_t tag, long long value);
    void writeUnsignedEnum(std::uint32_t tag, unsigned long long value);

private:
    BinarySinkBase* m_sink;
    log11_detail::SerdesOptions& m_options;


    template <typename T>
    friend class log11_detail::Serdes;
};

template <typename T, typename>
BinaryStream& BinaryStream::operator<<(T&& value)
{
    std::uint32_t tag = log11_detail::try_get_tagid_from_typeinfo<
            std::decay_t<T>>::template get<std::decay_t<T>>(std::true_type());
    m_sink->beginStruct(tag);
    log11_detail::try_typeinfo_binarystream_write<std::decay_t<T>>::f(
        *this, std::forward<T>(value), std::true_type());
    m_sink->endStruct(tag);

    return *this;
}

} // namespace log11

#endif // LOG11_BINARYSTREAM_HPP
