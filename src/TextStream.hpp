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

#ifndef LOG11_TEXTSTREAM_HPP
#define LOG11_TEXTSTREAM_HPP

#include "Config.hpp"
#include "TextSink.hpp"
#include "Utility.hpp"

#include <cstddef>
#include <type_traits>
#include <utility>

#ifdef LOG11_USE_WEOS
#include <weos/utility.hpp>
#endif // LOG11_USE_WEOS


namespace log11
{
class SplitStringView;
class TextStream;

template <typename T>
struct TypeInfo;

namespace log11_detail
{

template <typename T>
class Serdes;



template <typename T>
struct NoTextStreamOutputFunctionDefined;

template <typename T>
struct try_textstream_shift_operator
{
    template <typename U>
    static constexpr auto test(U*)
        -> decltype(std::declval<log11::TextStream&>() << std::declval<U&>(), std::true_type());

    template <typename U>
    static constexpr std::false_type test(...);

    using can_call = decltype(test<T>(0));

    template <typename U>
    static
    void f(log11::TextStream& stream, U&& value, can_call)
    {
        stream << std::move(value);
    }

    template <typename U>
    static
    void f(log11::TextStream&, U&&, not_t<can_call>)
    {
        NoTextStreamOutputFunctionDefined<T> dummy;
        (void)dummy;
    }
};

template <typename T>
struct try_free_textstream_write
{
    template <typename U>
    static constexpr auto test(U*)
        -> decltype(write(std::declval<log11::TextStream&>(), std::declval<U&>()), std::true_type());

    template <typename U>
    static constexpr std::false_type test(...);

    using can_call = decltype(test<T>(0));

    template <typename U>
    static
    void f(log11::TextStream& stream, U&& value, can_call)
    {
        write(stream, std::move(value));
    }

    template <typename U>
    static
    void f(log11::TextStream& stream, U&& value, not_t<can_call>)
    {
        try_textstream_shift_operator<T>::f(stream, std::move(value), std::true_type());
    }
};

template <typename T>
struct try_member_textstream_write
{
    template <typename U>
    static constexpr auto test(U*)
        -> decltype(std::declval<U&>().write(std::declval<log11::TextStream&>()), std::true_type());

    template <typename U>
    static constexpr std::false_type test(...);

    using can_call = decltype(test<T>(0));

    template <typename U>
    static
    void f(log11::TextStream& stream, U&& value, can_call)
    {
        std::move(value).write(stream);
    }

    template <typename U>
    static
    void f(log11::TextStream& stream, U&& value, not_t<can_call>)
    {
        try_free_textstream_write<T>::f(stream, std::move(value), std::true_type());
    }
};



class TextForwarderSink : public TextSink
{
public:
    explicit
    TextForwarderSink(TextStream& stream);

    TextForwarderSink(const TextForwarderSink&) = delete;
    TextForwarderSink& operator=(const TextForwarderSink&) = delete;

    virtual
    void putChar(char ch) override;

    virtual
    void putString(const char* str, std::size_t size) override;

private:
    TextStream& m_stream;
    std::size_t m_numCharacters;
};

} // namespace log11_detail



class TextStream
{
public:
    explicit
    TextStream(TextSink& sink);

    TextStream(const TextStream&) = delete;
    TextStream& operator=(const TextStream&) = delete;

    //TODO: TextStream(TextStream&& rhs);
    //TODO: TextStream& operator=(TextStream&& rhs);

    TextStream& operator<<(bool value);

    TextStream& operator<<(char ch);

    TextStream& operator<<(signed char value);
    TextStream& operator<<(unsigned char value);
    TextStream& operator<<(short value);
    TextStream& operator<<(unsigned short value);
    TextStream& operator<<(int value);
    TextStream& operator<<(unsigned int value);
    TextStream& operator<<(long value);
    TextStream& operator<<(unsigned long value);
    TextStream& operator<<(long long value);
    TextStream& operator<<(unsigned long long value);

    TextStream& operator<<(float value);
    TextStream& operator<<(double value);
    TextStream& operator<<(long double value);

