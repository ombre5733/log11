/*******************************************************************************
  log11
  https://github.com/ombre5733/log11

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

// ----=====================================================================----
//     BinarySink
// ----=====================================================================----

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
    if (!isCurrentRecordLogged())
        return;

    writeByte(value ? 0xE0 : 0xE1);
}

void BinarySink::write(char ch)
{
    if (!isCurrentRecordLogged())
        return;

    writeByte(0x41);
    writeByte(byte(ch));
}

// -----------------------------------------------------------------------------
//     Integer output
// -----------------------------------------------------------------------------

void BinarySink::write(signed char value)
{
    if (!isCurrentRecordLogged())
        return;

    writeSignedInteger(value);
}

void BinarySink::write(unsigned char value)
{
    if (!isCurrentRecordLogged())
        return;

    writeUnsignedInteger(value);
}

void BinarySink::write(short value)
{
    if (!isCurrentRecordLogged())
        return;

    writeSignedInteger(value);
}

void BinarySink::write(unsigned short value)
{
    if (!isCurrentRecordLogged())
        return;

    writeUnsignedInteger(value);
}

void BinarySink::write(int value)
{
    if (!isCurrentRecordLogged())
        return;

    writeSignedInteger(value);
}

void BinarySink::write(unsigned int value)
{
    if (!isCurrentRecordLogged())
        return;

    writeUnsignedInteger(value);
}

void BinarySink::write(long value)
{
    if (!isCurrentRecordLogged())
        return;

    writeSignedInteger(value);
}

void BinarySink::write(unsigned long value)
{
    if (!isCurrentRecordLogged())
        return;

    writeUnsignedInteger(value);
}

void BinarySink::write(long long value)
{
    if (!isCurrentRecordLogged())
        return;

    writeSignedInteger(value);
}

void BinarySink::write(unsigned long long value)
{
    if (!isCurrentRecordLogged())
        return;

    writeUnsignedInteger(value);
}

// -----------------------------------------------------------------------------
//     Floating point output
// -----------------------------------------------------------------------------

void BinarySink::write(float value)
{
    if (!isCurrentRecordLogged())
        return;

    writeByte(0xE0 + 8);
    writeBytes(reinterpret_cast<byte*>(&value), sizeof(value));
}

void BinarySink::write(double value)
{
    if (!isCurrentRecordLogged())
        return;

    writeByte(0xE0 + 9);
    writeBytes(reinterpret_cast<byte*>(&value), sizeof(value));
}

void BinarySink::write(long double value)
{
    if (!isCurrentRecordLogged())
        return;

    writeByte(0xE0 + 10);
    writeBytes(reinterpret_cast<byte*>(&value), sizeof(value));
}

// -----------------------------------------------------------------------------
//     Pointer output
// -----------------------------------------------------------------------------

void BinarySink::write(const void* ptr)
{
    if (!isCurrentRecordLogged())
        return;

    std::uintptr_t value = std::uintptr_t(ptr);

    if (value == 0)
    {
        writeByte(0xE0 + 2);
    }
    else if (value < std::uintptr_t(1) << 24)
    {
        writeByte(0xE0 + 16);
        writeBytes(reinterpret_cast<const byte*>(&value), 3);
    }
    else if (sizeof(value) == 4)
    {
        writeByte(0xE0 + 17);
        writeBytes(reinterpret_cast<const byte*>(&value), sizeof(value));
    }
    else
    {
        writeByte(0xE0 + 18);
        writeBytes(reinterpret_cast<const byte*>(&value), sizeof(value));
    }
}

// -----------------------------------------------------------------------------
//     String output
// -----------------------------------------------------------------------------

void BinarySink::write(Immutable<const char*> str,
                       std::uintptr_t immutableStringSpaceBegin)
{
    if (!isCurrentRecordLogged())
        return;

    std::uintptr_t value = std::uintptr_t(str.get()) - immutableStringSpaceBegin;

    if (str.get() == nullptr)
    {
        writeByte(0x40);
    }
    else if (value < std::uintptr_t(1) << 24)
    {
        writeByte(0xE0 + 20);
        writeBytes(reinterpret_cast<const byte*>(&value), 3);
    }
    else if (sizeof(value) == 4)
    {
        writeByte(0xE0 + 21);
        writeBytes(reinterpret_cast<const byte*>(&value), sizeof(value));
    }
    else
    {
        writeByte(0xE0 + 22);
        writeBytes(reinterpret_cast<const byte*>(&value), sizeof(value));
    }
}

void BinarySink::write(const SplitStringView& str)
{
    if (!isCurrentRecordLogged())
        return;

    auto totalSize = str.length1 + str.length2;
    if (totalSize < 30)
    {
        writeByte(0x40 + totalSize);
    }
    else if (totalSize < 256)
    {
        writeByte(0x40 + 30);
        writeByte(totalSize);
    }
    else
    {
        writeByte(0x40 + 31);
        writeByte(totalSize);
        writeByte(totalSize >> 8);
    }

    if (str.length1)
        writeBytes(reinterpret_cast<const byte*>(str.begin1), str.length1);
    if (str.length2)
        writeBytes(reinterpret_cast<const byte*>(str.begin2), str.length2);
}

// -----------------------------------------------------------------------------
//     User-defined types output
// -----------------------------------------------------------------------------

void BinarySink::beginFormatTuple()
{
    if (!isCurrentRecordLogged())
        return;

    writeByte(0x60 + 16);
}

void BinarySink::endFormatTuple()
{
    if (!isCurrentRecordLogged())
        return;

    writeByte(0xE0 + 31);
}

void BinarySink::beginStruct(std::uint32_t tag)
{
    if (!isCurrentRecordLogged())
        return;

    if (tag < std::uint32_t(1) << 8)
    {
        writeByte(0x60 + 0);
        writeByte(tag);
    }
    else if (tag < std::uint32_t(1) << 16)
    {
        writeByte(0x60 + 1);
        writeByte(tag >> 0);
        writeByte(tag >> 8);
    }
    else if (tag < std::uint32_t(1) << 24)
    {
        writeByte(0x60 + 2);
        writeByte(tag >>  0);
        writeByte(tag >>  8);
        writeByte(tag >> 16);
    }
    else
    {
        writeByte(0x60 + 3);
        writeByte(tag >>  0);
        writeByte(tag >>  8);
        writeByte(tag >> 16);
        writeByte(tag >> 24);
    }
}

void BinarySink::endStruct(std::uint32_t /*tag*/)
{
    if (!isCurrentRecordLogged())
        return;

    writeByte(0xE0 + 31);
}

