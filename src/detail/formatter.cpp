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

class Formatter
{
public:
    Formatter();

    //! Resets the formatter.
    void reset();

    //! Parses the format string starting with \p str. Returns a pointer past
    //! the end of the format string.
    const char* parse(const char* str);

#define override

    virtual
    void visit(char value) override
    {
        if (m_type == Default)
            m_type = Character;
        printInteger(value);
    }


    virtual
    void visit(signed char value) override
    {
        printInteger(value);
    }

    virtual
    void visit(unsigned char value) override
    {
        printInteger(value);
    }

    virtual
    void visit(short value) override
    {
        printInteger(value);
    }

    virtual
    void visit(unsigned short value) override
    {
        printInteger(value);
    }

    virtual
    void visit(int value) override
    {
        printInteger(value);
    }

    virtual
    void visit(unsigned value) override
    {
        printInteger(value);
    }

    virtual
    void visit(long value) override
    {
        printInteger(value);
    }

    virtual
    void visit(unsigned long value) override
    {
        printInteger(value);
    }

    virtual
    void visit(long long value) override
    {
        printInteger(value);
    }

    virtual
    void visit(unsigned long long value) override
    {
        printInteger(value);
    }

    // TODO: floating point

    virtual
    void visit(const void* value) override
    {
        if (value)
        {
            if (m_type == Default)
                m_type = Hex;
            // TODO: base prefix, width, fill
            printInteger(uintptr_t(value));
            auto length = snprintf(m_logger.m_conversionBuffer, sizeof(m_logger.m_conversionBuffer),
                                   "0x%08lx", uintptr_t(value));
            m_logger.m_sink.load()->putString(m_logger.m_conversionBuffer, length);
        }
        else
        {
            m_logger.m_sink.load()->putString("<null>", 6);
        }
    }


private:
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

    // Filled when parsing the format specification.
    int m_argumentIndex;
    int m_width;
    int m_precision;
    char m_fill;
    Alignment m_align;
    SignPolicy m_sign;
    bool m_basePrefix;
    bool m_upperCase;
    Type m_type;

    // Additional state for printing.
    bool m_isNegative;
    int m_padding;

    TempSink* m_sink;


    template <typename T>
    typename std::enable_if<!std::is_signed<T>::value>::type
    printInteger(T value)
    {
        doPrintInteger(value);
    }

    template <typename T>
    typename std::enable_if<std::is_signed<T>::value>::type
    printInteger(T value)
    {
        if (value > 0)
        {
            doPrintInteger(value);
        }
        else
        {
            m_isNegative = true;
            doPrintInteger(-value);
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

    void doPrintInteger(std::uint64_t value);
    void printFloatFixedPoint(double value);

    void printPrePaddingAndSign();

    template <unsigned char Base>
    void printIntegerInBase(std::uint64_t x, std::uint64_t mask)
    {
        while (mask)
        {
            unsigned char digit = x / mask;
            if (Base <= 10)
                m_sink->putChar('0' + digit);
            else
                m_sink->putChar(digit < 10 ? '0' + digit
                                           : (m_upperCase ? 'A' - 10 + digit
                                                          : 'a' - 10 + digit));
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
    m_argumentIndex = 0;
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
    // argument index
    while (*str >= '0' && *str <= '9')
        m_argumentIndex = 10 * m_argumentIndex + (*str++ - '0');

    switch (*str)
    {
    case '}':
        return str;
    case ':':
        ++str;
        if (*str)
            break;
    default:
        // TODO: print an error indicator
        while (*str && *str != '}')
            ++str;
        return str;
    }

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

    if (*str == '0')
    {
        ++str;
        m_fill = '0';
        m_align = AlignAfterSign;
    }

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

    if (*str != '}')
    {
        // TODO: print an error indicator
        while (*str && *str != '}')
            ++str;
    }

    return str;
}


void Formatter::printPrePaddingAndSign()
{
    if (m_isNegative || m_sign != OnlyNegative)
        --m_padding;

    if (m_align == Right)
    {
        while (m_padding-- > 0)
            m_sink->putChar(m_fill);
    }
    else if (m_align == Centered)
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
        case SpaceForPositive: m_sink->putChar(' '); break;
        case Always: m_sink->putChar('+'); break;
        default: break;
        }
    }

    if (m_align == AlignAfterSign)
        while (m_padding-- > 0)
            m_sink->putChar(m_fill);
}

void Formatter::doPrintInteger(std::uint64_t value)
{
    using namespace std;

    if (m_align == AutoAlign)
        m_align = Right;

    pair<int, uint64_t> digitsMask;
    switch (m_type)
    {
    case Binary: digitsMask = getDigitsAndMask<2>(value); break;
    case Character: digitsMask = pair<int, uint64_t>(1, 0); break;
    default:
    case Decimal: digitsMask = getDigitsAndMask<10>(value); break;
    case Octal: digitsMask = getDigitsAndMask<8>(value); break;
    case Hex: digitsMask = getDigitsAndMask<16>(value); break;
    }

    m_padding = m_width - digitsMask.first;
    if (m_basePrefix)
        m_padding -= 2;

    printPrePaddingAndSign();

    switch (m_type)
    {
    case Binary:
        if (m_basePrefix)
            m_sink->putString("0b");
        printIntegerInBase<2>(value, digitsMask.second);
        break;

    case Character:
        if (!m_isNegative && value < 255)
            m_sink->putChar(value);
        else
            m_sink->putChar('?');
        break;

    default:
    case Decimal:
        if (m_basePrefix)
            m_sink->putString("0d");
        printIntegerInBase<10>(value, digitsMask.second);
        break;

    case Octal:
        if (m_basePrefix)
            m_sink->putString("0o");
        printIntegerInBase<8>(value, digitsMask.second);
        break;

    case Hex:
        if (m_basePrefix)
            m_sink->putString("0x");
        printIntegerInBase<16>(value, digitsMask.second);
        break;
    }

    if (m_align == Left || m_align == Centered)
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
    printIntegerInBase<10>(integer, digitsMask.second);
    m_sink->putChar('.');

    digitsMask = getDigitsAndMask<10>(fraction);
    for (int count = digitsMask.first; count < m_precision; ++count)
        m_sink->putChar('0');
    printIntegerInBase<10>(fraction, digitsMask.second);
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


class Fmt
{
public:

    void printBool(bool v);
    void printCharacter(char v);
    void printInteger(std::int64_t v);
    void printInteger(std::uint64_t v);
    void printFloat(double v);
    void printFloatFixPoint(double v);
    void printFloatScientific(double v);
    void printPointer(const void* v);
    void printString(const char* v);
    void printString(StringRef v);

    void print(bool v);

    void print(char v);

    template <typename T>
    std::enable_if_t<std::is_integral<T>::value && std::is_signed<T>::value>
    print(T v);

    template <typename T>
    std::enable_if_t<std::is_integral<T>::value && !std::is_signed<T>::value>
    print(T v);


};



struct Xyz
{
};

void log(Fmt& f, Xyz& v);





class Vormatter
{
public:
    const char* parseFormat(const char* spec);


    Vormatter& print(bool value);
    Vormatter& print(char value);
    Vormatter& print(signed char value);
    Vormatter& print(unsigned char value);

    Vormatter& print(short value);
    Vormatter& print(unsigned short value);

    Vormatter& print(int value);
    Vormatter& print(unsigned int value);

    // ...

    Vormatter& print(float value);
    Vormatter& print(double value);

    // ... const void*, nullptr_t, const char* ...


    Vormatter& print(const char* spec, bool value);

    // ...


private:
};


Vormatter& Vormatter::print(bool value)
{

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

