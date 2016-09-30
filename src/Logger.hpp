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

#include "Config.hpp"
#include "LogCore.hpp"
#include "Severity.hpp"
#include "TypeInfo.hpp"


namespace log11
{

//! A tag type for log entries which may be discarded.
struct may_discard_t {};
//! A tag type for log entries which may be truncated.
struct may_truncate_or_discard_t {};

//! This tag specifies that a log entry may be discarded if the FIFO is full.
//! The message will either be logged as a whole or discarded.
constexpr may_discard_t may_discard = may_discard_t();

//! This tag specifies that a log entry may be truncated or discarded
//! if there is no sufficient space in the FIFO.
constexpr may_truncate_or_discard_t may_truncate_or_discard = may_truncate_or_discard_t();


//! \brief A logger.
//!
//! The Logger is the front-end of the logging facility.
class Logger
{
public:
#ifdef LOG11_USE_WEOS
    explicit
    Logger(const weos::thread_attributes& attrs, std::size_t bufferSizeExponent);
#else
    explicit
    Logger(std::size_t bufferSizeExponent);
#endif // LOG11_USE_WEOS

    //! Creates a new logger which attaches to a given core.
    explicit
    Logger(LogCore* core);

    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;

    //! Destroys the logger.
    ~Logger();

    //! \brief Sets the logging level.
    //!
    //! Sets the logging level to the given \p threshold. Log messages with
    //! a severity lower than the threshold are discarded.
    void setLevel(Severity threshold) noexcept;

    //! If the level of this logger is lower or equal to the \p severity of
    //! the message, a new log entry is created by interpolating the
    //! \p message with the given \p args.
    //! The caller is blocked until there is sufficient space in the FIFO,
    //! which connects the logger front-ends to the sinks.
    template <typename... TArgs>
    void log(Severity severity, const char* message, TArgs&&... args)
    {
        if (severity >= m_severityThreshold)
        {
            m_core->log(LogCore::ClaimPolicy::Block, severity,
                        message, log11_detail::decayArgument(args)...);
        }
    }

    //! If the level of this logger is lower or equal to the \p severity of
    //! the message, a new log entry is created by interpolating the
    //! \p message with the given \p args.
    //! If there is not sufficient space in the FIFO, which connects the logger
    //! front-ends to the sinks, the entire message is discarded but the
    //! caller is not blocked.
    template <typename... TArgs>
    void log(may_discard_t, Severity severity, const char* message,
             TArgs&&... args)
    {
        if (severity >= m_severityThreshold)
        {
            m_core->log(LogCore::ClaimPolicy::Discard, severity,
                        message, log11_detail::decayArgument(args)...);
        }
    }

    //! If the level of this logger is lower or equal to the \p severity of
    //! the message, a new log entry is created by interpolating the
    //! \p message with the given \p args.
    //! If there is not sufficient space in the FIFO, which connects the logger
    //! front-ends to the sinks, the message may be either truncated or even
    //! discarded but the caller is not blocked.
    template <typename... TArgs>
    void log(may_truncate_or_discard_t, Severity severity, const char* message,
             TArgs&&... args)
    {
        if (severity >= m_severityThreshold)
        {
            m_core->log(LogCore::ClaimPolicy::Truncate, severity,
                        message, log11_detail::decayArgument(args)...);
        }
    }

    // //! Creates a log stream with the given \p severity. The resulting object
    // //! can be used to create a log entry using C++ stream notation as in
    // //! \code
    // //! log(Severity::Error) << "The input" << someVar << "is too large";
    // //! log(Severity::Debug) << "a:" << a << "b:" << b << "sum:" << a + b;
    // //! \endcode
    // log11_detail::LogStreamStatement<> log(Severity severity);

    // trace()

    //! \brief A convenience function for trace log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(Severity::Trace, message, args...);
    //! \endcode
    template <typename... TArgs>
    void trace(const char* message, TArgs&&... args)
    {
        if (Severity::Trace >= m_severityThreshold)
        {
            m_core->log(LogCore::ClaimPolicy::Block, Severity::Trace,
                        message, log11_detail::decayArgument(args)...);
        }
    }

    //! \brief A convenience function for trace log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(may_discard, Severity::Trace, message, args...);
    //! \endcode
    template <typename... TArgs>
    void trace(may_discard_t, const char* message, TArgs&&... args)
    {
        if (Severity::Trace >= m_severityThreshold)
        {
            m_core->log(LogCore::ClaimPolicy::Discard, Severity::Trace,
                        message, log11_detail::decayArgument(args)...);
        }
    }

    //! \brief A convenience function for trace log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(may_truncate_or_discard, Severity::Trace, message, args...);
    //! \endcode
    template <typename... TArgs>
    void trace(may_truncate_or_discard_t, const char* message, TArgs&&... args)
    {
        if (Severity::Trace >= m_severityThreshold)
        {
            m_core->log(LogCore::ClaimPolicy::Truncate, Severity::Trace,
                        message, log11_detail::decayArgument(args)...);
        }
    }

    // //! \brief A convenience function for trace streams.
    // //!
    // //! Creates a trace stream equivalent to
    // //! \code
    // //! log(Severity::Trace)
    // //! \endcode
    // log11_detail::LogStreamStatement<> trace()
    // {
    //     return log(Severity::Trace);
    // }

    // debug()

