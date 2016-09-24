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

#include <cstddef>
#include <type_traits>
#include <utility>

#ifdef LOG11_USE_WEOS
#include <weos/utility.hpp>
#endif // LOG11_USE_WEOS


namespace log11
{

class TextStream;

namespace log11_detail
{

template <typename T>
class Serdes;



template <typename... T>
struct TypeList {};



template <typename T, typename U>
struct is_member;

template <typename T, typename TH, typename... TL>
struct is_member<T, TypeList<TH, TL...>>
        : std::conditional<std::is_same<T, TH>::value,
                           std::true_type,
                           is_member<T, TypeList<TL...>>>::type
{
};

template <typename T>
struct is_member<T, TypeList<>> : std::false_type {};



using builtin_types = TypeList<bool,
                               char,
                               signed char,
                               unsigned char,
                               short,
                               unsigned short,
                               int,
                               unsigned int,
                               long,
                               unsigned long,
                               long long,
                               unsigned long long,
                               float,
                               double,
                               long double//,
                               >;//StringRef>;

template <typename T>
struct is_custom : std::integral_constant<
                       bool,
                       !is_member<std::decay_t<T>, builtin_types>::value
                       && !std::is_pointer<T>::value>
{
};



template <typename T>
struct has_member_print
{
    template <typename U>
    static constexpr auto test(U*) -> decltype(std::declval<U&>().print(std::declval<log11::TextStream&>()), std::true_type());

    template <typename U>
    static constexpr std::false_type test(...);

    using type = decltype(test<T>(0));
};

template <typename T>
struct has_free_print : std::false_type
{
    template <typename U>
    static constexpr auto test(U*) -> decltype(print(std::declval<log11::TextStream&>(), std::declval<U&>()), std::true_type());

    template <typename U>
    static constexpr std::false_type test(...);

    using type = decltype(test<T>(0));
};

template <typename T>
struct has_shift_operator : std::false_type
{
    template <typename U>
    static constexpr auto test(U*) -> decltype(std::declval<log11::TextStream&>() << std::declval<U&>(), std::true_type());

    template <typename U>
    static constexpr std::false_type test(...);

    using type = decltype(test<T>(0));
};

template <typename T>
struct NoPrintFunctionDefined;



template <typename T, typename U, typename V>
void selectCustomPrint(log11::TextStream& stream, T&& value,
                       std::true_type, U, V)
{
    std::move(value).print(stream);
}

template <typename T, typename U>
void selectCustomPrint(log11::TextStream& stream, T&& value,
                       std::false_type, std::true_type, U)
{
    print(stream, std::move(value));
}

template <typename T>
void selectCustomPrint(log11::TextStream& stream, T&& value,
                       std::false_type, std::false_type, std::true_type)
{
    stream << std::move(value);
}

template <typename T>
void selectCustomPrint(log11::TextStream& stream, T&&,
                       std::false_type, std::false_type, std::false_type)
{
    log11_detail::NoPrintFunctionDefined<T> dummy;
    (void)dummy;
}



class TextStreamSink
{
public:
    virtual
    void putChar(char ch) = 0;

    virtual
    void putString(const char* str, std::size_t size) = 0;
};

class TextForwarderSink : public TextStreamSink
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
    TextStream(log11_detail::TextStreamSink& sink);

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

    TextStream& operator<<(const char* str);

    TextStream& operator<<(const void* value);

    template <typename... TArgs>
    TextStream& print(const char* fmt, TArgs&&... args);

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

    log11_detail::TextStreamSink* m_sink;

    Format m_format;

    // Additional state for printing. TO BE CHANGED
    int m_padding;



    template <typename T>
    void doPrint(T value, std::false_type)
    {
        *this << value;
    }

    template <typename T>
    void doPrint(T&& value, std::true_type)
    {
        // TODO: padding

        // TODO: If this->m_format.align != left, we have to use a buffered sink
        log11_detail::TextForwarderSink sink(*this);
        TextStream chainedStream(sink);
        log11_detail::selectCustomPrint(
                    chainedStream, std::move(value),
                    typename log11_detail::has_member_print<T>::type(),
                    typename log11_detail::has_free_print<T>::type(),
                    typename log11_detail::has_shift_operator<T>::type());
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
    friend class Serdes;
};

template <typename... TArgs>
TextStream& TextStream::print(const char* fmt, TArgs&&... args)
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
        doPrint(std::move(arg), log11_detail::is_custom<TArg>());
    else
        printArgument(index - 1, std::move(args)...);
}

} // namespace log11

#endif // LOG11_TEXTSTREAM_HPP
