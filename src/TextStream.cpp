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

#include <cmath>
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
        alternateForm = true;
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
        minWidth = 10 * minWidth + (*str++ - '0');

    // precision
    if (*str == '.')
    {
        precision = 0;
        ++str;
        while (*str >= '0' && *str <= '9')
            precision = 10 * precision + (*str++ - '0');
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

    case 'e': type = Exponent; break;
    case 'E': type = Exponent; upperCase = true; break;
    case 'f': type = FixedPoint; break;
    case 'F': type = FixedPoint; upperCase = true; break;
    case 'g': type = GeneralFloat; break;
    case 'G': type = GeneralFloat; upperCase = true; break;

        // TODO:
    //case '%':
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
    if (m_format.align == Format::AutoAlign)
        m_format.align = Format::Left;

    int padding = m_format.minWidth - (value ? 4 : 5);
    padding = printPrePaddingAndSign(padding, false, Format::NoType);
    if (value)
        m_sink->putString("true", 4);
    else
        m_sink->putString("false", 5);
    printPostPadding(padding);

    reset();
    return *this;
}

TextStream& TextStream::operator<<(char ch)
{
    if (m_format.align == Format::AutoAlign)
        m_format.align = Format::Left;

    int padding = m_format.minWidth - 1;
    padding = printPrePaddingAndSign(padding, false, Format::NoType);
    m_sink->putChar(ch);
    printPostPadding(padding);
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
    printFloat(value);
    reset();
    return *this;
}

TextStream& TextStream::operator<<(double value)
{
    printFloat(value);
    reset();
    return *this;
}

TextStream& TextStream::operator<<(long double value)
{
    printFloat(value);
    reset();
    return *this;
}

// -----------------------------------------------------------------------------
//     Pointer printing
// -----------------------------------------------------------------------------

TextStream& TextStream::operator<<(const void* value)
{
    if (m_format.align == Format::AutoAlign)
        m_format.align = Format::Right;
    int padding = m_format.minWidth - 2 - 2 * sizeof(void*);
    padding = printPrePaddingAndSign(padding, false, Format::NoType);

    m_sink->putString("0x", 2);
    printIntegerDigits<16>(uintptr_t(value), uintptr_t(1) << (sizeof(void*) * 8 - 4));

    printPostPadding(padding);
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

TextStream& TextStream::operator<<(const SplitStringView& str)
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
    // TODO: Padding

    m_sink->putString("<?>", 3);
}

void TextStream::reset()
{
    m_format = Format();
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
    if (m_format.type == Format::NoType)
        m_format.type = Format::Decimal;

    int padding = m_format.minWidth;
    max_int_type divisor;
    switch (m_format.type)
    {
    case Format::Binary:  padding -= countDigits< 2>(value, divisor); break;
    default:
    case Format::Decimal: padding -= countDigits<10>(value, divisor); break;
    case Format::Octal:   padding -= countDigits< 8>(value, divisor); break;
    case Format::Hex:     padding -= countDigits<16>(value, divisor); break;
    }

    padding = printPrePaddingAndSign(
                  padding,
                  isNegative,
                  m_format.alternateForm ? m_format.type : Format::NoType);

    switch (m_format.type)
    {
    case Format::Binary:  printIntegerDigits< 2>(value, divisor); break;
    default:
    case Format::Decimal: printIntegerDigits<10>(value, divisor); break;
    case Format::Octal:   printIntegerDigits< 8>(value, divisor); break;
    case Format::Hex:     printIntegerDigits<16>(value, divisor); break;
    }

    printPostPadding(padding);
}

