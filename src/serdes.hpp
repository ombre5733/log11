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

#include "ringbuffer.hpp"

#include <cstddef>
#include <utility>

#ifdef LOG11_USE_WEOS
#include <weos/type_traits.hpp>
#else
#include <type_traits>
#endif // LOG11_USE_WEOS


namespace log11
{
namespace log11_detail
{

template <typename... T>
struct TypeList {};



template <typename T, typename U>
struct is_member;

template <typename T, typename TH, typename... TL>
struct is_member<T, TypeList<TH, TL...>>
        : LOG11_STD::conditional<LOG11_STD::is_same<T, TH>::value,
                                 LOG11_STD::true_type,
                                 is_member<T, TypeList<TL...>>>::type
{
};

template <typename T>
struct is_member<T, TypeList<>> : LOG11_STD::false_type {};



using serializable_types = TypeList<bool,
                                    char,
                                    signed char,
                                    unsigned char,
                                    short,
                                    unsigned short,
                                    int,
                                    unsigned,
                                    long,
                                    unsigned long,
                                    long long,
                                    unsigned long long,
                                    float,
                                    double,
                                    long double>;



template <typename T>
struct is_serializable : is_member<T, serializable_types> {};

template <typename T>
struct is_serializable<T*> : LOG11_STD::true_type {};



template <typename... T>
struct all_serializable : LOG11_STD::true_type {};

template <typename TH, typename... TL>
struct all_serializable<TH, TL...>
        : LOG11_STD::conditional<is_serializable<TH>::value != false,
                                 all_serializable<TL...>,
                                 LOG11_STD::false_type>::type
{
};



class Visitor
{
public:
    virtual
    ~Visitor() {}

    virtual void visit(bool value) = 0;
    virtual void visit(char value) = 0;
    virtual void visit(signed char value) = 0;
    virtual void visit(unsigned char value) = 0;
    virtual void visit(short value) = 0;
    virtual void visit(unsigned short value) = 0;
    virtual void visit(int value) = 0;
    virtual void visit(unsigned value) = 0;
    virtual void visit(long value) = 0;
    virtual void visit(unsigned long value) = 0;
    virtual void visit(long long value) = 0;
    virtual void visit(unsigned long long value) = 0;
    virtual void visit(float value) = 0;
    virtual void visit(double value) = 0;
    virtual void visit(long double value) = 0;

    virtual void visit(const void* value) = 0;
    virtual void visit(const char* value) = 0;

    virtual void outOfBounds() = 0;
};

class SerdesBase
{
public:
    virtual
    ~SerdesBase() {}

    virtual
    std::size_t numArguments() const noexcept = 0;

    virtual
    void apply(RingBuffer& buffer, RingBuffer::ByteRange range,
               std::size_t index, Visitor& visitor) = 0;

protected:
    template <typename TArg, typename... TArgs>
    static
    std::size_t doRequiredSize(const TArg& arg, const TArgs&... args)
    {
        return sizeof(arg) + doRequiredSize(args...);
    }

    static
    std::size_t doRequiredSize()
    {
        return 0;
    }

    template <typename TArg, typename... TArgs>
    static
    void doSerialize(RingBuffer& buffer, RingBuffer::ByteRange range,
                     const TArg& arg, const TArgs&... args)
    {
        if (range.length >= sizeof(TArg))
        {
            range = buffer.write(&arg, range, sizeof(TArg));
            doSerialize(buffer, range, args...);
        }
    }

    static
    void doSerialize(RingBuffer&, RingBuffer::ByteRange)
    {
    }

    template <typename TArg, typename... TArgs>
    static
    void doApply(TypeList<TArg, TArgs...>,
                 RingBuffer& buffer, RingBuffer::ByteRange range,
                 std::size_t argIndex, Visitor& visitor)
    {
        if (range.length >= sizeof(TArg))
        {
            if (argIndex == 0)
            {
                TArg temp;
                buffer.read(range, &temp, sizeof(TArg));
                visitor.visit(temp);
            }
            else
            {
                range.begin += sizeof(TArg);
                range.length -= sizeof(TArg);
                doApply(TypeList<TArgs...>(),
                        buffer, range, argIndex - 1, visitor);
            }
        }
        else
        {
            visitor.outOfBounds();
        }
    }

    static
    void doApply(TypeList<>,
                 RingBuffer& /*buffer*/, const RingBuffer::ByteRange& /*range*/,
                 std::size_t /*argIndex*/, Visitor& /*visitor*/)
    {
    }
};

template <typename... T>
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
    std::size_t requiredSize(const T&... args)
    {
        return SerdesBase::doRequiredSize(args...);
    }

    static
    void serialize(RingBuffer& buffer, RingBuffer::ByteRange range,
                   const T&... args)
    {
        SerdesBase::doSerialize(buffer, range, args...);
    }

    virtual
    std::size_t numArguments() const noexcept override
    {
        return sizeof...(T);
    }

    virtual
    void apply(RingBuffer& buffer, RingBuffer::ByteRange range,
               std::size_t index, Visitor& visitor) override
    {
        if (index < numArguments())
            SerdesBase::doApply(TypeList<T...>(), buffer, range, index, visitor);
        else
            visitor.outOfBounds();
    }
};

} // namespace log11_detail
} // namespace log11

#endif // LOG11_SERDES_HPP
