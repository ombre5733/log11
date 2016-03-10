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

#include "config.hpp"
#include "ringbuffer.hpp"
#include "serdes.hpp"
#include "severity.hpp"

#ifdef LOG11_USE_WEOS
#include <weos/atomic.hpp>
#include <weos/chrono.hpp>
#include <weos/condition_variable.hpp>
#include <weos/type_traits.hpp>
#else
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <type_traits>
#endif // LOG11_USE_WEOS

#include <cstddef>
#include <cstdint>
#include <new>

namespace log11
{

class Sink;

namespace log11_detail
{

struct LogStatement
{
    LogStatement(Severity severity, const char* msg);

    LogStatement(const LogStatement&) = delete;
    LogStatement& operator=(const LogStatement&) = delete;

    std::int64_t m_timeStamp;
    const char* m_message;
    unsigned m_extensionSize : 16;
    unsigned m_extensionType : 2;
    unsigned m_severity : 4;
};

} // namespace log11_detail

class Logger
{
    enum ClaimPolicy
    {
        Block,    //!< Block caller until sufficient space is available
        Truncate, //!< Message will be truncated or even discarded
        Discard   //!< Message is displayed fully or discarded
    };

public:
#ifdef LOG11_USE_WEOS
    Logger(const weos::thread_attributes& attrs, std::size_t bufferSize);
#else
    Logger(std::size_t bufferSize);
#endif // LOG11_USE_WEOS

    //! Destroys the logger.
    ~Logger();

    //! Sets the \p severity filter.
    void setSeverityThreshold(Severity severity);
    void setSink(Sink* sink);

    //! Logs the \p message interpolated with the \p args using the specified
    //! \p severity level. The caller is blocked until there is sufficient
    //! space in the buffer.
    template <typename... TArgs>
    void log(Severity severity, const char* message, TArgs&&... args)
    {
        doLog(Block, severity, message, LOG11_STD::forward<TArgs>(args)...);
    }

private:
    RingBuffer m_messageFifo;
    LOG11_STD::atomic_bool m_stop;
    LOG11_STD::atomic<Sink*> m_sink;
    LOG11_STD::atomic<Severity> m_severityThreshold;

    char m_conversionBuffer[32];


    void doLog(ClaimPolicy policy, Severity severity, const char* message);

    template <typename TArg, typename... TArgs>
    void doLog(ClaimPolicy policy, Severity severity, const char* format,
               TArg&& arg, TArgs&&... args);

    void consumeFifoEntries();
    void printHeader(log11_detail::LogStatement* stmt);


    friend class Converter;
};

template <typename TArg, typename... TArgs>
void Logger::doLog(ClaimPolicy policy, Severity severity, const char* format,
                   TArg&& arg, TArgs&&... args)
{
    using namespace LOG11_STD;
    using namespace log11_detail;

    static_assert(all<is_serializable<typename decay<TArgs>::type>...>::value,
                  "Unsuitable type for string interpolation");

    if (severity < m_severityThreshold)
        return;

    auto serdes = Serdes<void*, typename LOG11_STD::decay<TArg>::type,
                         typename LOG11_STD::decay<TArgs>::type...>::instance();
    auto argSize = serdes->requiredSize(nullptr, arg, args...);
    auto numSlots = 1 + (argSize + sizeof(LogStatement) - 1) / sizeof(LogStatement);

    auto claimed = policy == Block ? m_messageFifo.claim(numSlots)
                                   : m_messageFifo.tryClaim(numSlots, policy == Truncate);
    if (claimed.length == 0)
        return;

    auto stmt = new (m_messageFifo[claimed.begin]) LogStatement(severity, format);
    stmt->m_extensionType = 1;
    if (claimed.length > 1)
    {
        if (claimed.length != numSlots)
            argSize = (claimed.length - 1) * sizeof(LogStatement);
        stmt->m_extensionSize = argSize;
        serdes->serialize(
                m_messageFifo,
                m_messageFifo.byteRange(
                    RingBuffer::Range(claimed.begin + 1, claimed.length - 1)),
                serdes, arg, args...);
    }
    m_messageFifo.publish(claimed);
}

} // namespace log11

#endif // LOG11_LOGGER_HPP
