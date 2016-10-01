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

#include "Utility.hpp"
#include "Logger.hpp"

#include <cstring>


using namespace std;


namespace log11
{
namespace log11_detail
{

// ----=====================================================================----
//     ScratchPad
// ----=====================================================================----

ScratchPad::ScratchPad(unsigned capacity)
    : m_data(new char[capacity]),
      m_capacity(capacity),
      m_size(0)
{
}

ScratchPad::~ScratchPad()
{
    if (m_data)
        delete[] m_data;
}

void ScratchPad::resize(unsigned capacity)
{
    if (capacity <= m_capacity)
        return;

    char* newData = new char[capacity];
    if (m_size)
        memcpy(newData, m_data, m_size);
    delete[] m_data;
    m_data = newData;
    m_capacity = capacity;
}

void ScratchPad::clear() noexcept
{
    m_size = 0;
}

void ScratchPad::push(char ch)
{
    if (m_size == m_capacity)
        resize((m_size + 7) & ~7);
    m_data[m_size] = ch;
    ++m_size;
}

void ScratchPad::push(const char* data, unsigned size)
{
    unsigned newSize = m_size + size;
    if (newSize > m_capacity)
        resize((newSize + 7) & ~7);
    memcpy(m_data + m_size, data, size);
    m_size = newSize;
}

const char* ScratchPad::data() const noexcept
{
    return m_data;
}

unsigned ScratchPad::size() const noexcept
{
    return m_size;
}

// ----=====================================================================----
//     Record header generators
// ----=====================================================================----

struct LiteralGenerator : public RecordHeaderGenerator
{
    LiteralGenerator(const char* str, std::size_t length)
        : m_data(str),
          m_length(length)
    {
    }

    virtual
    void append(LogRecordData&, log11_detail::ScratchPad& pad) override
    {
        pad.push(m_data, m_length);
    }

    const char* m_data;
    std::size_t m_length;
};

struct SeverityGenerator : public RecordHeaderGenerator
{
    explicit
    SeverityGenerator()
    {
    }

    virtual
    void append(LogRecordData& record, log11_detail::ScratchPad& pad) override
    {
        static const char* severity_texts[] = {
            "TRACE",
            "DEBUG",
            "INFO ",
            "WARN ",
            "ERROR"
        };
        pad.push(severity_texts[static_cast<unsigned>(record.severity)], 5);
    }
};

struct TimeGenerator : public RecordHeaderGenerator
{
    enum Flags : unsigned char
    {
        Days         = 0x01,
        Hours        = 0x02,
        Minutes      = 0x04,
        Seconds      = 0x08,
        Milliseconds = 0x10,
        Microseconds = 0x20,
        Nanoseconds  = 0x40,
    };

    explicit
    TimeGenerator()
        : m_numSeparators(0),
          m_flags(0)
    {
    }

    virtual
    void append(LogRecordData& record, log11_detail::ScratchPad& pad) override
    {
        using namespace std::chrono;
        using rep_t = high_resolution_clock::rep;

        auto print = [&] (rep_t val) {
            rep_t divisor = 10;
            if (val > 100)
            {
                if (val < 1000)
                {
                    divisor = 100;
                }
                else if (val < 10000)
                {
                    divisor = 1000;
                }
                else
                {
                    while (divisor <= std::numeric_limits<rep_t>::max() / 10
                           && divisor * 10 <= val)
                        divisor *= 10;
                }
            }

            while (divisor)
            {
                rep_t digit = val / divisor;
                pad.push('0' + digit);
                val -= digit * divisor;
                divisor /= 10;
            }
        };

        auto printFraction = [&] (rep_t val, rep_t divisor) {
            while (divisor)
            {
                rep_t digit = val / divisor;
                pad.push('0' + digit);
                val -= digit * divisor;
                divisor /= 10;
            }
        };

        auto time = record.time.time_since_epoch();
        if (m_flags & Days)
        {
            using day_type = duration<rep_t, std::ratio<86400>>;
            auto days = duration_cast<day_type>(time);
            print(days.count());
            time -= days;
        }
        if (m_flags & Hours)
        {
            auto hours = duration_cast<std::chrono::hours>(time);
            print(hours.count());
            time -= hours;
        }
        if (m_flags & Minutes)
        {
            auto minutes = duration_cast<std::chrono::minutes>(time);
            print(minutes.count());
            time -= minutes;
        }
        if (m_flags & Seconds)
        {
            auto seconds = duration_cast<std::chrono::seconds>(time);
            print(seconds.count());
            time -= seconds;
        }

        if (m_flags & Milliseconds)
        {
            auto ms = duration_cast<std::chrono::milliseconds>(time);
            printFraction(ms.count() % rep_t(1000), 100);
        }
        else if (m_flags & Microseconds)
        {
            auto ms = duration_cast<std::chrono::microseconds>(time);
            printFraction(ms.count() % rep_t(1000000), 100000);
        }
        else if (m_flags & Nanoseconds)
        {
            auto ms = duration_cast<std::chrono::nanoseconds>(time);
            printFraction(ms.count() % rep_t(1000000000), 100000000);
        }

        // TODO: Milli micro nano output
        record.time = high_resolution_clock::time_point(time);
    }

