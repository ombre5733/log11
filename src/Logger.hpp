/*******************************************************************************
  log11
  https://github.com/ombre5733/log11

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
#include "LogBuffer.hpp"
#include "LogCore.hpp"
#include "Severity.hpp"
#include "TypeTraits.hpp"
#include "Utility.hpp"

#ifdef LOG11_USE_WEOS
#include <weos/utility.hpp>
#endif // LOG11_USE_WEOS


namespace log11
{

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

    // Configuration

    //! \brief Enables or disables the logger.
    //!
    //! If \p enable is set, the logger is enabled. By default, the logger is
    //! enabled.
    void setEnabled(bool enable) noexcept;

    //! \brief Checks if the sink is enabled.
    //!
    //! Returns \p true, if the sink is enabled.
    bool isEnabled() const noexcept;

    //! \brief Sets the logging level.
    //!
    //! Sets the logging level to the given \p threshold. Log messages with
    //! a severity lower than the threshold are discarded. By default, the
    //! threshold is set to Severity::Info.
    void setLevel(Severity threshold) noexcept;

    //! \brief Returns the logging level.
    Severity level() const noexcept;



    //! If the level of this logger is lower or equal to the \p severity of
    //! the message, a new log entry is created by interpolating the
    //! \p message with the given \p args.
    //! The caller is blocked until there is sufficient space in the FIFO,
    //! which connects the logger front-ends to the sinks.
    template <typename... TArgs>
    void log(Severity severity, const char* message, TArgs&&... args)
    {
        if (canLog(severity))
        {
            m_core->log(LogCore::ClaimPolicy::Block, severity,
                        log11_detail::makeFormatTuple(
                            message, log11_detail::decayArgument(args)...));
        }
    }

    template <typename TArg, typename... TArgs>
    void logRaw(Severity severity, TArg&& arg, TArgs&&... args)
    {
        if (canLog(severity))
        {
            m_core->log(LogCore::ClaimPolicy::Block, severity,
                        log11_detail::decayArgument(arg),
                        log11_detail::decayArgument(args)...);
        }
    }

    LogBuffer logBuffer(Severity severity, std::size_t size);



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
        if (canLog(severity))
        {
            m_core->log(LogCore::ClaimPolicy::Discard, severity,
                        log11_detail::makeFormatTuple(
                            message, log11_detail::decayArgument(args)...));
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
        if (canLog(severity))
        {
            m_core->log(LogCore::ClaimPolicy::Truncate, severity,
                        log11_detail::makeFormatTuple(
                            message, log11_detail::decayArgument(args)...));
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
        this->log(Severity::Trace, message, std::forward<TArgs>(args)...);
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
        this->log(may_discard, Severity::Trace,
                  message, std::forward<TArgs>(args)...);
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
        this->log(may_truncate_or_discard, Severity::Trace,
                  message, std::forward<TArgs>(args)...);
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
        this->log(Severity::Debug, message, std::forward<TArgs>(args)...);
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
        this->log(may_discard, Severity::Debug,
                  message, std::forward<TArgs>(args)...);
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
        this->log(may_truncate_or_discard, Severity::Debug,
                  message, std::forward<TArgs>(args)...);
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
        this->log(Severity::Info, message, std::forward<TArgs>(args)...);
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
        this->log(may_discard, Severity::Info,
                  message, std::forward<TArgs>(args)...);
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
        this->log(may_truncate_or_discard, Severity::Info,
                  message, std::forward<TArgs>(args)...);
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
        this->log(Severity::Warn, message, std::forward<TArgs>(args)...);
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
        this->log(may_discard, Severity::Warn,
                  message, std::forward<TArgs>(args)...);
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
        this->log(may_truncate_or_discard, Severity::Warn,
                  message, std::forward<TArgs>(args)...);
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
        this->log(Severity::Error, message, std::forward<TArgs>(args)...);
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
        this->log(may_discard, Severity::Error,
                  message, std::forward<TArgs>(args)...);
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
        this->log(may_truncate_or_discard, Severity::Error,
                  message, std::forward<TArgs>(args)...);
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
    //! to be forwarded to the core. The MSB is used to keep track of the
    //! enabled state.
    std::atomic<unsigned char> m_configuration;


    //! Returns \p true, if a message with level \p severity can be logged.
    bool canLog(Severity severity) const noexcept
    {
        auto config = m_configuration.load();
        return (config & 0x80) != 0 && severity >= Severity(config & 0x7F);
    }
};

} // namespace log11

#endif // LOG11_LOGGER_HPP
