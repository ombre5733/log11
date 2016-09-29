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

#include "BinaryStream.hpp"
#include "Serdes.hpp"

#include <cstring>

using namespace std;


namespace log11
{

// ----=====================================================================----
//     BinaryStream
// ----=====================================================================----

BinaryStream::BinaryStream(BinarySinkBase& sink, log11_detail::SerdesOptions& opt)
    : m_sink(&sink)
    , m_options(opt)
{
}

// -----------------------------------------------------------------------------
//     Bool & char output
// -----------------------------------------------------------------------------

BinaryStream& BinaryStream::operator<<(bool value)
{
    m_sink->write(value);
    return *this;
}

BinaryStream& BinaryStream::operator<<(char ch)
{
    m_sink->write(ch);
    return *this;
}

// -----------------------------------------------------------------------------
//     Integer output
// -----------------------------------------------------------------------------

BinaryStream& BinaryStream::operator<<(signed char value)
{
    m_sink->write(value);
    return *this;
}

BinaryStream& BinaryStream::operator<<(unsigned char value)
{
    m_sink->write(value);
    return *this;
}

BinaryStream& BinaryStream::operator<<(short value)
{
    m_sink->write(value);
    return *this;
}

BinaryStream& BinaryStream::operator<<(unsigned short value)
{
    m_sink->write(value);
    return *this;
}

BinaryStream& BinaryStream::operator<<(int value)
{
    m_sink->write(value);
    return *this;
}

BinaryStream& BinaryStream::operator<<(unsigned int value)
{
    m_sink->write(value);
    return *this;
}

BinaryStream& BinaryStream::operator<<(long value)
{
    m_sink->write(value);
    return *this;
}

BinaryStream& BinaryStream::operator<<(unsigned long value)
{
    m_sink->write(value);
    return *this;
}

BinaryStream& BinaryStream::operator<<(long long value)
{
    m_sink->write(value);
    return *this;
}

BinaryStream& BinaryStream::operator<<(unsigned long long value)
{
    m_sink->write(value);
    return *this;
}

// -----------------------------------------------------------------------------
//     Floating point output
// -----------------------------------------------------------------------------

BinaryStream& BinaryStream::operator<<(float value)
{
    m_sink->write(value);
    return *this;
}

BinaryStream& BinaryStream::operator<<(double value)
{
    m_sink->write(value);
    return *this;
}

BinaryStream& BinaryStream::operator<<(long double value)
{
    m_sink->write(value);
    return *this;
}

// -----------------------------------------------------------------------------
//     Pointer output
// -----------------------------------------------------------------------------

BinaryStream& BinaryStream::operator<<(const void* value)
{
    m_sink->write(value);
    return *this;
}

// -----------------------------------------------------------------------------
//     String output
// -----------------------------------------------------------------------------

BinaryStream& BinaryStream::operator<<(const char* str)
{
    if (m_options.isImmutable(str))
        m_sink->write(Immutable<const char*>(str), m_options.immutableStringBegin);
    else
        m_sink->write(SplitStringView{str, str ? strlen(str) : 0, nullptr, 0});
    return *this;
}

BinaryStream& BinaryStream::operator<<(Immutable<const char*> str)
{
    m_sink->write(str, m_options.immutableStringBegin);
    return *this;
}

BinaryStream& BinaryStream::operator<<(const SplitStringView& str)
{
    m_sink->write(str);
    return *this;
}

} // namespace log11
