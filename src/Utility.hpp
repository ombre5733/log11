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

#include <type_traits>


namespace log11
{
class LogRecordData;
class SplitStringView;

template <typename T>
class Format;

template <typename T>
class Immutable;



namespace log11_detail
{
template <typename T>
struct is_format : std::false_type
{
};

template <typename T>
struct is_format<Format<T>> : std::true_type
{
};

template <typename T>
struct is_immutable : std::false_type
{
};

template <typename T>
struct is_immutable<Immutable<T>> : std::true_type
{
};

} // namespace log11_detail



//! Marks a string as immutable.
template <typename T>
class Immutable
{
public:
    static_assert(!log11_detail::is_immutable<T>::value, "");

    Immutable() = default;

    constexpr explicit
    Immutable(T v) noexcept
        : m_value(v)
    {
    }

    constexpr
    const char* get() const noexcept
    {
        return m_value;
    }

    constexpr explicit
    operator T() const noexcept
    {
        return m_value;
    }

private:
    T m_value;
};

//! Marks a string as format string.
template <typename T>
class Format
{
public:
    static_assert(!log11_detail::is_format<T>::value, "");
    static_assert(!log11_detail::is_immutable<T>::value, "");

    Format() = default;

    constexpr explicit
    Format(T v)
        : m_value(v)
    {
    }

    explicit
    operator T()
    {
        return m_value;
    }

private:
    T m_value;
};



namespace log11_detail
{

template <typename... T>
struct TypeList {};



template <typename T, typename U>
struct is_member;

template <typename T, typename TH, typename... TL>
struct is_member<T, TypeList<TH, TL...>>
        : std::conditional<std::is_same<T, TH>::value,
                           std::true_type,
                           is_member<T, TypeList<TL...>>>::type
{
};

template <typename T>
struct is_member<T, TypeList<>> : std::false_type {};



using builtin_types = TypeList<bool,
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
                               SplitStringView>;

template <typename T>
struct is_builtin : std::integral_constant<
                        bool,
                        is_member<std::decay_t<T>, builtin_types>::value
                        || std::is_pointer<T>::value>
{
};



template <typename T>
struct NotType;

template <bool B>
struct NotType<std::integral_constant<bool, B>>
{
    using type = std::integral_constant<bool, !B>;
};

template <typename T>
using not_t = typename NotType<T>::type;



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

} // namespace log11_detail
} // namespace log11

#endif // LOG11_UTILITY_HPP
