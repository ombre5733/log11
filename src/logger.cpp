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
#include "sink.hpp"

#include <cstdint>
#include <cstring>

#ifdef LOG11_USE_WEOS
#include <weos/thread.hpp>
#else
#include <thread>
#endif // LOG11_USE_WEOS


using namespace std;

namespace log11
{

class Converter : public Visitor
{
public:
    Converter(Logger& logger)
        : m_logger(logger)
    {
    }

    virtual
    void visit(int value) override
    {
        Sink* sink = m_logger.m_sink;
        if (sink)
        {
            auto length = snprintf(m_logger.m_conversionBuffer, sizeof(m_logger.m_conversionBuffer),
                                   "%d", value);
            sink->putString(m_logger.m_conversionBuffer, length);
        }
    }

    virtual
    void visit(float value) override
    {
        Sink* sink = m_logger.m_sink;
        if (sink)
        {
            auto length = snprintf(m_logger.m_conversionBuffer, sizeof(m_logger.m_conversionBuffer),
                                   "%f", value);
            sink->putString(m_logger.m_conversionBuffer, length);
        }
    }

    virtual
    void visit(const void* value) override
    {
        Sink* sink = m_logger.m_sink;
        if (sink)
        {
            auto length = snprintf(m_logger.m_conversionBuffer, sizeof(m_logger.m_conversionBuffer),
                                   "0x%08x", uintptr_t(value));
            sink->putString(m_logger.m_conversionBuffer, length);
        }
    }

    virtual
    void visit(const char* value) override
    {
        Sink* sink = m_logger.m_sink;
        if (sink)
            sink->putString(value, std::strlen(value));
    }

private:
    Logger& m_logger;
};

LogStatement::LogStatement(Severity severity, const char* msg)
    : m_timeStamp(LOG11_STD::chrono::high_resolution_clock::now().time_since_epoch().count()),
      m_message(msg),
      m_extensionSize(0),
      m_extensionType(0),
      m_severity(static_cast<int>(severity))
{
}


#ifdef LOG11_USE_WEOS
Logger::Logger(const weos::thread_attributes& attrs)
#else
Logger::Logger()
#endif // LOG11_USE_WEOS
    : m_messageFifo(sizeof(LogStatement), 100),
      m_stop(false),
      m_sink{nullptr}
{
#ifdef LOG11_USE_WEOS
    weos::thread(attrs, &Logger::consumeFifoEntries, this).detach();
#else
    std::thread(&Logger::consumeFifoEntries, this).detach();
#endif // LOG11_USE_WEOS
}

Logger::~Logger()
{
    m_stop = true;
    // Signal the consumer thread.
    auto claimed = m_messageFifo.claim(1);
    new (m_messageFifo[claimed.begin]) LogStatement(Severity::Debug, nullptr);
    m_messageFifo.publish(claimed);
}

void Logger::setSink(Sink* sink)
{
    m_sink = sink;
}

void Logger::log(Severity severity, const char* message)
{
    auto claimed = m_messageFifo.claim(1);
    new (m_messageFifo[claimed.begin]) LogStatement(severity, message);
    m_messageFifo.publish(claimed);
}

void Logger::consumeFifoEntries()
{
    Converter converter(*this);

    while (!m_stop)
    {
        auto available = m_messageFifo.available();
        while (!m_stop && available.length)
        {
            // If no sink is attached to the logger, consume all FIFO entries.
            Sink* sink = m_sink;
            if (!sink)
            {
                m_messageFifo.consume(available.length);
                break;
            }

            auto stmt = static_cast<LogStatement*>(m_messageFifo[available.begin]);
            if (!stmt->m_message)
                return;

            printHeader(stmt);

            if (stmt->m_extensionType == 0)
            {
                sink->putString(stmt->m_message, std::strlen(stmt->m_message));
                sink->putChar('\n');

                ++available.begin;
                --available.length;
                m_messageFifo.consume(1);
            }
            else
            {
                unsigned extensionLength = (stmt->m_extensionSize + sizeof(LogStatement) - 1)
                                          / sizeof(LogStatement);
                SerdesBase* serdes = *static_cast<SerdesBase**>(m_messageFifo[available.begin + 1]);
                auto byteRange = m_messageFifo.byteRange(
                                     RingBuffer::Range(available.begin + 1, extensionLength));

                // Interpret the format string.
                unsigned argCounter = 0;
                const char* firstChar = stmt->m_message;
                const char* iter = firstChar;
                for (; *iter; ++iter)
                {
                    if (*iter == '{')
                    {
                        if (iter != firstChar)
                            sink->putString(firstChar, iter - firstChar);

                        firstChar = iter;
                        while (*iter && *iter != '}')
                            ++iter;
                        if (!*iter++)
                        {
                            firstChar = iter;
                            break;
                        }

                        ++argCounter;
                        if (argCounter < serdes->numArguments())
                            serdes->apply(m_messageFifo, byteRange, argCounter, converter);
                        else
                            sink->putString("?!", 2);

                        firstChar = iter;
                    }
                }
                if (iter != firstChar)
                    sink->putString(firstChar, iter - firstChar);

                sink->putChar('\n');

                available.begin += 1 + extensionLength;
                available.length -= 1 + extensionLength;
                m_messageFifo.consume(1 + extensionLength);
            }
        }
    }
}


int numDecimalDigits(uint64_t x)
{
    unsigned digits = 1;
    while (true)
    {
        if (x < 10)
            return digits;
        if (x < 100)
            return digits + 1;
        if (x < 1000)
            return digits + 2;
        if (x < 10000)
            return digits + 3;
        x /= 10000;
        digits += 4;
    }
    return digits;
}

template <typename T>
void printDecimal(T x, char* buffer, unsigned numDigits)
{
    buffer += numDigits;
    while (numDigits--)
    {
        *--buffer = static_cast<char>('0' + (x % 10));
        x /= 10;
    }
}

void Logger::printHeader(LogStatement* stmt)
{
    static const char* severity_texts[] = {
        "TRACE",
        "DEBUG",
        "INFO ",
        "WARN ",
        "ERROR"
    };

    using namespace LOG11_STD::chrono;

    Sink* sink = m_sink;
    if (!sink)
        return;

    auto t = duration_cast<microseconds>(
                 high_resolution_clock::duration(stmt->m_timeStamp)).count();

    auto secs = t / 1000000;
    auto mins = secs / 60;
    auto hours = mins / 60;
    unsigned days = (hours / 24) % 10000;

    memset(m_conversionBuffer, ' ', 29);
    m_conversionBuffer[0] = '[';
    m_conversionBuffer[27] = ']';
    m_conversionBuffer[8] = ':';
    m_conversionBuffer[11] = ':';
    m_conversionBuffer[14] = '.';
    auto digits = numDecimalDigits(days);
    printDecimal(days, m_conversionBuffer + 5 - digits, digits);
    printDecimal(unsigned(hours % 24), m_conversionBuffer + 6, 2);
    printDecimal(unsigned(mins % 60), m_conversionBuffer + 9, 2);
    printDecimal(unsigned(secs % 60), m_conversionBuffer + 12, 2);
    printDecimal(unsigned(t % 1000000), m_conversionBuffer + 15, 6);
    memcpy(m_conversionBuffer + 22, severity_texts[stmt->m_severity], 5);

    sink->putString(m_conversionBuffer, 29);
}

} // namespace log11
