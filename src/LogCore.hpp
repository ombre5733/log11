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
#include <mutex>
#include <type_traits>

#ifdef LOG11_USE_WEOS
#include <weos/thread.hpp>
#else
#include <thread>
#endif

namespace log11
{

class BinarySinkBase;
class TextSink;

namespace log11_detail
{

struct LogStatement
{
    enum class Command
    {
        Terminate = 1
    };

    LogStatement() = default;

    LogStatement(Severity severity);

    std::int64_t timeStamp;
    unsigned isTruncated : 1;
    unsigned isControl : 1;
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
    LogCore(unsigned exponent,
            TextSink* textSink = nullptr, BinarySinkBase* binarySink = nullptr);

    explicit
    LogCore(unsigned exponent,
            BinarySinkBase* binarySink, TextSink* textSink = nullptr);

    ~LogCore();

    LogCore(const LogCore&) = delete;
    LogCore& operator=(const LogCore&) = delete;

    void enableImmutableStringOptimization(
            std::uintptr_t beginAddress, std::uintptr_t endAddress);

    template <typename... TArgs>
    void log(ClaimPolicy policy, Severity severity, const char* format,
             TArgs&&... args);

private:
    enum ConsumerState
    {
        Initial,
        Terminated
    };

    struct ScratchPad
    {
    public:
        ScratchPad(unsigned capacity);
        ~ScratchPad();

        ScratchPad(const ScratchPad&) = delete;
        ScratchPad& operator=(const ScratchPad&) = delete;

        void reserve(unsigned capacity, bool keepContent);

        void push(char ch);
        void push(const char* data, unsigned size);

        const char* data() const noexcept;
        unsigned size() const noexcept;

    private:
        char* m_data;
        unsigned m_capacity;
        unsigned m_size;
    };


    //! An ultra-fast ring-buffer to pass log entries from the logging thread
    //! to the consumer thread.
    RingBuffer m_messageFifo;

    //! A scratch pad to perform some string conversions.
    ScratchPad m_scratchPad;

    //! Options for serialization.
    log11_detail::SerdesOptions m_serdesOptions;

    std::atomic<BinarySinkBase*> m_binarySink;
    std::atomic<TextSink*> m_textSink;

    //! Signals changes in the consumer state.
    std::condition_variable m_consumerThreadCv;
    //! A mutex for the consumer thread state.
    std::mutex m_consumerThreadMutex;
    //! The state of the consumer thread.
    ConsumerState m_consumerState{Initial};



    template <typename TArg, typename... TArgs>
    static
    std::size_t doRequiredSize(const log11_detail::SerdesOptions& opt,
                               const TArg& arg, const TArgs&... args)
    {
        return log11_detail::serdes_t<TArg>::requiredSize(opt, arg)
               + doRequiredSize(opt, args...);
    }

    static
    std::size_t doRequiredSize(const log11_detail::SerdesOptions&)
    {
        return 0;
    }


    template <typename TArg, typename... TArgs>
    static
    void doSerialize(const log11_detail::SerdesOptions& opt,
                     RingBuffer::Stream& stream,
                     const TArg& arg, const TArgs&... args)
    {
        if (log11_detail::serdes_t<TArg>::serialize(opt, stream, arg))
        {
            doSerialize(opt, stream, args...);
        }
    }

    static
    void doSerialize(const log11_detail::SerdesOptions&, RingBuffer::Stream&)
    {
    }


    void consumeFifoEntries();
    void writeText(TextSink* sink, RingBuffer::Stream inStream);
    void writeBinary(BinarySinkBase* sink, RingBuffer::Stream inStream);
};

template <typename... TArgs>
void LogCore::log(ClaimPolicy policy, Severity severity,
                  const char* format, TArgs&&... args)
{
    using namespace std;
    using namespace log11_detail;

    auto argumentSize = doRequiredSize(m_serdesOptions, args...);
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
    doSerialize(m_serdesOptions, stream, args...);

    if (policy == Block)
        m_messageFifo.publish(claimed);
    else
        m_messageFifo.tryPublish(claimed);
}

} // namespace log11

#endif // LOG11_LOGCORE_HPP
