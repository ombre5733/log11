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
};

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

// Fold the serializers for pointer into void* serializers
// (except for C-strings).
template <typename T>
struct SerdesSelector<T*> : public PointerSerdesSelector<std::remove_cv_t<T>>
{
};

template <typename T>
using serdes_t = typename SerdesSelector<std::decay_t<T>>::type;

} // namespace log11_detail
} // namespace log11

#endif // LOG11_SERDES_HPP