void TextStream::printFloat(double value)
{
    if (m_format.align == Format::AutoAlign)
        m_format.align = Format::Right;
    if (m_format.precision < 0)
        m_format.precision = 6;
    if (m_format.type == Format::NoType)
        m_format.type = Format::GeneralFloat;

    bool isNegative = signbit(value);
    if (isNegative)
        value = -value;

    bool removeTrailingZeros = m_format.type == Format::GeneralFloat && !m_format.alternateForm;
    int exponent = 0;

    // Handle special values.
    int klass = fpclassify(value);
    if (klass == FP_NAN || klass == FP_INFINITE)
    {
        int padding = m_format.minWidth - 3;
        padding = printPrePaddingAndSign(
                    padding,
                    klass == FP_INFINITE && isNegative, Format::NoType);
        switch (klass)
        {
        case FP_NAN:      m_sink->putString("nan", 3); break;
        case FP_INFINITE: m_sink->putString("inf", 3); break;
        }
        printPostPadding(padding);
        return;
    }
    else if (klass == FP_ZERO)
    {
        // Zeros are handled just like numbers in the interval [0, 10).
        if (m_format.type == Format::GeneralFloat)
        {
            m_format.type = Format::FixedPoint;
            if (m_format.precision)
                m_format.precision -= 1;
        }
    }
    else
    {
        if (m_format.type == Format::GeneralFloat)
        {
            if (m_format.precision == 0)
                m_format.precision = 1;

            exponent = floor(log10(value));
            double temp = value * pow(double(10), -exponent);
            temp += 5 * pow(double(10), -double(m_format.precision));
            if (temp >= double(10))
            {
                temp /= double(10);
                ++exponent;
            }

            if (exponent >= -4 && exponent < m_format.precision - 1)
            {
                m_format.type = Format::FixedPoint;
                m_format.precision = m_format.precision - exponent - 1;
                value += 5 * pow(double(10), -double(m_format.precision + 1));
            }
            else
            {
                m_format.type = Format::Exponent;
                m_format.precision -= 1;
                value = temp;
            }
        }
        else
        {
            if (m_format.type == Format::Exponent)
            {
                // Scale the value such that 1 <= value < 10.
                exponent = floor(log10(value));
                value *= pow(double(10), -exponent);
            }

            value += 5 * pow(double(10), -double(m_format.precision + 1));
            if (m_format.type == Format::Exponent && value >= double(10))
            {
                value /= double(10);
                ++exponent;
            }
        }
    }

    max_int_type integer, fraction;
    unsigned fractionDivisor;
    {
        double intf;
        double fracf = modf(value, &intf);
        double scale = pow(double(10), double(m_format.precision));
        fracf *= scale;
        integer = intf;
        fraction = fracf;
        fractionDivisor = unsigned(scale) / 10;
    }
    if (removeTrailingZeros)
        while (m_format.precision && fraction % 10 == 0)
        {
            fraction /= 10;
            fractionDivisor /= 10;
            --m_format.precision;
        }

    max_int_type divisor = 1;
    int integerDigits = m_format.type == Format::Exponent
                        ? 1
                        : countDigits<10>(integer, divisor);
    int padding = m_format.minWidth - (integerDigits + m_format.precision);
    if (m_format.precision || m_format.alternateForm)
        padding -= 1; // '.'
    if (m_format.type == Format::Exponent)
        padding -= 4; // 'e+xx'

    padding = printPrePaddingAndSign(padding, isNegative, Format::NoType);
    printIntegerDigits<10>(integer, divisor);

    if (m_format.precision)
    {
        m_sink->putChar('.');
        printIntegerDigits<10>(fraction, fractionDivisor ? fractionDivisor : 1);
    }
    else if (m_format.alternateForm)
        m_sink->putChar('.');

    if (m_format.type == Format::Exponent)
    {
         m_sink->putChar(m_format.upperCase ? 'E' : 'e');
         if (exponent >= 0)
         {
             m_sink->putChar('+');
         }
         else
         {
             m_sink->putChar('-');
             exponent = -exponent;
         }
         printIntegerDigits<10>(exponent, 10);
    }

    printPostPadding(padding);
}

int TextStream::printPrePaddingAndSign(
        int padding, bool isNegative, Format::Type prefix)
{
    if (isNegative || m_format.sign != Format::OnlyNegative)
        --padding;
    if (prefix != Format::NoType)
        padding -= 2;

    if (m_format.align == Format::Right)
    {
        while (padding-- > 0)
            m_sink->putChar(m_format.fill);
    }
    else if (m_format.align == Format::Centered)
    {
        for (int count = 0; count < (padding + 1) / 2; ++count)
            m_sink->putChar(m_format.fill);
        padding /= 2;
    }

    if (isNegative)
        m_sink->putChar('-');
    else if (m_format.sign == Format::SpaceForPositive)
        m_sink->putChar(' ');
    else if (m_format.sign == Format::Always)
        m_sink->putChar('+');

    switch (prefix)
    {
    default:
    case Format::NoType:  break;
    case Format::Binary:  m_sink->putString("0b", 2); break;
    case Format::Decimal: m_sink->putString("0d", 2); break;
    case Format::Octal:   m_sink->putString("0o", 2); break;
    case Format::Hex:     m_sink->putString("0x", 2); break;
    }

    if (m_format.align == Format::AlignAfterSign)
        while (padding-- > 0)
            m_sink->putChar(m_format.fill);

    return padding;
}

void TextStream::printPostPadding(int padding)
{
    if (m_format.align == Format::Left || m_format.align == Format::Centered)
        while (padding-- > 0)
            m_sink->putChar(m_format.fill);
}

} // namespace log11
