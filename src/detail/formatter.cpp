#include <iostream>

#include "../sink.hpp"

#include <math.h>
#include <type_traits>

using namespace std;


class TempSink
{
public:
    void putChar(char ch) { cout << ch; }
    void putString(const char* s) { cout << s; }
};

struct Formatter
{
    enum Alignment
    {
        AutoAlign,
        Left,
        Right,
        Centered,
        AlignAfterSign
    };

    enum SignPolicy
    {
        OnlyNegative,
        SpaceForPositive,
        Always
    };

    enum Type
    {
        Default,

        Binary,
        Character,
        Decimal,
        Octal,
        Hex

        // TODO: Float types
    };

    // Format spec
    int m_width;
    int m_precision;
    char m_fill;
    Alignment m_align;
    SignPolicy m_sign;
    bool m_basePrefix;
    bool m_upperCase;
    Type m_type;


    bool m_isNegative;
    int m_padding;

    TempSink* m_sink;

    Formatter();

    //! Resets the formatter.
    void reset();

    //! Parses the format string starting with \p str. Returns a pointer past
    //! the end of the format string.
    const char* parse(const char* str);


    template <typename T>
    typename std::enable_if<std::is_integral<T>::value && !std::is_signed<T>::value>::type
    print(T value)
    {
        printInteger(value);
    }

    template <typename T>
    typename std::enable_if<std::is_integral<T>::value && std::is_signed<T>::value>::type
    print(T value)
    {
        if (value > 0)
        {
            printInteger(value);
        }
        else
        {
            m_isNegative = true;
            printInteger(-value);
        }
    }

    template <typename T>
    typename std::enable_if<std::is_floating_point<T>::value>::type
    print(T value)
    {
        if (m_precision == 0)
            m_precision = 6;

        m_isNegative = signbit(value);
        // Handle special values.
        int klass = fpclassify(value);
        if (klass == FP_NAN || klass == FP_INFINITE)
        {
            m_padding = m_width - 3;
            printPrePaddingAndSign();
            switch (klass)
            {
            case FP_NAN: m_sink->putString("nan"); break;
            case FP_INFINITE: m_sink->putString("inf"); break;
            }
        }
        else
        {
            printFloatFixedPoint(value);
        }
    }

private:
    template <unsigned char Base>
    std::pair<int, std::uint64_t> getDigitsAndMask(std::uint64_t value)
    {
        std::uint64_t mask = 1;
        int digits = 1;
        while (value >= Base)
        {
            value /= Base;
            mask *= Base;
            ++digits;
        }
        return std::make_pair(digits, mask);
    }

    void printInteger(std::uint64_t value);
    void printFloatFixedPoint(double value);

    void printPrePaddingAndSign();

    template <unsigned char Base>
    void doPrintIntegerValue(std::uint64_t x, std::uint64_t mask)
    {
        while (mask)
        {
            unsigned char digit = x / mask;
            if (Base <= 10)
                m_sink->putChar('0' + digit);
            else
                m_sink->putChar(digit < 10 ? '0' + digit
                                           : (m_upperCase ? 'A' + digit - 10
                                                          : 'a' + digit - 10));
            x -= digit * mask;
            mask /= Base;
        }
    }
};

Formatter::Formatter()
{
    reset();

    m_sink = new TempSink;
}

void Formatter::reset()
{
    m_width = 0;
    m_precision = 0;
    m_fill = ' ';
    m_align = AutoAlign;
    m_sign = OnlyNegative;
    m_basePrefix = false;
    m_upperCase = false;
    m_type = Default;

    m_isNegative = false;
    m_padding = 0;
}

const char* Formatter::parse(const char* str)
{
    if (!*str || *str == '}')
        return str;

    switch (str[1])
    {
    case '<':
    case '>':
    case '^':
    case '=':
        m_fill = *str++;
    }

    // align
    switch (*str)
    {
    case '<': m_align = Left; ++str; break;
    case '>': m_align = Right; ++str; break;
    case '^': m_align = Centered; ++str; break;
    case '=': m_align = AlignAfterSign; ++str; break;
    }

    // sign
    switch (*str)
    {
    case '+': m_sign = Always; ++str; break;
    case '-': m_sign = OnlyNegative; ++str; break;
    case ' ': m_sign = SpaceForPositive; ++str; break;
    }

    // base prefix
    if (*str == '#')
    {
        m_basePrefix = true;
        ++str;
    }

    // TODO: if (ch == '0') {}

    // width
    while (*str >= '0' && *str <= '9')
        m_width = 10 * m_width + (*str++ - '0');

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
    case 'b': m_type = Binary; break;
    case 'c': m_type = Character; break;
    case 'd': m_type = Decimal; break;
    case 'o': m_type = Octal; break;
    case 'x': m_type = Hex; break;
    case 'X': m_type = Hex; m_upperCase = true; break;

        // TODO: float formats
    }

    while (*str && *str != '}')
        ++str;

    return str;
}


