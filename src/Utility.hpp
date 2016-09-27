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

template <typename T>
class Immutable
{
public:
    Immutable() = default;

    constexpr explicit
    Immutable(T v)
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
                               long double//,
                               >;//StringRef>;

template <typename T>
struct is_custom : std::integral_constant<
                       bool,
                       !is_member<std::decay_t<T>, builtin_types>::value
                       && !std::is_pointer<T>::value>
{
};

} // namespace log11_detail
} // namespace log11

#endif // LOG11_UTILITY_HPP
