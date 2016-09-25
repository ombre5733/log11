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
#include "BinarySink.hpp"
#include "TextSink.hpp"

#include <chrono>


namespace log11
{

namespace log11_detail
{

LogStatement::LogStatement(Severity s)
    : timeStamp(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    , severity(static_cast<int>(s))
{
}

} // namespace log11_detail


using namespace log11_detail;


LogCore::LogCore(unsigned exponent, TextSink* textSink)
    : m_messageFifo(exponent),
      m_textSink(textSink)
{
#ifdef LOG11_USE_WEOS
    m_consumerThread = weos::thread(attrs, &LogCore::consumeFifoEntries, this);
#else
    m_consumerThread = std::thread(&LogCore::consumeFifoEntries, this);
#endif // LOG11_USE_WEOS
}

LogCore::~LogCore()
{
    // TODO
    //m_consumerThread.join();
    m_consumerThread.detach();
}

void LogCore::consumeFifoEntries()
{
    struct AutoConsumer
    {
        AutoConsumer(RingBuffer& buffer, RingBuffer::Block& block)
            : m_buffer(buffer),
              m_block(block)
        {
        }

        ~AutoConsumer()
        {
            m_buffer.consume(m_block);
        }

        RingBuffer& m_buffer;
        RingBuffer::Block& m_block;
    };

    constexpr int StopRequest = 1;

    while ((m_flags & StopRequest) == 0)
    {
        auto block = m_messageFifo.wait();
        AutoConsumer consumer(m_messageFifo, block);
        if (m_flags & StopRequest)
            return;

        // Deserialize the header.
        auto stream = block.stream(m_messageFifo);
        LogStatement stmt;
        stream.read(&stmt, sizeof(LogStatement));

        TextSink* textSink = m_textSink;
        if (textSink)
        {
            textSink->beginLogEntry(static_cast<Severity>(stmt.severity));
            writeText(textSink, stream);
            textSink->endLogEntry();
        }
    }
}

void LogCore::writeText(TextSink* sink, RingBuffer::Stream inStream)
{
    const char* fmt;
    if (!inStream.read(&fmt, sizeof(const char*)))
        return;

    TextStream outStream(*sink);
    unsigned argCounter = 0;
    const char* marker = fmt;
    for (; *fmt; ++fmt)
    {
        if (*fmt != '{')
            continue;

        if (fmt != marker)
            sink->putString(marker, fmt - marker);

        // Loop to the end of the format specifier (or the end of the string).
        marker = ++fmt;
        while (*fmt && *fmt != '}')
            ++fmt;
        if (!*fmt)
        {
            marker = fmt;
            break;
        }

        // TODO:
        // if (argCounter != argument.from.format.spec)
        // skipToArgument()


        log11_detail::SerdesBase* serdes;
        if (inStream.read(&serdes, sizeof(void*)))
        {
            serdes->deserialize(inStream, outStream);
        }
        else
        {
            // TODO: Output a marker
        }

        ++argCounter;
        marker = fmt + 1;
    }
    if (fmt != marker)
        sink->putString(marker, fmt - marker);

    // TODO: Output rest of arguments
}

void LogCore::writeBinary(RingBuffer::Stream inStream)
{
#if 0
    while (range.length > sizeof(void*))
    {
        log11_detail::SerdesBase* serdes;
        range = m_messageFifo.read(range, &serdes, sizeof(void*));
        range = serdes->deserialize(m_messageFifo, range, stream);
    }
#endif
}

} // namespace log11
