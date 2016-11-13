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

#include "BinaryStream.hpp"
#include "Serdes.hpp"

#include <cstring>

using namespace std;


namespace log11
{

// ----=====================================================================----
//     BinaryStream
// ----=====================================================================----

BinaryStream::BinaryStream(BinarySinkBase& sink,
                           log11_detail::SerdesOptions& opt)
    : m_sink(&sink)
    , m_options(opt)
{
}

// -----------------------------------------------------------------------------
//     Bool & char output
// -----------------------------------------------------------------------------

void BinaryStream::write(bool value)
{
    m_sink->write(value);
}

void BinaryStream::write(char ch)
{
    m_sink->write(ch);
}

// -----------------------------------------------------------------------------
//     Integer output
// -----------------------------------------------------------------------------

void BinaryStream::write(signed char value)
{
    m_sink->write(value);
}

void BinaryStream::write(unsigned char value)
{
    m_sink->write(value);
}

void BinaryStream::write(short value)
{
    m_sink->write(value);
}

void BinaryStream::write(unsigned short value)
{
    m_sink->write(value);
}

void BinaryStream::write(int value)
{
    m_sink->write(value);
}

void BinaryStream::write(unsigned int value)
{
    m_sink->write(value);
}

void BinaryStream::write(long value)
{
    m_sink->write(value);
}

void BinaryStream::write(unsigned long value)
{
    m_sink->write(value);
}

void BinaryStream::write(long long value)
{
    m_sink->write(value);
}

void BinaryStream::write(unsigned long long value)
{
    m_sink->write(value);
}

// -----------------------------------------------------------------------------
//     Floating point output
// -----------------------------------------------------------------------------

void BinaryStream::write(float value)
{
    m_sink->write(value);
}

void BinaryStream::write(double value)
{
    m_sink->write(value);
}

void BinaryStream::write(long double value)
{
    m_sink->write(value);
}

// -----------------------------------------------------------------------------
//     Pointer output
// -----------------------------------------------------------------------------

void BinaryStream::write(const void* value)
{
    m_sink->write(value);
}

// -----------------------------------------------------------------------------
//     String output
// -----------------------------------------------------------------------------

void BinaryStream::write(const char* str)
{
    if (m_options.isImmutable(str))
        m_sink->write(Immutable<const char*>(str), m_options.immutableStringBegin);
    else
        m_sink->write(SplitStringView{str, str ? strlen(str) : 0, nullptr, 0});
}

void BinaryStream::write(Immutable<const char*> str)
{
    m_sink->write(str, m_options.immutableStringBegin);
}

void BinaryStream::write(const SplitStringView& str)
{
    m_sink->write(str);
}

} // namespace log11
