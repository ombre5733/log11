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
class FormatTupleSerdes;
class SerdesOptions;
class TupleSerdes;

template <typename T>
class Serdes;



template <typename T>
struct NoTypeTagGetterDefined;

template <typename T>
struct try_get_typetag_from_typeinfo
{
    template <typename U>
    static constexpr auto test(U*)
        -> decltype(log11::TypeInfo<U>::typeTag(), std::true_type());

    template <typename U>
    static constexpr std::false_type test(...);

    using can_call = decltype(test<T>(nullptr));

    template <typename U>
    static
    std::uint32_t get(can_call)
    {
        return log11::TypeInfo<T>::typeTag();
    }

    template <typename U>
    static
    std::uint32_t get(Not<can_call>)
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

    using can_call = decltype(test<T>(nullptr));

    template <typename U>
    static
    void f(log11::BinaryStream& stream, U&& value, can_call)
    {
        log11::TypeInfo<T>::write(stream, std::forward<U>(value));
    }

    template <typename U>
    static
    void f(log11::BinaryStream&, U&&, Not<can_call>)
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

    //! \brief Outputs a value.
    //!
    //! Outputs the \p value and returns a reference to this stream.
    template <typename T>
    BinaryStream& operator<<(T&& value)
    {
        write(std::forward<T>(value));
        return *this;
    }


    void write(bool value);

    void write(char ch);

    void write(signed char value);
    void write(unsigned char value);
    void write(short value);
    void write(unsigned short value);
    void write(int value);
    void write(unsigned int value);
    void write(long value);
    void write(unsigned long value);
    void write(long long value);
    void write(unsigned long long value);

    void write(float value);
    void write(double value);
    void write(long double value);

    void write(const void* value);

    void write(const char* str);
    void write(Immutable<const char*> str);
    void write(const SplitStringView& str);


    //! \brief Outputs a struct.
    //!
    //! Outputs the struct \p value.
    template <typename T>
    std::enable_if_t<!log11_detail::IsBuiltin<T>::value
                     && std::is_class<std::decay_t<T>>::value>
    write(T&& value)
    {
        std::uint32_t tag = log11_detail::try_get_typetag_from_typeinfo<
                std::decay_t<T>>::template get<std::decay_t<T>>(std::true_type());
        m_sink->beginStruct(tag);
        log11_detail::try_typeinfo_binarystream_write<std::decay_t<T>>::f(
            *this, std::forward<T>(value), std::true_type());
        m_sink->endStruct(tag);
    }

    //! \brief Outputs an enum.
    //!
    //! Outputs the enum \p value.
    template <typename T>
    std::enable_if_t<!log11_detail::IsBuiltin<T>::value
                     && std::is_enum<std::decay_t<T>>::value>
    write(T&& value)
    {
        std::uint32_t tag = log11_detail::try_get_typetag_from_typeinfo<
                std::decay_t<T>>::template get<std::decay_t<T>>(std::true_type());
        m_sink->writeEnum(tag, static_cast<std::int64_t>(value));
    }

private:
    BinarySinkBase* m_sink;
    log11_detail::SerdesOptions& m_options;


    friend
    class log11_detail::FormatTupleSerdes;

    friend
    class log11_detail::TupleSerdes;
};

} // namespace log11

#endif // LOG11_BINARYSTREAM_HPP
