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

#ifndef LOG11_UTILITY_HPP
#define LOG11_UTILITY_HPP

#include "Config.hpp"
#include "TypeInfo.hpp"

#include <atomic>
#include <cstddef>
#include <tuple>
#include <type_traits>
#include <utility>

#ifdef LOG11_USE_WEOS
#include <weos/utility.hpp>
#endif


namespace log11
{
class LogRecordData;

// ----=====================================================================----
//     Immutable
// ----=====================================================================----

template <typename T>
class Immutable;

//! A pointer to an immutable string (a string residing in the ROM area).
template <>
class Immutable<const char*>
{
public:
    using type = const char*;

    Immutable() = default;

    constexpr explicit
    Immutable(type v) noexcept
        : m_value(v)
    {
    }

    constexpr
    type get() const noexcept
    {
        return m_value;
    }

    constexpr explicit
    operator type() const noexcept
    {
        return m_value;
    }

private:
    type m_value;
};

// ----=====================================================================----
//     SplitStringView
// ----=====================================================================----

struct SplitStringView
{
    const char* begin1;
    std::size_t length1;
    const char* begin2;
    std::size_t length2;
};

namespace log11_detail
{

// ----=====================================================================----
//     Built-in types
// ----=====================================================================----

template <typename... T>
struct TypeList {};

template <typename T, typename U>
struct IsMember;

template <typename T, typename TH, typename... TL>
struct IsMember<T, TypeList<TH, TL...>>
        : std::conditional<std::is_same<T, TH>::value,
                           std::true_type,
                           IsMember<T, TypeList<TL...>>>::type
{
};

template <typename T>
struct IsMember<T, TypeList<>> : std::false_type {};



using BuiltInTypes = TypeList<bool,
                              char,
                              signed char,
                              unsigned char,
                              short,
                              unsigned short,
                              int,
                              unsigned int,
                              long,
                              unsigned long,
                              long long,
                              unsigned long long,
                              float,
                              double,
                              long double,
                              Immutable<const char*>,
                              SplitStringView>;

template <typename T>
struct IsBuiltin : std::integral_constant<
                       bool,
                       IsMember<std::decay_t<T>, BuiltInTypes>::value
                       || std::is_pointer<std::decay_t<T>>::value>
{
};

// ----=====================================================================----
//     integral_constant inversion
// ----=====================================================================----

template <typename T>
struct NotType;

template <bool B>
struct NotType<std::integral_constant<bool, B>>
{
    using type = std::integral_constant<bool, !B>;
};

template <typename T>
using Not = typename NotType<T>::type;

// ----=====================================================================----
//     ScratchPad
// ----=====================================================================----

//! A scratch pad.
class ScratchPad
{
public:
    ScratchPad(unsigned capacity);
    ~ScratchPad();

    ScratchPad(const ScratchPad&) = delete;
    ScratchPad& operator=(const ScratchPad&) = delete;

    void resize(unsigned capacity);

    void clear() noexcept;

    void push(char ch);
    void push(const char* data, unsigned size);

    const char* data() const noexcept;
    unsigned size() const noexcept;

private:
    char* m_data;
    unsigned m_capacity;
    unsigned m_size;
};

// ----=====================================================================----
//     RecordHeaderGenerator
// ----=====================================================================----

//! A generator for the header of a log record.
class RecordHeaderGenerator
{
public:
    explicit
    RecordHeaderGenerator();

    virtual
    ~RecordHeaderGenerator();

    RecordHeaderGenerator(const RecordHeaderGenerator&) = delete;
    RecordHeaderGenerator& operator=(const RecordHeaderGenerator&) = delete;

    void generate(LogRecordData& record, log11_detail::ScratchPad& pad);

    virtual
    void append(LogRecordData& record, log11_detail::ScratchPad& pad) = 0;

    static
    RecordHeaderGenerator* parse(const char* str);

    RecordHeaderGenerator* m_next;
};

// ----=====================================================================----
//     FormatTuple
// ----=====================================================================----

template <typename... TArgs>
struct FormatTuple
{
    template <typename... U>
    explicit
    FormatTuple(const char* fmt, U&&... args)
        : format(fmt),
          args(std::forward<U>(args)...)
    {
    }

    const char* format;
    std::tuple<TArgs...> args;
};

template <typename TArg, typename... TArgs>
FormatTuple<TArg&&, TArgs&&...> makeFormatTuple(const char* format,
                                                TArg&& arg,
                                                TArgs&&... args)
{
    return FormatTuple<TArg&&, TArgs&&...>(
                format, std::forward<TArg>(arg), std::forward<TArgs>(args)...);
}

inline
const char* makeFormatTuple(const char* format)
{
    return format;
}

// ----=====================================================================----
//     Argument decaying
// ----=====================================================================----

template <typename T>
struct IsAtomic : public std::false_type {};

template <typename T>
struct IsAtomic<std::atomic<T>> : public std::true_type {};


template <typename T>
struct Decayer
{
    template <typename U>
    static constexpr
    std::decay_t<U> decay(U&& x) noexcept
    {
        return std::forward<U>(x);
    }
};

template <typename T>
struct EnumDecayer
{
    static constexpr
    std::underlying_type_t<T> decay(T x) noexcept
    {
        return static_cast<std::underlying_type_t<T>>(x);
    }
};

template <typename T>
struct AtomicDecayer;

template <typename T>
struct AtomicDecayer<std::atomic<T>>
{
    static
    T decay(const std::atomic<T>& x) noexcept
    {
        return x.load();
    }
};


template <typename T>
using Decayer_t = typename std::conditional_t<
        std::is_enum<T>::value && (TreatAsInteger<T>::value || std::is_convertible<T, int>::value),
        EnumDecayer<T>,
        std::conditional_t<IsAtomic<T>::value,
                           AtomicDecayer<T>,
                           Decayer<T>>>;

template <typename T>
auto decayArgument(T&& x) -> decltype(Decayer_t<std::decay_t<T>>::decay(std::forward<T>(x)))
{
    return Decayer_t<std::decay_t<T>>::decay(std::forward<T>(x));
}

} // namespace log11_detail
} // namespace log11

#endif // LOG11_UTILITY_HPP
