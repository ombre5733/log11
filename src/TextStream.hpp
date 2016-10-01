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
#include "RingBuffer.hpp"
#include "TextSink.hpp"
#include "Utility.hpp"

#include <cstddef>
#include <cstring>
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
struct try_free_textstream_format
{
    template <typename U>
    static constexpr auto test(U*)
        -> decltype(format(std::declval<log11::TextStream&>(), std::declval<U&>()), std::true_type());

    template <typename U>
    static constexpr std::false_type test(...);

    using can_call = decltype(test<T>(nullptr));

    template <typename U>
    static
    void f(log11::TextStream& stream, U&& value, can_call)
    {
        format(stream, std::forward<U>(value));
    }

    template <typename U>
    static
    void f(log11::TextStream&, U&&, Not<can_call>)
    {
        NoTextStreamOutputFunctionDefined<T> dummy;
        (void)dummy;
    }
};

template <typename T>
struct try_member_textstream_format
{
    template <typename U>
    static constexpr auto test(U*)
        -> decltype(std::declval<U&>().format(std::declval<log11::TextStream&>()), std::true_type());

    template <typename U>
    static constexpr std::false_type test(...);

    using can_call = decltype(test<T>(nullptr));

    template <typename U>
    static
    void f(log11::TextStream& stream, U&& value, can_call)
    {
        std::forward<U>(value).format(stream);
    }

    template <typename U>
    static
    void f(log11::TextStream& stream, U&& value, Not<can_call>)
    {
        try_free_textstream_format<T>::f(stream, std::forward<U>(value), std::true_type());
    }
};

template <typename T>
struct try_typeinfo_textstream_format
{
    template <typename U>
    static constexpr auto test(U*)
        -> decltype(log11::TypeInfo<U>::format(std::declval<log11::TextStream&>(), std::declval<U&>()), std::true_type());

    template <typename U>
    static constexpr std::false_type test(...);

    using can_call = decltype(test<T>(nullptr));

    template <typename U>
    static
    void f(log11::TextStream& stream, U&& value, can_call)
    {
        log11::TypeInfo<T>::format(stream, std::forward<U>(value));
    }

    template <typename U>
    static
    void f(log11::TextStream& stream, U&& value, Not<can_call>)
    {
        try_member_textstream_format<T>::f(stream, std::forward<U>(value), std::true_type());
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
    void writeChar(char ch) override;

    virtual
    void writeString(const char* str, std::size_t size) override;

private:
    TextStream& m_stream;
    std::size_t m_numCharacters;
};



template <typename...  T>
struct ArgumentForwarder
{
    template <typename... U>
    explicit
    ArgumentForwarder(TextStream& outStream, U&&... args) noexcept
        : m_index(0),
          m_outStream(outStream),
          m_args(std::forward<U>(args)...)
    {
    }

    void printNext()
    {
        doPrintNext<0>(std::integral_constant<bool, 0 < sizeof...(T)>());
        ++m_index;
    }

    void printRest()
    {
        while (m_index < sizeof...(T))
        {
            doPrintNext<0>(std::integral_constant<bool, 0 < sizeof...(T)>());
            ++m_index;
        }
    }

private:
    unsigned m_index;
    TextStream& m_outStream;
    std::tuple<T...> m_args;

    template <unsigned I>
    void doPrintNext(std::integral_constant<bool, true>)
    {
        if (m_index == I)
            m_outStream << std::get<I>(m_args);
        else
            doPrintNext<I+1>(std::integral_constant<bool, I+1 < sizeof...(T)>());
    }

    template <unsigned I>
    void doPrintNext(std::integral_constant<bool, false>)
    {
    }
};

template <>
struct ArgumentForwarder<RingBuffer::Stream>
{
    explicit
    ArgumentForwarder(TextStream& outStream, RingBuffer::Stream& inStream)
        : m_inStream(inStream),
          m_outStream(outStream)
    {
    }

    void printNext();
    void printRest();

private:
    RingBuffer::Stream& m_inStream;
    TextStream& m_outStream;
};

} // namespace log11_detail



class TextStream
{
public:
    explicit
    TextStream(TextSink& sink, log11_detail::ScratchPad& scratchPad);

    TextStream(const TextStream&) = delete;
    TextStream& operator=(const TextStream&) = delete;

