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

#ifndef LOG11_BINARYSINK_HPP
#define LOG11_BINARYSINK_HPP

#include "BinarySinkBase.hpp"

#include <cstddef>
#include <cstdint>


namespace log11
{

class BinarySink : public BinarySinkBase
{
public:
    enum class TypeTag
    {
        False         =  1,
        True          =  2,

        Char          =  3,

        Int8          =  4,
        Int16         =  5,
        Int32         =  6,
        Int64         =  7,

        Uint8         =  8,
        Uint16        =  9,
        Uint32        = 10,
        Uint64        = 11,

        VarInt16      = 12,
        VarInt32      = 13,
        VarInt64      = 14,

        VarUint16     = 15,
        VarUint32     = 16,
        VarUint64     = 17,

        Float         = 18,
        Double        = 19,
        LongDouble    = 20,

        Pointer       = 21,

        String        = 22,
        StringPointer = 23,
        StringView    = 24, // not used

        EndOfStruct   = 25
    };


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
    void beginStruct(std::uint32_t tag);

    virtual
    void endStruct(std::uint32_t tag);



    void writeTag(TypeTag tag);

    template <typename T>
    void writeInteger(T value);

    void writeVarInt(std::uint32_t value);

    void writeVarInt(std::int32_t value);

    void writeVarInt(std::uint64_t value);

    void writeVarInt(std::int64_t value);
};

} // namespace log11

#endif // LOG11_BINARYSINK_HPP
