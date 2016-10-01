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

#ifndef LOG11_SERDES_HPP
#define LOG11_SERDES_HPP

#include "BinaryStream.hpp"
#include "RingBuffer.hpp"
#include "TextStream.hpp"
#include "Utility.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <tuple>
#include <type_traits>


namespace log11
{
namespace log11_detail
{

struct SerdesOptions
{
    bool isImmutable(const char* str) const noexcept;

    std::uintptr_t immutableStringBegin{0};
    std::uintptr_t immutableStringEnd{0};
};

class SerdesBase
{
public:
    SerdesBase(const SerdesBase&) = delete;
    SerdesBase& operator=(const SerdesBase&) = delete;

    virtual
    ~SerdesBase();

    virtual
    bool deserialize(RingBuffer::Stream& inStream,
                     BinaryStream& outStream) const noexcept = 0;

    virtual
    bool deserialize(RingBuffer::Stream& inStream,
                     TextStream& outStream) const noexcept = 0;

protected:
    SerdesBase() = default;
};

struct SerdesVisitor
{
    // Size calculation

    template <typename... TArgs>
    static
    std::size_t requiredSize(const SerdesOptions& opt,
                             const TArgs&... args) noexcept
    {
        return doRequiredSize(opt, args...);
    }

    template <typename TArg, typename... TArgs>
    static
    std::size_t doRequiredSize(const SerdesOptions& opt,
                               const TArg& arg, const TArgs&... args);

    static
    std::size_t doRequiredSize(const SerdesOptions&)
    {
        return 0;
    }

    // Serialization

    // Return value is true, if no truncation occurred.
    template <typename... TArgs>
    static
    bool serialize(const SerdesOptions& opt, RingBuffer::Stream& stream,
                   const TArgs&... args) noexcept
    {
        return doSerialize(opt, stream, args...);
    }

    template <typename TArg, typename... TArgs>
    static
    bool doSerialize(const SerdesOptions& opt, RingBuffer::Stream& stream,
                     const TArg& arg, const TArgs&... args);

    static
    bool doSerialize(const SerdesOptions&, RingBuffer::Stream&)
    {
        return true;
    }
};

template <typename T>
class Serdes : public SerdesBase
{
public:
    static
    Serdes* instance()
    {
        static Serdes serdes;
        return &serdes;
    }

    static
    std::size_t requiredSize(const SerdesOptions&, const T&) noexcept
    {
        return sizeof(void*) + sizeof(T);
    }

    static
    bool serialize(const SerdesOptions&,
                   RingBuffer::Stream& stream, const T& value) noexcept
    {
        SerdesBase* serdes = instance();
        return stream.write(&serdes, sizeof(void*))
               && stream.write(&value, sizeof(T));
    }

    virtual
    bool deserialize(RingBuffer::Stream& inStream,
                     BinaryStream& outStream) const noexcept override
    {
        T temp;
        if (inStream.read(&temp, sizeof(T)))
        {
            outStream << std::move(temp);
            return true;
        }
        else
        {
            return false;
        }
    }

    virtual
    bool deserialize(RingBuffer::Stream& inStream,
                     TextStream& outStream) const noexcept override
    {
        T temp;
        if (inStream.read(&temp, sizeof(T)))
        {
            outStream << std::move(temp);
            return true;
        }
        else
        {
            return false;
        }
    }
};

class CharStarSerdes : public SerdesBase
{
public:
    static
    std::size_t requiredSize(const SerdesOptions& opt, const char* str) noexcept;

    static
    bool serialize(const SerdesOptions& opt,
                   RingBuffer::Stream& stream, const char* str) noexcept;

    virtual
    bool deserializeString(RingBuffer::Stream& inStream, SplitStringView& str) const noexcept = 0;
};

class ImmutableCharStarSerdes : public CharStarSerdes
{
public:
    static
    ImmutableCharStarSerdes* instance();

    virtual
    bool deserialize(RingBuffer::Stream& inStream,
                     BinaryStream& outStream) const noexcept override;
    virtual
    bool deserialize(RingBuffer::Stream& inStream,
                     TextStream& outStream) const noexcept override;

    virtual
    bool deserializeString(RingBuffer::Stream& inStream, SplitStringView& str) const noexcept override;
};

class MutableCharStarSerdes : public CharStarSerdes
{
public:
    static
    MutableCharStarSerdes* instance();

