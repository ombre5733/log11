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
#include <weos/thread.hpp>
#include <weos/type_traits.hpp>
#include <weos/utility.hpp>
#else
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <type_traits>
#include <utility>
#endif // LOG11_USE_WEOS

#include <cstddef>
#include <cstdint>
#include <new>

namespace log11
{

class Logger;
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

template <typename... T>
class LogStreamStatement;

template <>
class LogStreamStatement<>
{
public:
    LogStreamStatement(Logger* logger, Severity severity)
        : m_logger(logger),
          m_severity(static_cast<int>(severity))
    {
    }

    template <typename TArg>
    LogStreamStatement<typename LOG11_STD::decay<TArg>::type>
    operator<<(TArg&& arg)
    {
        using namespace LOG11_STD;
        return LogStreamStatement<typename decay<TArg>::type>(
                    *this, forward<TArg>(arg));
    }

private:
    Logger* m_logger;
    unsigned m_severity : 4;

    template <typename... U>
    friend class LogStreamStatement;
};

template <typename... T>
class LogStreamStatement
{
public:
    ~LogStreamStatement();

    template <typename TArg>
    LogStreamStatement<T..., typename LOG11_STD::decay<TArg>::type>
    operator<<(TArg&& arg)
    {
        using namespace LOG11_STD;
        m_active = false;
        return LogStreamStatement<T..., typename decay<TArg>::type>(
                    *this, forward<TArg>(arg),
                    make_integer_sequence<int, sizeof...(T)>());
    }

private:
    LOG11_STD::tuple<T...> m_data;
    Logger* m_logger;
    unsigned m_severity : 4;
    unsigned m_active : 1;


    template <typename T2>
    LogStreamStatement(const LogStreamStatement<>& stmt, T2&& tail)
        : m_data(tail),
          m_logger(stmt.m_logger),
          m_severity(stmt.m_severity),
          m_active(true)
    {
    }

    template <typename... T1, typename T2, int... TI>
    LogStreamStatement(const LogStreamStatement<T1...>& stmt,
                       T2&& tail,
                       LOG11_STD::integer_sequence<int, TI...>)
        : m_data(LOG11_STD::get<TI>(stmt.m_data)..., tail),
          m_logger(stmt.m_logger),
          m_severity(stmt.m_severity),
          m_active(true)
    {
    }

    template <typename... U>
    friend class LogStreamStatement;
};

} // namespace log11_detail


//! A tag type for log entries which may be discarded.
struct may_discard_t {};
//! A tag type for log entries which may be truncated.
struct may_truncate_t {};

