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

#include "Config.hpp"
#include "RingBuffer.hpp"
#include "Serdes.hpp"
#include "Severity.hpp"
#include "Synchronic.hpp"
#include "Utility.hpp"

#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
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

struct Directive
{
    enum Command
    {
        Terminate,
        SetImmutableSpace,
        SetBothSinks,
        SetBinarySink,
        SetTextSink
    };

    static
    Directive command(Command c)
    {
        Directive result;
        result.isCommand = 1;
        result.isTruncated = 0;
        result.severityOrCommand = static_cast<unsigned char>(c);
        return result;
    }

    static
    Directive entry(Severity s, bool truncated)
    {
        Directive result;
        result.isCommand = 0;
        result.isTruncated = truncated;
        result.severityOrCommand = static_cast<unsigned char>(s);
        return result;
    }

    //! If set, this is a control instruction rather than a log entry.
    unsigned char isCommand : 1;
    unsigned char isTruncated : 1;
    unsigned char reserved : 3;
    unsigned char severityOrCommand : 3;
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


#ifdef LOG11_USE_WEOS
    //! \brief Creates a log core.
    explicit
    LogCore(const weos::thread_attributes& attrs, std::size_t bufferSize);
#else
    explicit
    LogCore(std::size_t bufferSize);
#endif

    ~LogCore();

    LogCore(const LogCore&) = delete;
    LogCore& operator=(const LogCore&) = delete;

    //! \brief Sets a binary sink.
    //!
    //! Sets the binary sink to \p binarySink.
    void setSink(BinarySinkBase* binarySink);

    //! \brief Sets a text sink.
    //!
    //! Sets the text sink to \p textSink.
    void setSink(TextSink* textSink);

    //! \brief Sets binary and text sinks.
    //!
    //! Sets the binary sink to \p binarySink and the text sink to \p textSink.
    void setSinks(BinarySinkBase* binarySink, TextSink* textSink);

    //! \brief Enables the immutable string optimization.
    //!
    //! Tells the logger to optimize the strings which are located in the
    //! memory areay <tt>[beginAddress, endAddress)</tt>.
    void setImmutableStringSpace(
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


    //! An ultra-fast ring-buffer to pass log entries from the logging thread
    //! to the consumer thread.
    RingBuffer m_messageFifo;

    //! A scratch pad to hold perform some string conversions.
    log11_detail::ScratchPad m_scratchPad;

    //! Options for serialization.
    log11_detail::SerdesOptions m_serdesOptions;
    //! Set while a cross-thread change in the options is ongoing.
    std::atomic_bool m_crossThreadChangeOngoing;
    //! Signals when cross-thread changes are done.
    log11_detail::synchronic<bool> m_crossThreadChangeDone;

    //! The attached binary sink.
    BinarySinkBase* m_binarySink;
    //! The attached text sink.
    TextSink* m_textSink;

    //! The generator of the log header.
    log11_detail::RecordHeaderGenerator* m_headerGenerator;

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

    static constexpr unsigned header_size
            = sizeof(Directive)
              + sizeof(std::chrono::high_resolution_clock::rep)
              + sizeof(const char*);

    // We cannot rely on the serdes options if a change is going on.
    if (m_crossThreadChangeOngoing)
    {
        if (policy != Block)
            return;
        m_crossThreadChangeDone.expect(m_crossThreadChangeOngoing, false);
    }

    auto argumentSize = doRequiredSize(m_serdesOptions, args...);
    auto totalSize = argumentSize + header_size;

    RingBuffer::Block claimed;
    switch (policy)
    {
    case Block:    claimed = m_messageFifo.claim(totalSize); break;
    case Truncate: claimed = m_messageFifo.tryClaim(header_size, totalSize); break;
    case Discard:  claimed = m_messageFifo.tryClaim(totalSize, totalSize); break;
    }
    if (claimed.length() == 0)
        return;

    auto stream = claimed.stream(m_messageFifo);

    // Write the header.
    stream.write(Directive::entry(severity, claimed.length() < totalSize));
    auto durationSinceEpoche
            = std::chrono::high_resolution_clock::now().time_since_epoch().count();
    stream.write(&durationSinceEpoche, sizeof(durationSinceEpoche));
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
