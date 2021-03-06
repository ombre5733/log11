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

#include "Serdes.hpp"


using namespace std;


namespace log11
{
namespace log11_detail
{

// ----=====================================================================----
//     SerdesOptions
// ----=====================================================================----

bool SerdesOptions::isImmutable(const char* str) const noexcept
{
    return str == nullptr
           || (uintptr_t(str) < immutableStringEnd
               && uintptr_t(str) >= immutableStringBegin);
}

// ----=====================================================================----
//     SerdesBase
// ----=====================================================================----

SerdesBase::~SerdesBase()
{
}

// ----=====================================================================----
//     FormatTupleSerdes
// ----=====================================================================----

FormatTupleSerdes* FormatTupleSerdes::instance()
{
    static FormatTupleSerdes serdes;
    return &serdes;
}

bool FormatTupleSerdes::deserialize(
        RingBuffer::Stream& inStream, BinaryStream& outStream) const noexcept
{
    RingBuffer::Stream postStream = inStream;
    std::uint16_t length;
    if (!inStream.read(&length, sizeof(std::uint16_t)))
        return false;
    postStream.skip(length);
    inStream.limit(length);

    SerdesBase* serdes;
    if (!inStream.read(&serdes, sizeof(void*)) || !serdes)
        return false;

    SplitStringView str{nullptr, 0, nullptr, 0};
    if (!static_cast<CharStarSerdes*>(serdes)->deserializeString(inStream, str))
        return false;

    outStream.m_sink->beginFormatTuple();
    outStream << str;
    for (;;)
    {
        if (!inStream.read(&serdes, sizeof(void*)) || !serdes)
            break;
        serdes->deserialize(inStream, outStream);
    }
    outStream.m_sink->endFormatTuple();

    return true;
}

bool FormatTupleSerdes::deserialize(
        RingBuffer::Stream& inStream, TextStream& outStream) const noexcept
{
    RingBuffer::Stream postStream = inStream;
    std::uint16_t length;
    if (!inStream.read(&length, sizeof(std::uint16_t)))
        return false;
    postStream.skip(length);
    inStream.limit(length);

    SerdesBase* serdes;
    if (!inStream.read(&serdes, sizeof(void*)) || !serdes)
        return false;

    SplitStringView str{nullptr, 0, nullptr, 0};
    if (!static_cast<CharStarSerdes*>(serdes)->deserializeString(inStream, str))
        return false;

    outStream.doFormat(str,
                       log11_detail::ArgumentForwarder<RingBuffer::Stream>(
                           outStream, inStream));
    return true;
}

// ----=====================================================================----
//     CharStarSerdes
// ----=====================================================================----

std::size_t CharStarSerdes::requiredSize(const log11_detail::SerdesOptions& opt,
                                         const char* str) noexcept
{
    static_assert(sizeof(Immutable<const char*>) == sizeof(const char*), "");

    if (opt.isImmutable(str))
    {
        return sizeof(void*) + sizeof(const char*);
    }
    else
    {
        std::uint16_t length = str ? std::strlen(str) : 0;
        return sizeof(void*) + sizeof(std::uint16_t) + length;
    }
}

bool CharStarSerdes::serialize(const log11_detail::SerdesOptions& opt,
                               RingBuffer::Stream& stream, const char* str) noexcept
{
    if (opt.isImmutable(str))
    {
        SerdesBase* serdes = ImmutableCharStarSerdes::instance();
        return stream.write(&serdes, sizeof(void*))
                && stream.write(&str, sizeof(const char*));
    }
    else
    {
        SerdesBase* serdes = MutableCharStarSerdes::instance();
        std::uint16_t length = str ? std::strlen(str) : 0;
        return stream.write(&serdes, sizeof(void*))
                && stream.write(&length, sizeof(std::uint16_t))
                && stream.writeString(str, length);
    }
}

// ----=====================================================================----
//     ImmutableCharStarSerdes
// ----=====================================================================----

ImmutableCharStarSerdes* ImmutableCharStarSerdes::instance()
{
    static ImmutableCharStarSerdes serdes;
    return &serdes;
}

bool ImmutableCharStarSerdes::deserialize(
        RingBuffer::Stream& inStream, BinaryStream& outStream) const noexcept
{
    Immutable<const char*> str;
    if (inStream.read(&str, sizeof(Immutable<const char*>)))
    {
        outStream << str;
        return true;
    }
    else
    {
        return false;
    }
}

bool ImmutableCharStarSerdes::deserialize(
        RingBuffer::Stream& inStream, TextStream& outStream) const noexcept
{
    Immutable<const char*> str;
    if (inStream.read(&str, sizeof(Immutable<const char*>)))
    {
        outStream << static_cast<const char*>(str);
        return true;
    }
    else
    {
        return false;
    }
}

bool ImmutableCharStarSerdes::deserializeString(
        RingBuffer::Stream& inStream, SplitStringView& str) const noexcept
{
    Immutable<const char*> temp;
    if (inStream.read(&temp, sizeof(Immutable<const char*>)))
    {
        str.begin1 = temp.get();
        str.length1 = std::strlen(str.begin1);
        return true;
    }
    else
    {
        return false;
    }
}

// ----=====================================================================----
//     MutableCharStarSerdes
// ----=====================================================================----

MutableCharStarSerdes* MutableCharStarSerdes::instance()
{
    static MutableCharStarSerdes serdes;
    return &serdes;
}

bool MutableCharStarSerdes::deserialize(
        RingBuffer::Stream& inStream, BinaryStream& outStream) const noexcept
{
    std::uint16_t length;
    SplitStringView str;
    if (inStream.read(&length, sizeof(std::uint16_t)))
    {
        auto readLength = inStream.readString(str, length);
        if (readLength)
            outStream << str;
        return readLength == length;
    }
    else
    {
        return false;
    }
}

bool MutableCharStarSerdes::deserialize(
        RingBuffer::Stream& inStream, TextStream& outStream) const noexcept
{
    std::uint16_t length;
    SplitStringView str;
    if (inStream.read(&length, sizeof(std::uint16_t)))
    {
        auto readLength = inStream.readString(str, length);
        if (readLength)
            outStream << str;
        return readLength == length;
    }
    else
    {
        return false;
    }
}

bool MutableCharStarSerdes::deserializeString(
        RingBuffer::Stream& inStream, SplitStringView& str) const noexcept
{
    std::uint16_t length;
    if (inStream.read(&length, sizeof(std::uint16_t)))
    {
        inStream.readString(str, length);
        return true;
    }
    else
    {
        return false;
    }
}

} // namespace log11_detail
} // namespace log11
