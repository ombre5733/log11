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

#include "LogCore.hpp"
#include "BinarySink.hpp"
#include "String.hpp"
#include "TextSink.hpp"
#include "Utility.hpp"

#include <chrono>
#include <cstring>


using namespace std;


namespace log11
{

namespace log11_detail
{

template <typename T, typename... TArgs>
void prepareSerializer(TypeList<T, TArgs...>)
{
    Serdes<T>::instance();
    prepareSerializer(TypeList<TArgs...>());
}

void prepareSerializer(TypeList<>)
{
    ImmutableCharStarSerdes::instance();
    MutableCharStarSerdes::instance();
}

} // namespace log11_detail


using namespace log11_detail;


// ----=====================================================================----
//     LogCore
// ----=====================================================================----

#ifdef LOG11_USE_WEOS
LogCore::LogCore(const weos::thread_attributes& attrs, std::size_t bufferSize)
#else
LogCore::LogCore(std::size_t bufferSize)
#endif
    : m_messageFifo(bufferSize)
    , m_scratchPad(32)
    , m_crossThreadChangeOngoing(false)
    , m_binarySink(nullptr)
    , m_textSink(nullptr)
    , m_headerGenerator(nullptr)
{
    log11_detail::prepareSerializer(log11_detail::builtin_types());

    m_headerGenerator = RecordHeaderGenerator::parse("[{D} {H}:{M}:{S}.{us} {L}] ");

#ifdef LOG11_USE_WEOS
    weos::thread(attrs, &LogCore::consumeFifoEntries, this).detach();
#else
    std::thread(&LogCore::consumeFifoEntries, this).detach();
#endif // LOG11_USE_WEOS
}

LogCore::~LogCore()
{
    auto claimed = m_messageFifo.claim(m_messageFifo.size());
    claimed.stream(m_messageFifo).write(Directive::command(Directive::Terminate));
    m_messageFifo.publish(claimed);

    unique_lock<mutex> lock(m_consumerThreadMutex);
    m_consumerThreadCv.wait(lock,
                            [&] { return m_consumerState == Terminated; });

    if (m_headerGenerator)
        delete m_headerGenerator;
}

void LogCore::setSinks(BinarySinkBase* binarySink, TextSink* textSink)
{
    m_crossThreadChangeOngoing = true;

    auto claimed = m_messageFifo.claim(m_messageFifo.size());
    auto stream = claimed.stream(m_messageFifo);
    stream.write(Directive::command(Directive::SetBothSinks));
    stream.write(&binarySink, sizeof(BinarySinkBase*));
    stream.write(&textSink, sizeof(TextSink*));
    m_messageFifo.publish(claimed);
}

void LogCore::setSink(BinarySinkBase* binarySink)
{
    m_crossThreadChangeOngoing = true;

    auto claimed = m_messageFifo.claim(m_messageFifo.size());
    auto stream = claimed.stream(m_messageFifo);
    stream.write(Directive::command(Directive::SetBinarySink));
    stream.write(&binarySink, sizeof(BinarySinkBase*));
    m_messageFifo.publish(claimed);
}

void LogCore::setSink(TextSink* textSink)
{
    m_crossThreadChangeOngoing = true;

    auto claimed = m_messageFifo.claim(m_messageFifo.size());
    auto stream = claimed.stream(m_messageFifo);
    stream.write(Directive::command(Directive::SetTextSink));
    stream.write(&textSink, sizeof(TextSink*));
    m_messageFifo.publish(claimed);
}

void LogCore::setImmutableStringSpace(
        uintptr_t beginAddress, uintptr_t endAddress)
{
    m_crossThreadChangeOngoing = true;

    auto claimed = m_messageFifo.claim(m_messageFifo.size());
    auto stream = claimed.stream(m_messageFifo);
    stream.write(Directive::command(Directive::SetImmutableSpace));
    stream.write(beginAddress);
    stream.write(endAddress);
    m_messageFifo.publish(claimed);
}

void LogCore::consumeFifoEntries()
{
    using namespace std::chrono;

    struct BlockConsumer
    {
        BlockConsumer(RingBuffer& buffer, RingBuffer::Block& block)
            : m_buffer(buffer),
              m_block(block)
        {
        }

        ~BlockConsumer()
        {
            m_buffer.consume(m_block);
        }

        RingBuffer& m_buffer;
        RingBuffer::Block& m_block;
    };

    for (;;)
    {
        auto block = m_messageFifo.wait();
        BlockConsumer consumer(m_messageFifo, block);

        auto stream = block.stream(m_messageFifo);

        // Deserialize the header.
        Directive directive;
        if (!stream.read(&directive, 1))
            continue;

        // Process control commands.
        if (directive.isCommand)
        {
            auto command = static_cast<Directive::Command>(directive.severityOrCommand);
            if (command == Directive::SetImmutableSpace)
            {
                stream.read(&m_serdesOptions.immutableStringBegin, sizeof(uintptr_t));
                stream.read(&m_serdesOptions.immutableStringEnd, sizeof(uintptr_t));
            }
            if (command == Directive::SetBinarySink
                    || command == Directive::SetBothSinks)
            {
                BinarySinkBase* sink;
                if (stream.read(&sink, sizeof(BinarySinkBase*)))
                    m_binarySink = sink;
            }
            if (command == Directive::SetTextSink
                    || command == Directive::SetBothSinks)
            {
                TextSink* sink;
                if (stream.read(&sink, sizeof(TextSink*)))
                    m_textSink = sink;
            }
            // Signal that the cross-thread changes are done.
            m_crossThreadChangeDone.notify(m_crossThreadChangeOngoing, false);

            if (command == Directive::Terminate)
                break;
            else
                continue;
        }

        // Read the log record's header.
        LogRecordData record;
        record.severity = static_cast<Severity>(directive.severityOrCommand);
        record.isTruncated = directive.isTruncated;
        {
            high_resolution_clock::duration::rep durationSinceEpoche;
            if (!stream.read(&durationSinceEpoche, sizeof(durationSinceEpoche)))
                continue;
            record.time = high_resolution_clock::time_point(
                    high_resolution_clock::duration(durationSinceEpoche));
        }

        // Write the entry to the binary sink.
        if (m_binarySink)
        {
            m_binarySink->beginLogEntry(record);
            writeBinary(m_binarySink, stream);
            m_binarySink->endLogEntry();
        }

        // Write the entry to the text sink.
        if (m_textSink)
        {
            m_scratchPad.clear();
            m_headerGenerator->generate(record, m_scratchPad);
            m_textSink->beginLogEntry(record);
            m_textSink->putString(m_scratchPad.data(), m_scratchPad.size());
            writeText(m_textSink, stream);
            m_textSink->endLogEntry();
        }
    }

    lock_guard<mutex> lock(m_consumerThreadMutex);
    m_consumerState = Terminated;
    m_consumerThreadCv.notify_one();
}

void LogCore::writeText(TextSink* sink, RingBuffer::Stream inStream)
{
    const char* format;
    if (!inStream.read(&format, sizeof(const char*)))
        return;

    SplitStringView str{format, strlen(format), nullptr, 0};

    TextStream outStream(*sink);
    unsigned argCounter = 0;
    const char* iter = str.begin1;
    const char* marker = str.begin1;
    const char* end = str.begin1 + str.length1;
    for (;; ++iter)
    {
        // Loop to the start of the format specifier (or the end of the string).
        if (iter == end)
        {
            if (iter != marker)
                sink->putString(marker, iter - marker);
            marker = iter = str.begin2;
            end = str.begin2 + str.length2;
            str.length2 = 0;
            if (iter == end)
                break;
        }
        if (*iter != '{')
            continue;

        if (iter != marker)
            sink->putString(marker, iter - marker);

        m_scratchPad.clear();
        // Loop to the end of the format specifier (or the end of the string).
        marker = iter + 1;
        while (*iter != '}')
        {
            ++iter;
            if (iter == end)
            {
                if (iter != marker)
                    m_scratchPad.push(marker, iter - marker);
                marker = iter = str.begin2;
                end = str.begin2 + str.length2;
                str.length2 = 0;
                if (iter == end)
                    break;
            }
        }
        if (iter == end)
            break;

        if (iter != marker)
            m_scratchPad.push(marker, iter - marker);

        if (m_scratchPad.size())
        {
            m_scratchPad.push('\0');
            outStream.parseFormatString(m_scratchPad.data());
        }

        // TODO:
        // if (argCounter != argument.from.format.spec)
        // skipToArgument()


        log11_detail::SerdesBase* serdes;
        if (!inStream.read(&serdes, sizeof(void*))
            || !serdes
            || !serdes->deserialize(inStream, outStream))
        {
            sink->putString("<?>", 3);
        }

        ++argCounter;
        marker = iter + 1;
    }
    if (iter != marker)
        sink->putString(marker, iter - marker);

    log11_detail::SerdesBase* serdes;
    while (inStream.read(&serdes, sizeof(void*)) && serdes)
    {
        sink->putString(" <", 2);
        bool result = serdes->deserialize(inStream, outStream);
        sink->putChar('>');
        if (!result)
            break;
    }
}

void LogCore::writeBinary(BinarySinkBase* sink, RingBuffer::Stream inStream)
{
    BinaryStream outStream(*sink, m_serdesOptions);
    const char* format;
    if (!inStream.read(&format, sizeof(const char*)))
        return;
    outStream << format;

    for (;;)
    {
        log11_detail::SerdesBase* serdes;
        if (!inStream.read(&serdes, sizeof(void*)) || !serdes)
            break;
        serdes->deserialize(inStream, outStream);
    }
}

} // namespace log11
