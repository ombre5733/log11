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

#ifndef LOG11_BINARYSINK_HPP
#define LOG11_BINARYSINK_HPP

#include "BinarySinkBase.hpp"

#include <cstddef>
#include <cstdint>


namespace log11
{

// 0x00: 000x xxxx ... positive integer
// 0x20: 001x xxxx ... negative integer
//                     0-23 ... immediate
//                       24 ... +1 byte
//                       25 ... +2 byte
//                       26 ... +3 byte
//                       27 ... +4 byte
//                       28 ... +5 byte
//                       29 ... +6 byte
//                       30 ... +7 byte
//                       31 ... +8 byte
//
// 0x40: 010x xxxx ... string
//                     0-29 ... immediate size
//                       30 ... +1 byte
//                       31 ... +2 byte
//
// 0x60: 011x xxxx ... user-defined types
//                        0 ... struct with 1 byte ID
//                        1 ... struct with 2 byte ID
//                        2 ... struct with 3 byte ID
//                        3 ... struct with 4 byte ID
//                        4 ... enum with 1 byte ID
//                        5 ... enum with 2 byte ID
//                        6 ... enum with 3 byte ID
//                        7 ... enum with 4 byte ID
//                       16 ... format tuple begin
//
// 0x80: reserved
// 0xA0: reserved
// 0xC0: reserved
//
// 0xE0: 111x xxxx ... simple types
//                        0 ... false
//                        1 ... true
//                        2 ... null
//                        8 ... float
//                        9 ... double
//                       10 ... long double
//                       16 ... void* (3 byte)
//                       17 ... void* (4 byte)
//                       18 ... void* (8 byte)
//                       20 ... char* (3 byte)
//                       21 ... char* (4 byte)
//                       22 ... char* (8 byte)
//                       31 ... break
//
// TODO:
// - arrays
class BinarySink : public BinarySinkBase
{
public:
    virtual
    void writeByte(byte data) = 0;

    virtual
    void writeBytes(const byte* data, unsigned size);

protected:
    virtual
    void write(bool value) override;

    virtual
    void write(char ch) override;

    virtual
    void write(signed char value) override;

    virtual
    void write(unsigned char value) override;

    virtual
    void write(short value) override;

    virtual
    void write(unsigned short value) override;

    virtual
    void write(int value) override;

    virtual
    void write(unsigned int value) override;

    virtual
    void write(long value) override;

    virtual
    void write(unsigned long value) override;

    virtual
    void write(long long value) override;

    virtual
    void write(unsigned long long value) override;

    virtual
    void write(float value) override;

    virtual
    void write(double value) override;

    virtual
    void write(long double value) override;

    virtual
    void write(const void* value) override;

    virtual
    void write(Immutable<const char*> str,
               std::uintptr_t immutableStringSpaceBegin) override;

    virtual
    void write(const SplitStringView& str) override;

    virtual
    void beginFormatTuple() override;

    virtual
    void endFormatTuple() override;

    virtual
    void beginStruct(std::uint32_t tag) override;

    virtual
    void endStruct(std::uint32_t tag) override;

    virtual
    void writeEnum(std::uint32_t tag, std::int64_t value) override;



    void writeUnsignedInteger(std::uint64_t value, byte tag = 0x00);
    void writeSignedInteger(std::int64_t value);
};

} // namespace log11

#endif // LOG11_BINARYSINK_HPP
