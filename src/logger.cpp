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

#include "logger.hpp"

#include <chrono>
#include <cstdint>
#include <thread>


struct LogStatement
{
    LogStatement(const char* msg)
        : m_timeStamp(std::chrono::high_resolution_clock::now().time_since_epoch().count()),
          m_message(msg)
    {
    }

    std::int64_t m_timeStamp;
    const char* m_message;
};


Logger::Logger()
    : m_messageFifo(16, 100),
      m_stop(false)
{
    std::thread(&Logger::doLog, this).detach();
}

Logger::~Logger()
{
    m_stop = true;
}

void Logger::log(const char* message)
{
    auto claimed = m_messageFifo.claim(1);
    new (m_messageFifo[claimed.begin]) LogStatement(message);
    m_messageFifo.publish(claimed);
}


void Logger::doLog()
{
    using namespace std::chrono;

    while (!m_stop)
    {
        // TODO: add a condition variable here

        auto available = m_messageFifo.available();
        if (available.length == 0)
            continue;

        auto stmt = static_cast<LogStatement*>(m_messageFifo[available.begin]);

        auto t = duration_cast<microseconds>(
                                high_resolution_clock::duration(stmt->m_timeStamp)).count();

        auto secs = t / 1000000;
        auto mins = secs / 60;
        auto hours = mins / 60;
        auto days = hours / 24;

        printf("[%4d %02d:%02d:%02d.%06d] %s\n", int(days), int(hours % 24), int(mins % 60), int(secs % 60),
               int(t % 1000000),
               stmt->m_message);

        m_messageFifo.consumeTo(available.begin);
    }
}
