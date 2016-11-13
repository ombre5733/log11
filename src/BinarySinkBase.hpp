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

#ifndef LOG11_BINARYSINKBASE_HPP
#define LOG11_BINARYSINKBASE_HPP

#include "LogRecordData.hpp"
#include "Severity.hpp"
#include "SinkBase.hpp"
#include "Utility.hpp"

#include <atomic>
#include <cstddef>


namespace log11
{

//! \brief The base class for all binary logger sinks.
//!
//! The BinarySinkBase is the base class for all binary logger sinks.
class BinarySinkBase : public SinkBase
{
public:
    using byte = std::uint8_t; // TODO: std::byte



    virtual
    void write(bool value) = 0;



    virtual
    void write(char ch) = 0;



    virtual
    void write(signed char value) = 0;

    virtual
    void write(unsigned char value) = 0;

    virtual
    void write(short value) = 0;

    virtual
    void write(unsigned short value) = 0;

    virtual
    void write(int value) = 0;

    virtual
    void write(unsigned int value) = 0;

    virtual
    void write(long value) = 0;

    virtual
    void write(unsigned long value) = 0;

    virtual
    void write(long long value) = 0;

    virtual
    void write(unsigned long long value) = 0;



    virtual
    void write(float value) = 0;

    virtual
    void write(double value) = 0;

    virtual
    void write(long double value) = 0;



    virtual
    void write(const void* value) = 0;



    virtual
    void write(Immutable<const char*> str,
               std::uintptr_t immutableStringSpaceBegin) = 0;

    virtual
    void write(const SplitStringView& str) = 0;



    virtual
    void beginFormatTuple() = 0;

    virtual
    void endFormatTuple() = 0;

    virtual
    void beginStruct(std::uint32_t tag) = 0;

    virtual
    void endStruct(std::uint32_t tag) = 0;

    virtual
    void writeEnum(std::uint32_t tag, std::int64_t value) = 0;
};

} // namespace log11

#endif // LOG11_BINARYSINKBASE_HPP
