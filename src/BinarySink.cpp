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

#include "BinarySink.hpp"

#include <cstdint>
#include <type_traits>

using namespace std;


namespace log11
{

template <typename T>
constexpr
std::enable_if_t<std::is_unsigned<T>::value, bool> isVarIntProfitable(T value)
{
    return sizeof(T) > 1 && value < T(1ull << (7 * (sizeof(T) - 1)));
}

template <typename T, typename U = std::make_unsigned_t<T>>
constexpr
std::enable_if_t<std::is_signed<T>::value, bool> isVarIntProfitable(T value)
{
    return sizeof(T) > 1
            && value >= -T(1ull << (7 * (sizeof(T) - 1))) / 2
            && value < T(1ull << (7 * (sizeof(T) - 1))) / 2;
}

template <unsigned N, bool S>
struct IntToTagMap;

#define INT_TO_TAG_MAP(n, s, x, y)                                             \
    template <>                                                                \
    struct IntToTagMap<n, s>                                                   \
    {                                                                          \
        static constexpr auto tag = BinarySink::TypeTag::x;                    \
        static constexpr auto var_tag = BinarySink::TypeTag::y;                \
    };

INT_TO_TAG_MAP(1, true, Int8,  Int8 /*dummy*/)
INT_TO_TAG_MAP(2, true, Int16, VarInt16)
INT_TO_TAG_MAP(4, true, Int32, VarInt32)
INT_TO_TAG_MAP(8, true, Int64, VarInt64)

INT_TO_TAG_MAP(1, false, Uint8,  Uint8 /*dummy*/)
INT_TO_TAG_MAP(2, false, Uint16, VarUint16)
INT_TO_TAG_MAP(4, false, Uint32, VarUint32)
INT_TO_TAG_MAP(8, false, Uint64, VarUint64)



template <bool TFits32Bit, bool TSigned>
struct promote_integer_type_helper;

template <>
struct promote_integer_type_helper<true, true>   { using type = std::int32_t; };

template <>
struct promote_integer_type_helper<false, true>  { using type = std::int64_t; };

template <>
struct promote_integer_type_helper<true, false>  { using type = std::uint32_t; };

template <>
struct promote_integer_type_helper<false, false> { using type = std::uint64_t; };


template <typename T>
using promoted_integer_t = typename promote_integer_type_helper<sizeof(T) <= 4, is_signed<T>::value>::type;

// ----=====================================================================----
//     BinarySink
// ----=====================================================================----

void BinarySink::beginLogEntry(Severity severity)
{
    // TODO
}

void BinarySink::endLogEntry()
{
    // TODO
}

void BinarySink::writeBytes(const byte* data, unsigned size)
{
    while (size--)
    {
        writeByte(*data++);
    }
}

// -----------------------------------------------------------------------------
//     Bool & char output
// -----------------------------------------------------------------------------

void BinarySink::write(bool value)
{
    writeTag(value ? TypeTag::True : TypeTag::False);
}

void BinarySink::write(char ch)
{
    writeTag(TypeTag::Char);
    writeByte(static_cast<byte>(ch));
}

// -----------------------------------------------------------------------------
//     Integer output
// -----------------------------------------------------------------------------

void BinarySink::write(signed char value)
{
    writeInteger(value);
}

void BinarySink::write(unsigned char value)
{
    writeInteger(value);
}

void BinarySink::write(short value)
{
    writeInteger(value);
}

void BinarySink::write(unsigned short value)
{
    writeInteger(value);
}

void BinarySink::write(int value)
{
    writeInteger(value);
}

void BinarySink::write(unsigned int value)
{
    writeInteger(value);
}

void BinarySink::write(long value)
{
    writeInteger(value);
}

void BinarySink::write(unsigned long value)
{
    writeInteger(value);
}

void BinarySink::write(long long value)
{
    writeInteger(value);
}

void BinarySink::write(unsigned long long value)
{
    writeInteger(value);
}

// -----------------------------------------------------------------------------
//     Floating point output
// -----------------------------------------------------------------------------

void BinarySink::write(float value)
{
    writeTag(TypeTag::Float);
    writeBytes(reinterpret_cast<byte*>(&value), sizeof(value));
}

void BinarySink::write(double value)
{
    writeTag(TypeTag::Double);
    writeBytes(reinterpret_cast<byte*>(&value), sizeof(value));
}

void BinarySink::write(long double value)
{
    writeTag(TypeTag::LongDouble);
    writeBytes(reinterpret_cast<byte*>(&value), sizeof(value));
}

// -----------------------------------------------------------------------------
//     Pointer output
// -----------------------------------------------------------------------------

void BinarySink::write(const void* value)
{
    writeTag(TypeTag::Pointer);
    writeBytes(reinterpret_cast<byte*>(&value), sizeof(value));
}

// -----------------------------------------------------------------------------
//     String output
// -----------------------------------------------------------------------------

void BinarySink::write(Immutable<const char*> str)
{
    writeTag(TypeTag::StringPointer);
    writeBytes(reinterpret_cast<byte*>(&str), sizeof(Immutable<const char*>));
}

void BinarySink::write(const log11_detail::SplitString& str)
{
    writeTag(TypeTag::String);
    auto totalSize = str.length1 + str.length2;
    writeVarInt(static_cast<promoted_integer_t<decltype(totalSize)>>(
        totalSize));
    if (str.length1)
        writeBytes(reinterpret_cast<const byte*>(str.begin1), str.length1);
    if (str.length2)
        writeBytes(reinterpret_cast<const byte*>(str.begin2), str.length2);
}

// ----=====================================================================----
//     Protected methods
// ----=====================================================================----

void BinarySink::writeTag(TypeTag tag)
{
    writeByte(static_cast<byte>(tag));
}

void BinarySink::writeVarInt(std::uint32_t value)
{
    while (value > 0x7F)
    {
        writeByte((value & 0x7F) | 0x80);
        value >>= 7;
    }
    writeByte(value);
}

void BinarySink::writeVarInt(std::int32_t x)
{
    using U = unsigned;
    U value = static_cast<U>(x) << 1;
    if (x < 0)
        value = ~value;
    while (value > 0x7F)
    {
        writeByte((value & 0x7F) | 0x80);
        value >>= 7;
    }
    writeByte(value);
}

void BinarySink::writeVarInt(std::uint64_t value)
{
    while (value > 0x7F)
    {
        writeByte((value & 0x7F) | 0x80);
        value >>= 7;
    }
    writeByte(value);
}

void BinarySink::writeVarInt(std::int64_t x)
{
    using U = unsigned long long;
    U value = static_cast<U>(x) << 1;
    if (x < 0)
        value = ~value;
    while (value > 0x7F)
    {
        writeByte((value & 0x7F) | 0x80);
        value >>= 7;
    }
    writeByte(value);
}

template <typename T>
void BinarySink::writeInteger(T value)
{
    if (isVarIntProfitable(value))
    {
        writeTag(IntToTagMap<sizeof(T), is_signed<T>::value>::var_tag);
        writeVarInt(static_cast<promoted_integer_t<T>>(value));
    }
    else
    {
        writeTag(IntToTagMap<sizeof(T), is_signed<T>::value>::tag);
        writeBytes(reinterpret_cast<byte*>(&value), sizeof(value));
    }
}

} // namespace log11
