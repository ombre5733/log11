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

#include "Logger.hpp"

using namespace std;

namespace log11
{

// ----=====================================================================----
//     Logger
// ----=====================================================================----

Logger::Logger(LogCore* core)
    : m_core(core),
      m_configuration(static_cast<unsigned char>(Severity::Info) | 0x80)
{
}

Logger::~Logger()
{
}

void Logger::setEnabled(bool enable) noexcept
{
    auto config = m_configuration.load() & 0x7F;
    if (enable)
        config |= 0x80;
    m_configuration = config;
}

bool Logger::isEnabled() const noexcept
{
    auto config = m_configuration.load();
    return (config & 0x80) != 0;
}

void Logger::setLevel(Severity threshold) noexcept
{
    auto config = m_configuration.load();
    m_configuration = static_cast<unsigned char>(threshold) | (config & 0x80);
}

Severity Logger::level() const noexcept
{
    return static_cast<Severity>(m_configuration.load() & 0x7F);
}

LogBuffer Logger::logBuffer(Severity severity, std::size_t size)
{
    return LogBuffer(m_core, LogCore::ClaimPolicy::Block, severity, size);
}

} // namespace log11