    //! \brief Formats in a printf-like manner.
    //! Prints the given \p fmt string, with interpolated \p args.
    template <typename... TArgs>
    TextStream& format(const char* fmt, TArgs&&... args);

    //! \brief Outputs a value.
    //!
    //! Prints the given \p value and returns this stream.
    template <typename T>
    TextStream& operator<<(T&& value)
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

    template <typename T,
              typename = std::enable_if_t<!log11_detail::IsBuiltin<T>::value>>
    void write(T&& value);


    template <typename... TArgs>
    void doFormat(SplitStringView str,
                  log11_detail::ArgumentForwarder<TArgs...>&& args);

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
            NoType,

            Binary,
            Character,
            Decimal,
            Octal,
            Hex,

            Exponent,
            FixedPoint,
            GeneralFloat
        };


        constexpr
        Format() noexcept
            : m_argumentIndex(0),
              minWidth(0),
              precision(-1),
              fill(' '),
              align(AutoAlign),
              sign(OnlyNegative),
              alternateForm(false),
              upperCase(false),
              type(NoType)
        {
        }

        const char* parse(const char* str);

        short int m_argumentIndex;
        short int minWidth;
        short int precision;
        char fill;
        Alignment align;
        SignPolicy sign;
        bool alternateForm;
        bool upperCase;
        Type type;
    };

    TextSink* m_sink;
    log11_detail::ScratchPad& m_scratchPad;

    Format m_format;



    template <unsigned char TBase>
    int countDigits(max_int_type value, max_int_type& divisor);

    template <unsigned char TBase>
    void printIntegerDigits(max_int_type value, max_int_type divisor);

    void printInteger(max_int_type value, bool isNegative);
    void printFloat(double value);

    int printPrePaddingAndSign(int padding, bool isNegative, Format::Type prefix);
    void printPostPadding(int padding);

    void reset();



    friend class log11_detail::TextForwarderSink;

    template <typename T>
    friend class log11_detail::Serdes;
};

template <typename T, typename>
void TextStream::write(T&& value)
{
    // TODO: padding
    // TODO: If this->m_format.align != left, we have to use a buffered sink

    log11_detail::TextForwarderSink sink(*this);
    TextStream chainedStream(sink, m_scratchPad);
    log11_detail::try_typeinfo_textstream_format<std::decay_t<T>>::f(
        chainedStream, std::forward<T>(value), std::true_type());
}

template <typename... TArgs>
TextStream& TextStream::format(const char* fmt, TArgs&&... args)
{
    using namespace std;

    doFormat(SplitStringView{fmt, strlen(fmt), nullptr, 0},
             log11_detail::ArgumentForwarder<TArgs&&...>(
                 *this, std::forward<TArgs>(args)...));
    return *this;
}

template <typename... TArgs>
void TextStream::doFormat(SplitStringView str,
                          log11_detail::ArgumentForwarder<TArgs...>&& args)
{
    using namespace std;

    const char* iter = str.begin1;
    const char* marker = str.begin1;
    const char* end = str.begin1 + str.length1;
    for (;; ++iter)
    {
        // Loop to the start of the format specifier (or the end of the string).
        if (iter == end)
        {
            if (iter != marker)
                m_sink->writeString(marker, iter - marker);
            marker = iter = str.begin2;
            end = str.begin2 + str.length2;
            str.length2 = 0;
            if (iter == end)
                break;
        }
        if (*iter != '{')
            continue;

        if (iter != marker)
            m_sink->writeString(marker, iter - marker);

        m_scratchPad.clear();
        // Loop to the end of the format specifier (or the end of the string).
        marker = iter + 1;
        while (*iter != '}')
        {
            ++iter;
            if (iter == end)
            {
                if (iter != marker)
                    m_scratchPad.push(marker, iter - marker);
                marker = iter = str.begin2;
                end = str.begin2 + str.length2;
                str.length2 = 0;
                if (iter == end)
                    break;
            }
        }
        if (iter == end)
            break;

        if (iter != marker)
            m_scratchPad.push(marker, iter - marker);

        if (m_scratchPad.size())
        {
            m_scratchPad.push('\0');
            m_format.parse(m_scratchPad.data());
        }

        args.printNext();

        marker = iter + 1;
    }
    if (iter != marker)
        m_sink->writeString(marker, iter - marker);

    args.printRest();
}

} // namespace log11

#endif // LOG11_TEXTSTREAM_HPP
