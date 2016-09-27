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

LogStatement::LogStatement(Severity s)
    : timeStamp(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    , isTruncated(false)
    , isControl(false)
    , severity(static_cast<int>(s))
{
}

template <typename T, typename... TArgs>
void prepareSerializer(TypeList<T, TArgs...>)
{
    Serdes<T>::instance();
    prepareSerializer(TypeList<TArgs...>());
}

void prepareSerializer(TypeList<>)
{
}

} // namespace log11_detail


using namespace log11_detail;


// ----=====================================================================----
//     LogCore::ScratchPad
// ----=====================================================================----

LogCore::ScratchPad::ScratchPad(unsigned capacity)
    : m_data(new char[capacity]),
      m_capacity(capacity),
      m_size(0)
{
}

LogCore::ScratchPad::~ScratchPad()
{
    if (m_data)
        delete[] m_data;
}

void LogCore::ScratchPad::reserve(unsigned capacity, bool keepContent)
{
    if (capacity <= m_capacity)
        return;

    if (keepContent && m_size)
    {
        char* newData = new char[capacity];
        memcpy(newData, m_data, m_size);
        delete[] m_data;
        m_data = newData;
        m_capacity = capacity;
    }
    else
    {
        delete[] m_data;
        m_data = nullptr;
        m_data = new char[capacity];
        m_capacity = capacity;
        m_size = 0;
    }
}

void LogCore::ScratchPad::push(char ch)
{
    if (m_size == m_capacity)
        reserve((m_size + 7) & ~7, true);
    m_data[m_size] = ch;
    ++m_size;
}

void LogCore::ScratchPad::push(const char* data, unsigned size)
{
    unsigned newSize = m_size + size;
    if (newSize > m_capacity)
        reserve((newSize + 7) & ~7, true);
    memcpy(m_data + m_size, data, size);
    m_size = newSize;
}

const char* LogCore::ScratchPad::data() const noexcept
{
    return m_data;
}

unsigned LogCore::ScratchPad::size() const noexcept
{
    return m_size;
}

// ----=====================================================================----
//     LogCore
// ----=====================================================================----

LogCore::LogCore(unsigned exponent, TextSink* textSink, BinarySinkBase* binarySink)
    : m_messageFifo(exponent),
      m_scratchPad(32),
      m_binarySink(binarySink),
      m_textSink(textSink)
{
    log11_detail::prepareSerializer(log11_detail::builtin_types());

#ifdef LOG11_USE_WEOS
    weos::thread(attrs, &LogCore::consumeFifoEntries, this).detach();
#else
    std::thread(&LogCore::consumeFifoEntries, this).detach();
#endif // LOG11_USE_WEOS
}

LogCore::LogCore(unsigned exponent, BinarySinkBase* binarySink, TextSink* textSink)
    : m_messageFifo(exponent),
      m_scratchPad(32),
      m_binarySink(binarySink),
      m_textSink(textSink)
{
    log11_detail::prepareSerializer(log11_detail::builtin_types());

#ifdef LOG11_USE_WEOS
    weos::thread(attrs, &LogCore::consumeFifoEntries, this).detach();
#else
    std::thread(&LogCore::consumeFifoEntries, this).detach();
#endif // LOG11_USE_WEOS
}

LogCore::~LogCore()
{
    LogStatement stmt;
    stmt.isControl = 1;
    stmt.severity = static_cast<int>(LogStatement::Command::Terminate);
    auto claimed = m_messageFifo.claim(sizeof(stmt));
    claimed.stream(m_messageFifo).write(&stmt, sizeof(LogStatement));
    m_messageFifo.publish(claimed);

    unique_lock<mutex> lock(m_consumerThreadMutex);
    m_consumerThreadCv.wait(lock,
                            [&] { return m_consumerState == Terminated; });
}

void LogCore::enableImmutableStringOptimization(
        uintptr_t beginAddress, uintptr_t endAddress)
{
    // TODO: Pause the consumer

    m_serdesOptions.immutableStringBegin = beginAddress;
    m_serdesOptions.immutableStringEnd = endAddress;
}

void LogCore::consumeFifoEntries()
{
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

        // Deserialize the header.
        auto stream = block.stream(m_messageFifo);
        LogStatement stmt;
        stream.read(&stmt, sizeof(LogStatement));

        // Process control commands.
        if (stmt.isControl)
        {
            auto command = static_cast<LogStatement::Command>(stmt.severity);
            if (command == LogStatement::Command::Terminate)
                break;

            continue;
        }

        BinarySinkBase* binarySink = m_binarySink;
        if (binarySink)
        {
            binarySink->beginLogEntry(static_cast<Severity>(stmt.severity));
            writeBinary(binarySink, stream);
            binarySink->endLogEntry();
        }

        TextSink* textSink = m_textSink;
        if (textSink)
        {
            textSink->beginLogEntry(static_cast<Severity>(stmt.severity));
            writeText(textSink, stream);
            textSink->endLogEntry();
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

    SplitString str{format, nullptr, strlen(format), 0};

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
            m_scratchPad.push('0');
            outStream.parseFormatString(m_scratchPad.data());
        }

        // TODO:
        // if (argCounter != argument.from.format.spec)
        // skipToArgument()


        log11_detail::SerdesBase* serdes;
        if (inStream.read(&serdes, sizeof(void*)))
        {
            serdes->deserialize(inStream, outStream);
        }
        else
        {
            // TODO: Output a marker
        }

        ++argCounter;
        marker = iter + 1;
    }
    if (iter != marker)
        sink->putString(marker, iter - marker);

    // TODO: Output rest of arguments
}

void LogCore::writeBinary(BinarySinkBase* sink, RingBuffer::Stream inStream)
{
    const char* format;
    if (!inStream.read(&format, sizeof(const char*)))
        return;

    SplitString str{format, nullptr, strlen(format), 0};

    BinaryStream outStream(*sink);
    outStream << str;
    for (;;)
    {
        log11_detail::SerdesBase* serdes;
        if (!inStream.read(&serdes, sizeof(void*)))
            break;
        serdes->deserialize(inStream, outStream);
    }
}

} // namespace log11
