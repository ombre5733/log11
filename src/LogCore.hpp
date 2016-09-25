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

#ifndef LOG11_LOGCORE_HPP
#define LOG11_LOGCORE_HPP

#include "_config.hpp"

#include "RingBuffer.hpp"
#include "Serdes.hpp"
#include "Severity.hpp"

#include <atomic>
#include <cstddef>
#include <type_traits>

#ifdef LOG11_USE_WEOS
#include <weos/thread.hpp>
#else
#include <thread>
#endif

namespace log11
{

class TextSink;

namespace log11_detail
{

struct LogStatement
{
    LogStatement() = default;

    LogStatement(Severity severity);

    std::int64_t timeStamp;
    unsigned isTruncated : 1;
    unsigned reserved : 11;
    unsigned severity : 4;
};

} // namespace log11_detail


class LogCore
{
public:
    enum ClaimPolicy
    {
        Block,    //!< Block caller until sufficient space is available
        Truncate, //!< Message will be truncated or even discarded
        Discard   //!< Message is displayed fully or discarded
    };


    explicit
    LogCore(unsigned exponent, TextSink* textSink);

    ~LogCore();

    LogCore(const LogCore&) = delete;
    LogCore& operator=(const LogCore&) = delete;

    template <typename... TArgs>
    void log(ClaimPolicy policy, Severity severity, const char* format,
             TArgs&&... args);

private:
    //! An ultra-fast ring-buffer to pass log entries from the logging thread
    //! to the consumer thread.
    RingBuffer m_messageFifo;

    std::atomic_int m_flags;

    std::atomic<TextSink*> m_textSink;

#ifdef LOG11_USE_WEOS
    weos::thread m_consumerThread;
#else
    std::thread m_consumerThread;
#endif


    template <typename TArg, typename... TArgs>
    static
    std::size_t doRequiredSize(const TArg& arg, const TArgs&... args)
    {
        return sizeof(void*)
               + log11_detail::Serdes<std::decay_t<TArg>>::requiredSize(arg)
               + doRequiredSize(args...);
    }

    static
    std::size_t doRequiredSize()
    {
        return 0;
    }


    template <typename TArg, typename... TArgs>
    static
    void doSerialize(RingBuffer::Stream& stream,
                     const TArg& arg, const TArgs&... args)
    {
        auto* serdes = log11_detail::Serdes<std::decay_t<TArg>>::instance();
        if (stream.write(&serdes, sizeof(void*))
            && serdes->serialize(stream, arg))
        {
            doSerialize(stream, args...);
        }
    }

    static
    void doSerialize(RingBuffer::Stream&)
    {
    }


    void consumeFifoEntries();
    void writeText(TextSink* sink, RingBuffer::Stream inStream);
    void writeBinary(RingBuffer::Stream inStream);
};

template <typename... TArgs>
void LogCore::log(ClaimPolicy policy, Severity severity,
                  const char* format, TArgs&&... args)
{
    using namespace std;
    using namespace log11_detail;

    auto argumentSize = doRequiredSize(args...);
    auto totalSize = argumentSize + sizeof(LogStatement) + sizeof(const char*);

    RingBuffer::Block claimed;
    switch (policy)
    {
    case Block:    claimed = m_messageFifo.claim(totalSize); break;
    case Truncate: claimed = m_messageFifo.tryClaim(sizeof(LogStatement), totalSize); break;
    case Discard:  claimed = m_messageFifo.tryClaim(totalSize, totalSize); break;
    }
    if (claimed.length() == 0)
        return;

    auto stream = claimed.stream(m_messageFifo);

    // TODO: Serialize LogStatement
    LogStatement stmt(severity);
    stmt.isTruncated = claimed.length() < totalSize;
    stream.write(&stmt, sizeof(LogStatement));
    stream.write(&format, sizeof(const char*));

    // Serialize all the arguments.
    doSerialize(stream, args...);

    if (policy == Block)
        m_messageFifo.publish(claimed);
    else
        m_messageFifo.tryPublish(claimed);
}

} // namespace log11

#endif // LOG11_LOGCORE_HPP
