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

#include "LogCore.hpp"

#include <chrono>


namespace log11
{

namespace log11_detail
{

LogStatement::LogStatement(Severity s, unsigned l)
    : timeStamp(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    , length(l)
    , severity(static_cast<int>(s))
{
}

} // namespace log11_detail


using namespace log11_detail;


void LogCore::consumeFifoEntries()
{
    constexpr int StopRequest = 1;

    while ((m_flags & StopRequest) == 0)
    {
        auto available = m_messageFifo.wait();
        while ((m_flags & StopRequest) == 0 && available.length)
        {
            // Deserialize the header.
            LogStatement stmt;
            m_messageFifo.read(available, &stmt, sizeof(LogStatement));

            RingBuffer::Range thisRange(available.begin, stmt.length);
            available.begin += stmt.length;
            available.length -= stmt.length;



            m_messageFifo.consume(stmt.length);
        }
    }
}

void LogCore::writeAsText(LogStatement& stmt)
{
    textSink->beginLogEntry(static_cast<Severity>(stmt.severity));

    unsigned argCounter = 0;
    const char* marker = fmt;
    for (; *fmt; ++fmt)
    {
        if (*fmt != '{')
            continue;

        if (fmt != marker)
            m_sink->putString(marker, fmt - marker);

        // Loop to the end of the format specifier (or the end of the string).
        marker = ++fmt;
        while (*fmt && *fmt != '}')
            ++fmt;
        if (!*fmt)
            return *this;

        // TODO:
        // if (argCounter != argument.from.format.spec)
        // skipToArgument()

        if (available.length >= sizeof(void*))
        {
            log11_detail::SerdesBase* serdes;
            buffer.read(range, &serdes, sizeof(void*));
            serdes->deserialize(buffer, range, stream);
        }
        else
        {
            // TODO: Output a marker
        }

        ++argCounter;
        marker = fmt + 1;
    }
    if (fmt != marker)
        m_sink->putString(marker, fmt - marker);

    // TODO: Output rest of arguments

    textSink->endLogEntry();
}

void LogCore::writeAsBinary(LogStatement& stmt, RingBuffer::Range range)
{
    while (range.length > sizeof(void*))
    {
        log11_detail::SerdesBase* serdes;
        range = m_messageFifo.read(range, &serdes, sizeof(void*));
        range = serdes->deserialize(m_messageFifo, range, stream);
    }
}

} // namespace log11
