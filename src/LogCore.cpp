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

#include "LogCore.hpp"
#include "BinarySink.hpp"
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
    serdes_t<T>::instance();
    prepareSerializer(TypeList<TArgs...>());
}

void prepareSerializer(TypeList<>)
{
    ImmutableCharStarSerdes::instance();
    MutableCharStarSerdes::instance();
    FormatTupleSerdes::instance();
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
    log11_detail::prepareSerializer(log11_detail::BuiltInTypes());

    m_headerGenerator = RecordHeaderGenerator::parse("[{D}d {H}:{M}:{S}.{us} {L}] ");

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

void LogCore::setTextHeader(const char* header)
{
    auto* generator = RecordHeaderGenerator::parse(header);
    if (m_headerGenerator)
        delete m_headerGenerator;
    m_headerGenerator = generator;
}

// ----=====================================================================----
//     Private methods
// ----=====================================================================----

void LogCore::writeRecordHeader(RingBuffer::Stream& stream,
                                Directive directive)
{
    stream.write(directive);
    auto durationSinceEpoche
            = chrono::high_resolution_clock::now().time_since_epoch().count();
    stream.write(&durationSinceEpoche, sizeof(durationSinceEpoche));
}

RingBuffer::Block LogCore::claim(ClaimPolicy policy, std::size_t argumentSize)
{
    // We cannot rely on the serdes options if a change is going on.
    if (m_crossThreadChangeOngoing)
    {
        if (policy != Block)
            return RingBuffer::Block();
        m_crossThreadChangeDone.expect(m_crossThreadChangeOngoing, false);
    }

    auto totalSize = argumentSize + headerSize;
    RingBuffer::Block claimed;
    switch (policy)
    {
    case Block:    claimed = m_messageFifo.claim(totalSize); break;
    case Truncate: claimed = m_messageFifo.tryClaim(headerSize, totalSize); break;
    case Discard:  claimed = m_messageFifo.tryClaim(totalSize, totalSize); break;
    }

    return claimed;
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
            if (command == Directive::Skip)
                continue;
            if (command == Directive::SetImmutableSpace)
            {
                stream.read(&m_serdesOptions.immutableStringBegin, sizeof(uintptr_t));
                stream.read(&m_serdesOptions.immutableStringEnd, sizeof(uintptr_t));
            }
            if (   command == Directive::SetBinarySink
                || command == Directive::SetBothSinks)
            {
                BinarySinkBase* sink;
                if (stream.read(&sink, sizeof(BinarySinkBase*)))
                    m_binarySink = sink;
            }
            if (   command == Directive::SetTextSink
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
            writeToBinary(stream);
            m_binarySink->endLogEntry(record);
        }

        // Write the entry to the text sink.
        if (m_textSink)
        {
            m_scratchPad.clear();
            m_textSink->beginLogEntry(record);
            if (m_headerGenerator)
            {
                m_headerGenerator->generate(record, m_scratchPad);
                m_textSink->writeHeader(m_scratchPad.data(), m_scratchPad.size());
            }
            else
            {
                m_textSink->writeHeader("", 0);
            }
            writeToText(stream);
            m_textSink->endLogEntry(record);
        }
    }

    lock_guard<mutex> lock(m_consumerThreadMutex);
    m_consumerState = Terminated;
    m_consumerThreadCv.notify_one();
}

void LogCore::writeToText(RingBuffer::Stream inStream)
{
    TextStream outStream(*m_textSink, m_scratchPad);
    for (;;)
    {
        SerdesBase* serdes;
        if (!inStream.read(&serdes, sizeof(void*)) || !serdes)
            return;
        serdes->deserialize(inStream, outStream);
    }
}

void LogCore::writeToBinary(RingBuffer::Stream inStream)
{
    BinaryStream outStream(*m_binarySink, m_serdesOptions);
    for (;;)
    {
        SerdesBase* serdes;
        if (!inStream.read(&serdes, sizeof(void*)) || !serdes)
            return;
        serdes->deserialize(inStream, outStream);
    }
}

} // namespace log11