//! This tag specifies that a log entry may be discarded if the FIFO is full.
constexpr may_discard_t may_discard = may_discard_t();
//! This tag specifies that a log entry may be truncated (or even discarded)
//! if there is no sufficient space in the FIFO.
constexpr may_truncate_t may_truncate = may_truncate_t();

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

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    //! Destroys the logger.
    ~Logger();

    //! Sets the \p severity filter.
    void setSeverityThreshold(Severity severity) noexcept;

    //! If \p enable is set, a new-line is automatically appended to every
    //! log statement. This is enabled by default.
    void setAutomaticNewLine(bool enable) noexcept;

    //! Sets the logger's \p sink.
    void setSink(Sink* sink) noexcept;

    //! Logs the \p message interpolated with the \p args using the specified
    //! \p severity level. The caller is blocked until there is sufficient
    //! space in the buffer.
    template <typename... TArgs>
    void log(Severity severity, const char* message, TArgs&&... args)
    {
        doLog(Block, severity, message, LOG11_STD::forward<TArgs>(args)...);
    }

    //! Logs the \p message interpolated with the \p args using the specified
    //! \p severity level. If the space in the buffer is too small, the
    //! log entry is discarded but the caller is not blocked.
    template <typename... TArgs>
    void log(may_discard_t, Severity severity, const char* message,
             TArgs&&... args)
    {
        doLog(Discard, severity, message, LOG11_STD::forward<TArgs>(args)...);
    }

    //! Logs the \p message interpolated with the \p args using the specified
    //! \p severity level. If the space in the buffer is too small, the
    //! log entry is truncated (or even discarded if no space is left at all)
    //! but the caller is not blocked.
    template <typename... TArgs>
    void log(may_truncate_t, Severity severity, const char* message,
             TArgs&&... args)
    {
        doLog(Truncate, severity, message, LOG11_STD::forward<TArgs>(args)...);
    }

    //! Creates a log stream with the given \p severity. The resulting object
    //! can be used to create a log entry using C++ streaming notation as in
    //! \code
    //! log(Severity::Error) << "The input" << someVar << "is too large";
    //! log(Severity::Debug) << "a:" << a << "b:" << b << "sum:" << a + b;
    //! \endcode
    log11_detail::LogStreamStatement<> log(Severity severity);

    // debug()

    //! \brief A convenience function for info log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(Severity::Debug, message, args...);
    //! \endcode
    template <typename... TArgs>
    void debug(const char* message, TArgs&&... args)
    {
        doLog(Block, Severity::Debug, message, LOG11_STD::forward<TArgs>(args)...);
    }

    //! \brief A convenience function for info log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(may_discard, Severity::Debug, message, args...);
    //! \endcode
    template <typename... TArgs>
    void debug(may_discard_t, const char* message, TArgs&&... args)
    {
        doLog(Discard, Severity::Debug, message, LOG11_STD::forward<TArgs>(args)...);
    }

    //! \brief A convenience function for info log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(may_truncate, Severity::Debug, message, args...);
    //! \endcode
    template <typename... TArgs>
    void debug(may_truncate_t, const char* message, TArgs&&... args)
    {
        doLog(Truncate, Severity::Debug, message, LOG11_STD::forward<TArgs>(args)...);
    }

    // info()

    //! \brief A convenience function for info log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(Severity::Info, message, args...);
    //! \endcode
    template <typename... TArgs>
    void info(const char* message, TArgs&&... args)
    {
        doLog(Block, Severity::Info, message, LOG11_STD::forward<TArgs>(args)...);
    }

    //! \brief A convenience function for info log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(may_discard, Severity::Info, message, args...);
    //! \endcode
    template <typename... TArgs>
    void info(may_discard_t, const char* message, TArgs&&... args)
    {
        doLog(Discard, Severity::Info, message, LOG11_STD::forward<TArgs>(args)...);
    }

    //! \brief A convenience function for info log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(may_truncate, Severity::Info, message, args...);
    //! \endcode
    template <typename... TArgs>
    void info(may_truncate_t, const char* message, TArgs&&... args)
    {
        doLog(Truncate, Severity::Info, message, LOG11_STD::forward<TArgs>(args)...);
    }

    // warn()

    //! \brief A convenience function for warning log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(Severity::Warn, message, args...);
    //! \endcode
    template <typename... TArgs>
    void warn(const char* message, TArgs&&... args)
    {
        doLog(Block, Severity::Warn, message, LOG11_STD::forward<TArgs>(args)...);
    }

    //! \brief A convenience function for warning log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(may_discard, Severity::Warn, message, args...);
    //! \endcode
    template <typename... TArgs>
    void warn(may_discard_t, const char* message, TArgs&&... args)
    {
        doLog(Discard, Severity::Warn, message, LOG11_STD::forward<TArgs>(args)...);
    }

    //! \brief A convenience function for warning log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(may_truncate, Severity::Warn, message, args...);
    //! \endcode
    template <typename... TArgs>
    void warn(may_truncate_t, const char* message, TArgs&&... args)
    {
        doLog(Truncate, Severity::Warn, message, LOG11_STD::forward<TArgs>(args)...);
    }

    // error()

    //! \brief A convenience function for error log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(Severity::Error, message, args...);
    //! \endcode
    template <typename... TArgs>
    void error(const char* message, TArgs&&... args)
    {
        doLog(Block, Severity::Error, message, LOG11_STD::forward<TArgs>(args)...);
    }

    //! \brief A convenience function for error log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(may_discard, Severity::Error, message, args...);
    //! \endcode
    template <typename... TArgs>
    void error(may_discard_t, const char* message, TArgs&&... args)
    {
        doLog(Discard, Severity::Error, message, LOG11_STD::forward<TArgs>(args)...);
    }

    //! \brief A convenience function for error log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(may_truncate, Severity::Error, message, args...);
    //! \endcode
    template <typename... TArgs>
    void error(may_truncate_t, const char* message, TArgs&&... args)
    {
        doLog(Truncate, Severity::Error, message, LOG11_STD::forward<TArgs>(args)...);
    }

