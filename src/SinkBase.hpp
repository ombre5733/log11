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

#ifndef LOG11_SINKBASE_HPP
#define LOG11_SINKBASE_HPP

#include "LogRecordData.hpp"
#include "Severity.hpp"

#include <atomic>
#include <cstddef>


namespace log11
{

//! \brief The base class for all logger sinks.
//!
//! The SinkBase is the base class for all sinks. It implements basic
//! functionality to enable or disable the sink and to set its severity.
class SinkBase
{
public:
    using byte = std::uint8_t; // TODO: std::byte


    SinkBase();

    //! Destroys the sink.
    virtual
    ~SinkBase();

    // Configuration

    //! \brief Enables or disables the sink.
    //!
    //! If \p enable is set, the sink is enabled. By default, the sink is
    //! disabled.
    //!
    //! If a derived class re-implements setLevel(), it has call the base
    //! implementation as well.
    virtual
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
    //!
    //! If a derived class re-implements setLevel(), it has call the base
    //! implementation as well.
    virtual
    void setLevel(Severity threshold) noexcept;

    //! \brief Returns the logging level.
    Severity level() const noexcept;


    //! \brief Starts a new log record.
    //!
    //! Starts a new log record. The meta-data of the record are
    //! passed in the \p data. The default implementation calls
    //! setRecordSeverity().
    //!
    //! If a derived class re-implements beginLogEntry(), it has to
    //! call setRecordSeverity() manually or call the base implementation.
    virtual
    void beginLogEntry(const LogRecordData& data);

    //! \brief Finishes a log record.
    //!
    //! Finishes the current log record. The default implementation does
    //! nothing.
    virtual
    void endLogEntry(const LogRecordData& data);

protected:
    //! \brief Checks if the current record has to be logged.
    //!
    //! Returns \p true, if the current record has to be logged. The result
    //! combines the enabled state of the sink with the severity of the record.
    bool isCurrentRecordLogged() const noexcept;

    //! \brief Sets the severity of the current record.
    //!
    //! Stores the \p severity of the current log record. At the same time
    //! when the severity is stored, also the enabled state of the sink is
    //! cached.
    void setRecordSeverity(Severity severity) noexcept;

private:
    //! The severity threshold which log levels must reach or exceed in order
    //! to be forwarded to the core. The MSB is used to keep track of the
    //! enabled state.
    std::atomic<unsigned char> m_configuration;

    //! Set if the current record needs to be logged.
    bool m_logCurrentRecord;
};

} // namespace log11

#endif // LOG11_SINKBASE_HPP