    //! \brief A convenience function for debug log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(Severity::Debug, message, args...);
    //! \endcode
    template <typename... TArgs>
    void debug(const char* message, TArgs&&... args)
    {
        if (Severity::Debug >= m_severityThreshold)
        {
            m_core->log(LogCore::ClaimPolicy::Block, Severity::Debug,
                        message, log11_detail::decayArgument(args)...);
        }
    }

    //! \brief A convenience function for debug log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(may_discard, Severity::Debug, message, args...);
    //! \endcode
    template <typename... TArgs>
    void debug(may_discard_t, const char* message, TArgs&&... args)
    {
        if (Severity::Debug >= m_severityThreshold)
        {
            m_core->log(LogCore::ClaimPolicy::Discard, Severity::Debug,
                        message, log11_detail::decayArgument(args)...);
        }
    }

    //! \brief A convenience function for debug log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(may_truncate_or_discard, Severity::Debug, message, args...);
    //! \endcode
    template <typename... TArgs>
    void debug(may_truncate_or_discard_t, const char* message, TArgs&&... args)
    {
        if (Severity::Debug >= m_severityThreshold)
        {
            m_core->log(LogCore::ClaimPolicy::Truncate, Severity::Debug,
                        message, log11_detail::decayArgument(args)...);
        }
    }

    // //! \brief A convenience function for debug streams.
    // //!
    // //! Creates a debug stream equivalent to
    // //! \code
    // //! log(Severity::Debug)
    // //! \endcode
    // log11_detail::LogStreamStatement<> debug()
    // {
    //     return log(Severity::Debug);
    // }

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
        if (Severity::Info >= m_severityThreshold)
        {
            m_core->log(LogCore::ClaimPolicy::Block, Severity::Info,
                        message, log11_detail::decayArgument(args)...);
        }
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
        if (Severity::Info >= m_severityThreshold)
        {
            m_core->log(LogCore::ClaimPolicy::Discard, Severity::Info,
                        message, log11_detail::decayArgument(args)...);
        }
    }

    //! \brief A convenience function for info log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(may_truncate_or_discard, Severity::Info, message, args...);
    //! \endcode
    template <typename... TArgs>
    void info(may_truncate_or_discard_t, const char* message, TArgs&&... args)
    {
        if (Severity::Info >= m_severityThreshold)
        {
            m_core->log(LogCore::ClaimPolicy::Truncate, Severity::Info,
                        message, log11_detail::decayArgument(args)...);
        }
    }

    // //! \brief A convenience function for info streams.
    // //!
    // //! Creates an info stream equivalent to
    // //! \code
    // //! log(Severity::Info)
    // //! \endcode
    // log11_detail::LogStreamStatement<> info()
    // {
    //     return log(Severity::Info);
    // }

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
        if (Severity::Warn >= m_severityThreshold)
        {
            m_core->log(LogCore::ClaimPolicy::Block, Severity::Warn,
                        message, log11_detail::decayArgument(args)...);
        }
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
        if (Severity::Warn >= m_severityThreshold)
        {
            m_core->log(LogCore::ClaimPolicy::Discard, Severity::Warn,
                        message, log11_detail::decayArgument(args)...);
        }
    }

    //! \brief A convenience function for warning log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(may_truncate_or_discard, Severity::Warn, message, args...);
    //! \endcode
    template <typename... TArgs>
    void warn(may_truncate_or_discard_t, const char* message, TArgs&&... args)
    {
        if (Severity::Warn >= m_severityThreshold)
        {
            m_core->log(LogCore::ClaimPolicy::Truncate, Severity::Warn,
                        message, log11_detail::decayArgument(args)...);
        }
    }

    // //! \brief A convenience function for warning streams.
    // //!
    // //! Creates a warning stream equivalent to
    // //! \code
    // //! log(Severity::Warn)
    // //! \endcode
    // log11_detail::LogStreamStatement<> warn()
    // {
    //     return log(Severity::Warn);
    // }

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
        if (Severity::Error >= m_severityThreshold)
        {
            m_core->log(LogCore::ClaimPolicy::Block, Severity::Error,
                        message, log11_detail::decayArgument(args)...);
        }
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
        if (Severity::Error >= m_severityThreshold)
        {
            m_core->log(LogCore::ClaimPolicy::Discard, Severity::Error,
                        message, log11_detail::decayArgument(args)...);
        }
    }

    //! \brief A convenience function for error log entries.
    //!
    //! This function is equivalent to calling
    //! \code
    //! log(may_truncate_or_discard, Severity::Error, message, args...);
    //! \endcode
    template <typename... TArgs>
    void error(may_truncate_or_discard_t, const char* message, TArgs&&... args)
    {
        if (Severity::Error >= m_severityThreshold)
        {
            m_core->log(LogCore::ClaimPolicy::Truncate, Severity::Error,
                        message, log11_detail::decayArgument(args)...);
        }
    }

    // //! \brief A convenience function for error streams.
    // //!
    // //! Creates an error stream equivalent to
    // //! \code
    // //! log(Severity::Error)
    // //! \endcode
    // log11_detail::LogStreamStatement<> error()
    // {
    //     return log(Severity::Error);
    // }

private:
    //! The core which is in charge of the actual logging.
    LogCore* m_core;

    //! The severity threshold which log levels must reach or exceed in order
    //! to be forwarded to the core.
    std::atomic<Severity> m_severityThreshold;
};

} // namespace log11

#endif // LOG11_LOGGER_HPP