    virtual
    bool deserialize(RingBuffer::Stream& inStream,
                     BinaryStream& outStream) const noexcept override;

    virtual
    bool deserialize(RingBuffer::Stream& inStream,
                     TextStream& outStream) const noexcept override;

    virtual
    bool deserializeString(RingBuffer::Stream& inStream, SplitStringView& str) const noexcept override;
};

// Serializer for a format tuple, i.e. (const char* format, args...).
class FormatTupleSerdes : public SerdesBase
{
public:
    static
    FormatTupleSerdes* instance();

    template <typename... TArgs>
    static
    std::size_t requiredSize(const SerdesOptions& opt,
                             const FormatTuple<TArgs...>& tuple) noexcept
    {
        return sizeof(void*)
               + sizeof(std::uint16_t)
               + CharStarSerdes::requiredSize(opt, tuple.format)
               + argumentSize(opt, tuple,
                              std::make_index_sequence<sizeof...(TArgs)>());
    }

    template <typename... TArgs, std::size_t... TIndices>
    static
    std::size_t argumentSize(const SerdesOptions& opt,
                             const FormatTuple<TArgs...>& tuple,
                             std::index_sequence<TIndices...>) noexcept
    {
        return SerdesVisitor::requiredSize(opt, std::get<TIndices>(tuple.args)...);
    }

    template <typename... TArgs>
    static
    bool serialize(const SerdesOptions& opt, RingBuffer::Stream& stream,
                   const FormatTuple<TArgs...>& tuple) noexcept
    {
        SerdesBase* serdes = instance();
        if (!stream.write(&serdes, sizeof(void*)))
            return false;

        RingBuffer::Stream backup = stream;
        stream.skip(sizeof(std::uint16_t));

        if (!CharStarSerdes::serialize(opt, stream, tuple.format))
            return false;
        bool complete = serializeArguments(
                    opt, stream, tuple,
                    std::make_index_sequence<sizeof...(TArgs)>());

        std::uint16_t length = stream.begin() - backup.begin();
        backup.write(length);
        return complete;
    }

    template <typename... TArgs, std::size_t... TIndices>
    static
    bool serializeArguments(const SerdesOptions& opt, RingBuffer::Stream& stream,
                     const FormatTuple<TArgs...>& tuple,
                     std::index_sequence<TIndices...>) noexcept
    {
        return SerdesVisitor::serialize(opt, stream, std::get<TIndices>(tuple.args)...);
    }

    virtual
    bool deserialize(RingBuffer::Stream& inStream,
                     BinaryStream& outStream) const noexcept override;

    virtual
    bool deserialize(RingBuffer::Stream& inStream,
                     TextStream& outStream) const noexcept override;
};

// ----=====================================================================----
//     SerdesSelector
// ----=====================================================================----

// The SerdesSelector is a utility for mapping types to the corresponding
// serdes.
// - All pointer types are mapped to Serdes<const void*>

template <typename T>
struct SerdesSelector
{
    using type = Serdes<T>;
};

template <typename TPointee>
struct PointerSerdesSelector
{
    using type = Serdes<const void*>;
};

template <>
struct PointerSerdesSelector<char>
{
    using type = CharStarSerdes;
};

// Fold the serializers for pointers into void* serializers
// (except for C-strings).
template <typename T>
struct SerdesSelector<T*> : public PointerSerdesSelector<std::remove_cv_t<T>>
{
};

template <typename... T>
struct SerdesSelector<FormatTuple<T...>>
{
    using type = FormatTupleSerdes;
};

template <typename T>
using serdes_t = typename SerdesSelector<std::decay_t<T>>::type;

// ----=====================================================================----
//     SerdesDriver implementation
// ----=====================================================================----

template <typename TArg, typename... TArgs>
std::size_t SerdesVisitor::doRequiredSize(
        const SerdesOptions& opt,
        const TArg& arg, const TArgs&... args)
{
    return serdes_t<TArg>::requiredSize(opt, arg)
           + doRequiredSize(opt, args...);
}

template <typename TArg, typename... TArgs>
bool SerdesVisitor::doSerialize(
        const SerdesOptions& opt, RingBuffer::Stream& stream,
        const TArg& arg, const TArgs&... args)
{
    return serdes_t<TArg>::serialize(opt, stream, arg) && doSerialize(opt, stream, args...);
}

} // namespace log11_detail
} // namespace log11

#endif // LOG11_SERDES_HPP