private:
    enum Flags
    {
        StopRequest   = 0x01,
        AppendNewLine = 0x02
    };

    RingBuffer m_messageFifo;
    LOG11_STD::atomic_int m_flags;
    LOG11_STD::atomic<Sink*> m_sink;
    LOG11_STD::atomic<Severity> m_severityThreshold;

    char m_conversionBuffer[32];


    void doLog(ClaimPolicy policy, Severity severity, const char* message);

    template <typename TArg, typename... TArgs>
    void doLog(ClaimPolicy policy, Severity severity, const char* format,
               TArg&& arg, TArgs&&... args);

    template <typename... TArgs, int... TIndices>
    void doLogStream(ClaimPolicy policy, Severity severity,
                     const LOG11_STD::tuple<TArgs...>& stream,
                     LOG11_STD::integer_sequence<int, TIndices...>);

    void consumeFifoEntries();
    void printHeader(log11_detail::LogStatement* stmt);


    friend class Converter;

    template <typename... U>
    friend class log11_detail::LogStreamStatement;
};

template <typename TArg, typename... TArgs>
void Logger::doLog(ClaimPolicy policy, Severity severity, const char* format,
                   TArg&& arg, TArgs&&... args)
{
    using namespace LOG11_STD;
    using namespace log11_detail;

    static_assert(all_serializable<typename decay<TArg>::type,
                                   typename decay<TArgs>::type...>::value,
                  "Unsuitable type for string interpolation");

    if (severity < m_severityThreshold)
        return;

    auto serdes = Serdes<void*,
                         typename decay<TArg>::type,
                         typename decay<TArgs>::type...>::instance();
    auto argSlots = (serdes->requiredSize(nullptr, arg, args...)
                     + sizeof(LogStatement) - 1) / sizeof(LogStatement);

    auto claimed = policy == Block ? m_messageFifo.claim(1 + argSlots)
                                   : m_messageFifo.tryClaim(1 + argSlots,
                                                            policy == Truncate);
    if (claimed.length == 0)
        return;

    auto stmt = new (m_messageFifo[claimed.begin]) LogStatement(severity, format);
    stmt->m_extensionType = 1;
    if (claimed.length > 1)
    {
        argSlots = claimed.length - 1;
        stmt->m_extensionSize = argSlots;
        serdes->serialize(
                m_messageFifo,
                m_messageFifo.byteRange(
                        RingBuffer::Range(claimed.begin + 1, argSlots)),
                serdes, arg, args...);
    }

    if (policy == Block)
        m_messageFifo.publish(claimed);
    else
        m_messageFifo.tryPublish(claimed);
}

template <typename... TArgs, int... TIndices>
void Logger::doLogStream(ClaimPolicy policy, Severity severity,
                         const LOG11_STD::tuple<TArgs...>& stream,
                         LOG11_STD::integer_sequence<int, TIndices...>)
{
    using namespace LOG11_STD;
    using namespace log11_detail;

    static_assert(sizeof...(TArgs) > 0, "Cannot log an empty stream");
    static_assert(all_serializable<typename decay<TArgs>::type...>::value,
                  "Unsuitable type for string interpolation");

    if (severity < m_severityThreshold)
        return;

    auto serdes = Serdes<void*,
                         typename decay<TArgs>::type...>::instance();
    auto argSlots = (serdes->requiredSize(nullptr, get<TIndices>(stream)...)
                     + sizeof(LogStatement) - 1) / sizeof(LogStatement);

    auto claimed = policy == Block ? m_messageFifo.claim(1 + argSlots)
                                   : m_messageFifo.tryClaim(1 + argSlots,
                                                            policy == Truncate);
    if (claimed.length == 0)
        return;

    auto stmt = new (m_messageFifo[claimed.begin]) LogStatement(severity, nullptr);
    stmt->m_extensionType = 2;
    if (claimed.length > 1)
    {
        argSlots = claimed.length - 1;
        stmt->m_extensionSize = argSlots;
        serdes->serialize(
                    m_messageFifo,
                    m_messageFifo.byteRange(
                            RingBuffer::Range(claimed.begin + 1, argSlots)),
                    serdes, get<TIndices>(stream)...);
    }

    if (policy == Block)
        m_messageFifo.publish(claimed);
    else
        m_messageFifo.tryPublish(claimed);
}

namespace log11_detail
{

template <typename... T>
LogStreamStatement<T...>::~LogStreamStatement()
{
    if (!m_active)
        return;

    m_logger->doLogStream(
                Logger::Block, static_cast<Severity>(m_severity),
                m_data, LOG11_STD::make_integer_sequence<int, sizeof...(T)>());
}

} // namespace log11_detail

} // namespace log11

#endif // LOG11_LOGGER_HPP