    unsigned m_numSeparators;
    unsigned char m_flags;
    char m_separator[4];
};

// ----=====================================================================----
//     Tags for headers
// ----=====================================================================----

enum class Tag
{
    Days,
    Hours,
    Minutes,
    Seconds,
    Milliseconds,
    Microseconds,
    Nanoseconds,
    Level,
    None,
};

bool operator>=(Tag t1, Tag t2) noexcept
{
    return int(t1) >= int(t2);
}

Tag toTag(const char* begin, std::size_t length) noexcept
{
    if (length == 1)
    {
        switch (*begin)
        {
        case 'D': return Tag::Days;
        case 'H': return Tag::Hours;
        case 'M': return Tag::Minutes;
        case 'S': return Tag::Seconds;
        case 'L': return Tag::Level;
        default: return Tag::None;
        }
    }
    else if (length == 2)
    {
        if (begin[1] != 's')
            return Tag::None;

        switch (*begin)
        {
        case 'm': return Tag::Milliseconds;
        case 'u': return Tag::Microseconds;
        case 'n': return Tag::Nanoseconds;
        default: return Tag::None;
        }
    }

    return Tag::None;
}

// ----=====================================================================----
//     RecordHeaderGenerator
// ----=====================================================================----

RecordHeaderGenerator::RecordHeaderGenerator()
    : m_next(nullptr)
{
}

RecordHeaderGenerator::~RecordHeaderGenerator()
{
    if (m_next)
        delete m_next;
}

void RecordHeaderGenerator::generate(
        LogRecordData& record, log11_detail::ScratchPad& pad)
{
    RecordHeaderGenerator* self = this;
    while (self)
    {
        self->append(record, pad);
        self = self->m_next;
    }
}

RecordHeaderGenerator* RecordHeaderGenerator::parse(const char* str)
{
    Tag previousTag = Tag::None;
    RecordHeaderGenerator* first = nullptr;
    RecordHeaderGenerator* last = nullptr;
    auto append = [&] (RecordHeaderGenerator* p) {
        if (!first)
        {
            first = last = p;
        }
        else
        {
            last = last->m_next = p;
        }
    };

    const char* marker = str;
    while (*str)
    {
        if (*str != '{')
        {
            ++str;
            continue;
        }

        if (str != marker)
        {
            append(new LiteralGenerator(marker, str - marker));
            previousTag = Tag::None;
        }

        marker = ++str;
        while (*str && *str != '}')
            ++str;

        Tag tag = toTag(marker, str - marker);
        marker = ++str;

        switch (tag)
        {
        case Tag::Days:
            append(new TimeGenerator());
            static_cast<TimeGenerator*>(last)->m_flags |= TimeGenerator::Days;
            break;

        case Tag::Hours:
            if (previousTag >= Tag::Hours)
                append(new TimeGenerator());
            static_cast<TimeGenerator*>(last)->m_flags |= TimeGenerator::Hours;
            break;

        case Tag::Minutes:
            if (previousTag >= Tag::Minutes)
                append(new TimeGenerator());
            static_cast<TimeGenerator*>(last)->m_flags |= TimeGenerator::Minutes;
            break;

        case Tag::Seconds:
            if (previousTag >= Tag::Seconds)
                append(new TimeGenerator());
            static_cast<TimeGenerator*>(last)->m_flags |= TimeGenerator::Seconds;
            break;

        case Tag::Milliseconds:
            if (previousTag >= Tag::Milliseconds)
                append(new TimeGenerator());
            static_cast<TimeGenerator*>(last)->m_flags |= TimeGenerator::Milliseconds;
            break;

        case Tag::Microseconds:
            if (previousTag >= Tag::Microseconds)
                append(new TimeGenerator());
            static_cast<TimeGenerator*>(last)->m_flags |= TimeGenerator::Microseconds;
            break;

        case Tag::Nanoseconds:
            if (previousTag >= Tag::Nanoseconds)
                append(new TimeGenerator());
            static_cast<TimeGenerator*>(last)->m_flags |= TimeGenerator::Nanoseconds;
            break;

        case Tag::Level:
            append(new SeverityGenerator());
            break;

        default:
        case Tag::None:
            append(new LiteralGenerator("<?>", 3));
            break;
        }

        previousTag = tag;
    }
    if (str != marker)
        append(new LiteralGenerator(marker, str - marker));

    return first;
}

} // namespace log11_detail
} // namespace log11
