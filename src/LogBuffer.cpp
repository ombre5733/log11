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

#include "LogBuffer.hpp"


using namespace std;


namespace log11
{

using namespace log11_detail;


LogBuffer::LogBuffer(LogCore* core,
                     LogCore::ClaimPolicy policy,
                     Severity severity,
                     std::size_t size)
    : m_core(core),
      m_severity(severity),
      m_policy(policy),
      m_hadEnoughSpace(true)
{
    m_claimed = m_core->claim(policy, size);
    m_stream = m_claimed.stream(m_core->m_messageFifo);
    m_stream.skip(LogCore::headerSize);
}

LogBuffer::LogBuffer(LogBuffer&& other) noexcept
    : m_core(other.m_core),
      m_severity(other.m_severity),
      m_policy(other.m_policy),
      m_hadEnoughSpace(other.m_hadEnoughSpace),
      m_claimed(other.m_claimed),
      m_stream(other.m_stream)
{
    other.m_core = nullptr;
}

LogBuffer& LogBuffer::operator=(LogBuffer&& other) noexcept
{
    m_core = other.m_core;
    m_severity = other.m_severity;
    m_policy = other.m_policy;
    m_hadEnoughSpace = other.m_hadEnoughSpace;
    m_claimed = other.m_claimed;
    m_stream = other.m_stream;
    other.m_core = nullptr;
    return *this;
}

LogBuffer::~LogBuffer()
{
    flush();
}

void LogBuffer::discard()
{
    if (!m_core)
        return;

    // Rewind the stream and write a directive to skip the entry.
    m_stream = m_claimed.stream(m_core->m_messageFifo);
    m_stream.write(Directive::command(Directive::Skip));
    if (m_policy == LogCore::ClaimPolicy::Block)
        m_core->m_messageFifo.publish(m_claimed);
    else
        m_core->m_messageFifo.tryPublish(m_claimed);
    m_core = nullptr;
}

void LogBuffer::flush()
{
    if (!m_core)
        return;

    // Write a terminator.
    SerdesBase* serdes = nullptr;
    m_stream.write(&serdes, sizeof(void*));

    // Rewind the stream and write the header.
    m_stream = m_claimed.stream(m_core->m_messageFifo);
    LogCore::writeRecordHeader(
                m_stream,
                Directive::entry(m_severity, !m_hadEnoughSpace));
    if (m_policy == LogCore::ClaimPolicy::Block)
        m_core->m_messageFifo.publish(m_claimed);
    else
        m_core->m_messageFifo.tryPublish(m_claimed);
    m_core = nullptr;
}

} // namespace log11
