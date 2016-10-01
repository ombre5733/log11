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

#ifndef LOG11_LOGBUFFER_HPP
#define LOG11_LOGBUFFER_HPP

#include "LogCore.hpp"
#include "RingBuffer.hpp"
#include "Serdes.hpp"
#include "Utility.hpp"


namespace log11
{

class LogBuffer
{
public:
    explicit
    LogBuffer(LogCore* core, LogCore::ClaimPolicy policy, Severity severity,
              std::size_t size);

    LogBuffer(LogBuffer&& other) noexcept;
    LogBuffer& operator=(LogBuffer&& other) noexcept;

    LogBuffer(const LogBuffer&) = delete;
    LogBuffer& operator=(const LogBuffer&) = delete;

    ~LogBuffer();

    void discard();

    //! \brief Flushes the buffer.
    //!
    //! Flushes the buffer to the log core. Flushing also closes the buffer
    //! so that no more entries can be added.
    void flush();

    template <typename... TArgs>
    LogBuffer& format(const char* format, TArgs&&... args);

    template <typename T>
    LogBuffer& operator<<(const T& value);

private:
    LogCore* m_core;
    Severity m_severity;
    LogCore::ClaimPolicy m_policy;
    bool m_hadEnoughSpace;
    RingBuffer::Block m_claimed;
    RingBuffer::Stream m_stream;
};

template <typename... TArgs>
LogBuffer& LogBuffer::format(const char* format, TArgs&&... args)
{
    using namespace log11_detail;
    if (m_core && m_hadEnoughSpace)
    {
        m_hadEnoughSpace = SerdesVisitor::serialize(
                    m_core->m_serdesOptions, m_stream,
                    makeFormatTuple(format, log11_detail::decayArgument(args)...));
    }
    return *this;
}

template <typename T>
LogBuffer& LogBuffer::operator<<(const T& value)
{
    using namespace log11_detail;
    if (m_core && m_hadEnoughSpace)
    {
        m_hadEnoughSpace = SerdesVisitor::serialize(
                    m_core->m_serdesOptions, m_stream,
                    log11_detail::decayArgument(value));
    }
    return *this;
}

} // namespace log11

#endif // LOG11_LOGBUFFER_HPP