void Formatter::printPrePaddingAndSign()
{
    if (m_isNegative || m_sign != Formatter::OnlyNegative)
        --m_padding;

    if (m_align == Formatter::Right)
    {
        while (m_padding-- > 0)
            m_sink->putChar(m_fill);
    }
    else if (m_align == Formatter::Centered)
    {
        for (int count = 0; count < (m_padding + 1) / 2; ++count)
            m_sink->putChar(m_fill);
        m_padding /= 2;
    }

    if (m_isNegative)
    {
        m_sink->putChar('-');
    }
    else
    {
        switch (m_sign)
        {
        case Formatter::SpaceForPositive: m_sink->putChar(' '); break;
        case Formatter::Always: m_sink->putChar('+'); break;
        default: break;
        }
    }

    if (m_align == Formatter::AlignAfterSign)
        while (m_padding-- > 0)
            m_sink->putChar(m_fill);
}

void Formatter::printInteger(std::uint64_t value)
{
    if (m_align == Formatter::AutoAlign)
        m_align = Formatter::Right;

    std::pair<int, std::uint64_t> digitsMask;
    switch (m_type)
    {
    case Formatter::Binary: digitsMask = getDigitsAndMask<2>(value); break;
    default:
    case Formatter::Decimal: digitsMask = getDigitsAndMask<10>(value); break;
    case Formatter::Octal: digitsMask = getDigitsAndMask<8>(value); break;
    case Formatter::Hex: digitsMask = getDigitsAndMask<16>(value); break;
    }

    m_padding = m_width - digitsMask.first;
    if (m_basePrefix)
        m_padding -= 2;

    printPrePaddingAndSign();

    switch (m_type)
    {
    case Formatter::Binary:
        if (m_basePrefix)
            m_sink->putString("0b");
        doPrintIntegerValue<2>(value, digitsMask.second);
        break;

    default:
    case Formatter::Decimal:
        if (m_basePrefix)
            m_sink->putString("0d");
        doPrintIntegerValue<10>(value, digitsMask.second);
        break;

    case Formatter::Octal:
        if (m_basePrefix)
            m_sink->putString("0o");
        doPrintIntegerValue<8>(value, digitsMask.second);
        break;

    case Formatter::Hex:
        if (m_basePrefix)
            m_sink->putString("0x");
        doPrintIntegerValue<16>(value, digitsMask.second);
        break;
    }

    if (m_align == Formatter::Left || m_align == Formatter::Centered)
        while (m_padding-- > 0)
            m_sink->putChar(m_fill);
}



void Formatter::printFloatFixedPoint(double value)
{
    if (value < 0)
    {
        m_isNegative = true;
        value = -value;
    }

    double integer;
    double fraction = modf(value, &integer);
    for (int count = 0; count < m_precision; ++count)
        fraction *= 10;
    fraction += double(0.5);

    std::pair<int, std::uint64_t> digitsMask = getDigitsAndMask<10>(integer);
    m_padding = m_width - digitsMask.first - m_precision - 1;
    printPrePaddingAndSign();
    doPrintIntegerValue<10>(integer, digitsMask.second);
    m_sink->putChar('.');

    digitsMask = getDigitsAndMask<10>(fraction);
    for (int count = digitsMask.first; count < m_precision; ++count)
        m_sink->putChar('0');
    doPrintIntegerValue<10>(fraction, digitsMask.second);
}



void print(const Formatter& s)
{
    cout << '\'' << s.m_fill << '\'';

    switch (s.m_align)
    {
    default: cout << '?'; break;
    case Formatter::Left: cout << '<'; break;
    case Formatter::Right: cout << '>'; break;
    case Formatter::Centered: cout << '^'; break;
    case Formatter::AlignAfterSign: cout << '='; break;
    case Formatter::AutoAlign: cout << '@'; break;
    }

    switch (s.m_sign)
    {
    default: cout << '?'; break;
    case Formatter::OnlyNegative: cout << '-'; break;
    case Formatter::SpaceForPositive: cout << ' '; break;
    case Formatter::Always: cout << '+'; break;
    }

    if (s.m_basePrefix)
        cout << '#';

    cout << s.m_width;
    cout << '.';
    cout << s.m_precision;

    switch (s.m_type)
    {
    default: cout << '?'; break;
    case Formatter::Default: cout << '@'; break;
    case Formatter::Binary: cout << 'b'; break;
    case Formatter::Character: cout << 'c'; break;
    case Formatter::Decimal: cout << 'd'; break;
    case Formatter::Octal: cout << 'o'; break;
    case Formatter::Hex: cout << 'x'; break;
    case Formatter::Hex: cout << 'x'; break;
    }

    cout << endl;
}


int main()
{
    cout << "Hello World!" << endl;


    char test1[] = ".= 11.4d}";

    Formatter s;
    auto end = s.parse(test1);
    cout << *end << endl;
    print(s);

    cout << "---\n";
    cout << ">>";
    s.print(-123.1234565);
    cout << "<<\n";

    return 0;
}