    TextStream& operator<<(const void* value);

    TextStream& operator<<(const char* str);
    TextStream& operator<<(const SplitStringView& str);

    template <typename... TArgs>
    TextStream& write(const char* fmt, TArgs&&... args);

    const char* parseFormatString(const char* str)
    {
        return m_format.parse(str);
    }

private:
    using max_int_type = unsigned long long;

    struct Format
    {
        enum Alignment : unsigned char
        {
            AutoAlign,
            Left,
            Right,
            Centered,
            AlignAfterSign
        };

        enum SignPolicy : unsigned char
        {
            OnlyNegative,     //!< Prints a minus sign, suppresses a plus.
            SpaceForPositive, //!< Prints a minus sign and a space instead of a plus.
            Always            //!< Prints always a sign.
        };

        enum Type : unsigned char
        {
            Default,

            Binary,
            Character,
            Decimal,
            Octal,
            Hex,

            // TODO: Float types
        };


        constexpr
        Format() noexcept
            : m_argumentIndex(0),
              width(0),
              m_precision(0),
              fill(' '),
              align(AutoAlign),
              sign(OnlyNegative),
              basePrefix(false),
              upperCase(false),
              type(Default)
        {
        }

        const char* parse(const char* str);

        int m_argumentIndex;
        int width;
        int m_precision;
        char fill;
        Alignment align;
        SignPolicy sign;
        bool basePrefix;
        bool upperCase;
        Type type;
    };

    TextSink* m_sink;

    Format m_format;

    // Additional state for printing. TO BE CHANGED
    int m_padding;



    template <typename T>
    void doPrint(T value, /* is_builtin = */ std::true_type)
    {
        *this << value;
    }

    template <typename T>
    void doPrint(T&& value, /* is_builtin = */ std::false_type)
    {
        // TODO: padding

        // TODO: If this->m_format.align != left, we have to use a buffered sink
        log11_detail::TextForwarderSink sink(*this);
        TextStream chainedStream(sink);
        log11_detail::try_member_textstream_write<T>::f(
            chainedStream, std::move(value), std::true_type());
    }

    void printArgument(unsigned index);

    template <typename TArg, typename... TArgs>
    void printArgument(unsigned index, TArg&& arg, TArgs&&... args);

    template <unsigned char TBase>
    int countDigits(max_int_type value, max_int_type& divisor);

    template <unsigned char TBase>
    void printIntegerDigits(max_int_type value, max_int_type divisor);

    void printInteger(max_int_type value, bool isNegative);

    void printPrePaddingAndSign(bool isNegative);

    void reset();



    friend class log11_detail::TextForwarderSink;

    template <typename T>
    friend class log11_detail::Serdes;
};

template <typename... TArgs>
TextStream& TextStream::write(const char* fmt, TArgs&&... args)
{
    unsigned argCounter = 0;
    const char* marker = fmt;
    for (; *fmt; ++fmt)
    {
        if (*fmt != '{')
            continue;

        if (fmt != marker)
            m_sink->putString(marker, fmt - marker);

        // Loop to the end of the format specifier (or the end of the string).
        marker = ++fmt;
        while (*fmt && *fmt != '}')
            ++fmt;
        if (!*fmt)
            return *this;

        printArgument(argCounter++, std::move(args)...);

        marker = fmt + 1;
    }
    if (fmt != marker)
        m_sink->putString(marker, fmt - marker);

    // TODO: Is this a good idea?
    while (argCounter < sizeof...(TArgs))
    {
        m_sink->putString(" <", 2);
        printArgument(argCounter++, std::move(args)...);
        m_sink->putChar('>');
    }

    return *this;
}

template <typename TArg, typename... TArgs>
void TextStream::printArgument(unsigned index, TArg&& arg, TArgs&&... args)
{
    if (index == 0)
        doPrint(std::move(arg), log11_detail::is_builtin<TArg>());
    else
        printArgument(index - 1, std::move(args)...);
}

} // namespace log11

#endif // LOG11_TEXTSTREAM_HPP
