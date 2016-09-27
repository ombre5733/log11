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

#include "TextStream.hpp"
#include "String.hpp"

#include <cstdint>
#include <cstring>

using namespace std;


namespace log11
{

// ----=====================================================================----
//     TextForwarderSink
// ----=====================================================================----

namespace log11_detail
{

TextForwarderSink::TextForwarderSink(TextStream& printer)
    : m_stream(printer),
      m_numCharacters(0)
{
}

void TextForwarderSink::putChar(char ch)
{
    m_stream.m_sink->putChar(ch);
    ++m_numCharacters;
}

void TextForwarderSink::putString(const char* str, std::size_t size)
{
    m_stream.m_sink->putString(str, size);
    m_numCharacters += size;
}

} // namespace log11_detail

// ----=====================================================================----
//     TextStream::Format
// ----=====================================================================----

const char* TextStream::Format::parse(const char* str)
{
    // // argument index
    // while (*str >= '0' && *str <= '9')
    //     m_argumentIndex = 10 * m_argumentIndex + (*str++ - '0');
    //
    // // argument separator
    // if (*str == ':')
    //     ++str;

    if (*str == 0)
        return str;
    switch (str[1])
    {
    case '<':
    case '>':
    case '^':
    case '=':
        fill = *str++;
    }

    // align
    switch (*str)
    {
    case '<': align = Left; ++str; break;
    case '>': align = Right; ++str; break;
    case '^': align = Centered; ++str; break;
    case '=': align = AlignAfterSign; ++str; break;
    }

    // sign
    switch (*str)
    {
    case '+': sign = Always; ++str; break;
    case '-': sign = OnlyNegative; ++str; break;
    case ' ': sign = SpaceForPositive; ++str; break;
    }

    // base prefix
    if (*str == '#')
    {
        basePrefix = true;
        ++str;
    }

    if (*str == '0')
    {
        ++str;
        fill = '0';
        align = AlignAfterSign;
    }

    // width
    while (*str >= '0' && *str <= '9')
        width = 10 * width + (*str++ - '0');

    // precision
    if (*str == '.')
    {
        ++str;
        while (*str >= '0' && *str <= '9')
            m_precision = 10 * m_precision + (*str++ - '0');
    }

    // type
    switch (*str)
    {
    case 'b': type = Binary; break;
    case 'c': type = Character; break;
    case 'd': type = Decimal; break;
    case 'o': type = Octal; break;
    case 'x': type = Hex; break;
    case 'X': type = Hex; upperCase = true; break;

        // TODO: float formats
    //case 'f':
    //case 'F':
    //case 'g':
    //case 'G':
    }

    return str;
}

// ----=====================================================================----
//     TextStream
// ----=====================================================================----

TextStream::TextStream(TextSink& sink)
    : m_sink(&sink)
{
}

// -----------------------------------------------------------------------------
//     Bool & char printing
// -----------------------------------------------------------------------------

TextStream& TextStream::operator<<(bool value)
{
    // TODO: padding

    if (value)
        m_sink->putString("true", 4);
    else
        m_sink->putString("false", 5);
    reset();
    return *this;
}

TextStream& TextStream::operator<<(char ch)
{
    // TODO: padding

    m_sink->putChar(ch);
    reset();
    return *this;
}

// -----------------------------------------------------------------------------
//     Integer printing
// -----------------------------------------------------------------------------

TextStream& TextStream::operator<<(signed char value)
{
    if (value >= 0)
        printInteger(value, false);
    else
        printInteger(-static_cast<max_int_type>(value), true);
    reset();
    return *this;
}

TextStream& TextStream::operator<<(unsigned char value)
{
    printInteger(value, false);
    reset();
    return *this;
}

TextStream& TextStream::operator<<(short value)
{
    if (value >= 0)
        printInteger(value, false);
    else
        printInteger(-static_cast<max_int_type>(value), true);
    reset();
    return *this;
}

TextStream& TextStream::operator<<(unsigned short value)
{
    printInteger(value, false);
    reset();
    return *this;
}

TextStream& TextStream::operator<<(int value)
{
    if (value >= 0)
        printInteger(value, false);
    else
        printInteger(-static_cast<max_int_type>(value), true);
    reset();
    return *this;
}

TextStream& TextStream::operator<<(unsigned int value)
{
    printInteger(value, false);
    reset();
    return *this;
}

TextStream& TextStream::operator<<(long value)
{
    if (value >= 0)
        printInteger(value, false);
    else
        printInteger(-static_cast<max_int_type>(value), true);
    reset();
    return *this;
}

TextStream& TextStream::operator<<(unsigned long value)
{
    printInteger(value, false);
    reset();
    return *this;
}

TextStream& TextStream::operator<<(long long value)
{
    if (value >= 0)
        printInteger(value, false);
    else
        printInteger(-static_cast<max_int_type>(value), true);
    reset();
    return *this;
}

TextStream& TextStream::operator<<(unsigned long long value)
{
    printInteger(value, false);
    reset();
    return *this;
}

// -----------------------------------------------------------------------------
//     Floating point printing
// -----------------------------------------------------------------------------

TextStream& TextStream::operator<<(float value)
{
    m_sink->putString("TODO", 4);
    reset();
    return *this;
}

TextStream& TextStream::operator<<(double value)
{
    m_sink->putString("TODO", 4);
    reset();
    return *this;
}

TextStream& TextStream::operator<<(long double value)
{
    m_sink->putString("TODO", 4);
    reset();
    return *this;
}

// -----------------------------------------------------------------------------
//     Pointer printing
// -----------------------------------------------------------------------------

TextStream& TextStream::operator<<(const void* value)
{
    // TODO: print padding before changing the format

    m_format.align = Format::AlignAfterSign;
    m_format.fill = '0';
    m_format.width = 2 + sizeof(void*) * 2;
    m_format.basePrefix = true;
    m_format.type = Format::Hex;

    printInteger(uintptr_t(value), false); // TODO: call printIntegerDigits directly
    reset();
    return *this;
}

// -----------------------------------------------------------------------------
//     String printing
// -----------------------------------------------------------------------------

TextStream& TextStream::operator<<(const char* str)
{
    // TODO: padding

    m_sink->putString(str, std::strlen(str));
    reset();
    return *this;
}

TextStream& TextStream::operator<<(Immutable<const char*> str)
{
    // TODO: padding

    m_sink->putString(static_cast<const char*>(str),
                      std::strlen(static_cast<const char*>(str)));
    reset();
    return *this;
}

TextStream& TextStream::operator<<(const log11_detail::SplitString& str)
{
    // TODO: padding

    if (str.length1)
        m_sink->putString(str.begin1, str.length1);
    if (str.length2)
        m_sink->putString(str.begin2, str.length2);
    reset();
    return *this;
}

// ----=====================================================================----
//     Private methods
// ----=====================================================================----

void TextStream::printArgument(unsigned)
{
    m_sink->putString("<?>", 3);
}

void TextStream::reset()
{
    m_format = Format();

    m_padding = 0;
}

template <unsigned char TBase>
int TextStream::countDigits(max_int_type value, max_int_type& divisor)
{
    divisor = 1;
    int digits = 1;
    while (value >= TBase)
    {
        value /= TBase;
        divisor *= TBase;
        ++digits;
    }
    return digits;
}

template <unsigned char TBase>
void TextStream::printIntegerDigits(max_int_type value, max_int_type divisor)
{
    while (divisor)
    {
        unsigned char digit = value / divisor;
        if (TBase <= 10)
            m_sink->putChar('0' + digit);
        else
            m_sink->putChar(digit < 10 ? '0' + digit
                                       : (m_format.upperCase ? 'A'- 10 + digit
                                                             : 'a'- 10 + digit));
        value -= digit * divisor;
        divisor /= TBase;
    }
}

void TextStream::printInteger(max_int_type value, bool isNegative)
{
    if (m_format.align == Format::AutoAlign)
        m_format.align = Format::Right;

    m_padding = m_format.width;
    max_int_type divisor;
    switch (m_format.type)
    {
    case Format::Binary:  m_padding -= countDigits< 2>(value, divisor); break;
    default:
    case Format::Decimal: m_padding -= countDigits<10>(value, divisor); break;
    case Format::Octal:   m_padding -= countDigits< 8>(value, divisor); break;
    case Format::Hex:     m_padding -= countDigits<16>(value, divisor); break;
    }

    printPrePaddingAndSign(isNegative);

    switch (m_format.type)
    {
    case Format::Binary:  printIntegerDigits< 2>(value, divisor); break;
    default:
    case Format::Decimal: printIntegerDigits<10>(value, divisor); break;
    case Format::Octal:   printIntegerDigits< 8>(value, divisor); break;
    case Format::Hex:     printIntegerDigits<16>(value, divisor); break;
    }

    if (m_format.align == Format::Left || m_format.align == Format::Centered)
        while (m_padding-- > 0)
            m_sink->putChar(m_format.fill);
}

void TextStream::printPrePaddingAndSign(bool isNegative)
{
    if (isNegative || m_format.sign != Format::OnlyNegative)
        --m_padding;
    if (m_format.basePrefix)
        m_padding -= 2;

    if (m_format.align == Format::Right)
    {
        while (m_padding-- > 0)
            m_sink->putChar(m_format.fill);
    }
    else if (m_format.align == Format::Centered)
    {
        for (int count = 0; count < (m_padding + 1) / 2; ++count)
            m_sink->putChar(m_format.fill);
        m_padding /= 2;
    }

    if (isNegative)
        m_sink->putChar('-');
    else if (m_format.sign == Format::SpaceForPositive)
        m_sink->putChar(' ');
    else if (m_format.sign == Format::Always)
        m_sink->putChar('+');

    if (m_format.basePrefix)
        switch (m_format.type)
        {
        case Format::Binary:  m_sink->putString("0b", 2); break;
        default:
        case Format::Decimal: m_sink->putString("0d", 2); break;
        case Format::Octal:   m_sink->putString("0o", 2); break;
        case Format::Hex:     m_sink->putString("0x", 2); break;
        }

    if (m_format.align == Format::AlignAfterSign)
        while (m_padding-- > 0)
            m_sink->putChar(m_format.fill);
}

} // namespace log11