void BinarySink::writeEnum(std::uint32_t tag, std::int64_t value)
{
    if (!isCurrentRecordLogged())
        return;

    if (tag < std::uint32_t(1) << 8)
    {
        writeByte(0x60 + 4);
        writeByte(tag);
    }
    else if (tag < std::uint32_t(1) << 16)
    {
        writeByte(0x60 + 5);
        writeByte(tag >> 0);
        writeByte(tag >> 8);
    }
    else if (tag < std::uint32_t(1) << 24)
    {
        writeByte(0x60 + 6);
        writeByte(tag >>  0);
        writeByte(tag >>  8);
        writeByte(tag >> 16);
    }
    else
    {
        writeByte(0x60 + 7);
        writeByte(tag >>  0);
        writeByte(tag >>  8);
        writeByte(tag >> 16);
        writeByte(tag >> 24);
    }
    writeSignedInteger(value);
}

// ----=====================================================================----
//     Protected methods
// ----=====================================================================----

void BinarySink::writeUnsignedInteger(std::uint64_t value, byte tag)
{
    byte buffer[9];
    unsigned idx = 1;

    if (value < 24)
    {
        writeByte(tag + value);
    }
    else
    {
        buffer[0] = tag + 23;
        while (value)
        {
            ++buffer[0];
            buffer[idx++] = value & 0xFF;
            value >>= 8;
        }
        writeBytes(&buffer[0], idx);
    }
}

void BinarySink::writeSignedInteger(std::int64_t value)
{
    if (value >= 0)
        writeUnsignedInteger(value, 0x00);
    else
        writeUnsignedInteger(~static_cast<std::uint64_t>(value), 0x20);
}

} // namespace log11
