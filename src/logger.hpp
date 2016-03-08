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

#ifndef LOG11_LOGGER_HPP
#define LOG11_LOGGER_HPP

#include "ringbuffer.hpp"
#include "serdes.hpp"

#include <atomic>
#include <chrono>
#include <cstdint>
#include <new>
#include <type_traits>


class Sink;

struct LogStatement
{
    LogStatement(const char* msg);

    std::int64_t m_timeStamp;
    const char* m_message;
    unsigned m_extensionSize : 16;
    unsigned m_extensionType : 2;
};

class Logger
{
public:
    Logger();
    ~Logger();

    void setSink(Sink* sink);

    void log(const char* message);

    template <typename TArg, typename... TArgs>
    void log(const char* format, TArg&& arg, TArgs&&... args);

private:
    RingBuffer m_messageFifo;
    std::atomic_bool m_stop;
    std::atomic<Sink*> m_sink;

    char m_conversionBuffer[32];

    void consumeFifoEntries();

    friend class Converter;
};

template <typename TArg, typename... TArgs>
void Logger::log(const char* format, TArg&& arg, TArgs&&... args)
{
    auto serdes = Serdes<void*, typename std::decay<TArg>::type,
                         typename std::decay<TArgs>::type...>::instance();
    auto argSize = serdes->requiredSize(nullptr, arg, args...);

    auto claimed = m_messageFifo.claim(
                       1 + (argSize + sizeof(LogStatement) - 1) / sizeof(LogStatement));
    auto stmt = new (m_messageFifo[claimed.begin]) LogStatement(format);
    stmt->m_extensionType = 1;
    stmt->m_extensionSize = argSize;
    serdes->serialize(m_messageFifo,
                      m_messageFifo.byteRange(RingBuffer::Range(claimed.begin + 1, claimed.length - 1)),
                      serdes, arg, args...);
    m_messageFifo.publish(claimed);
}

#endif // LOG11_LOGGER_HPP
