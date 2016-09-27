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

#ifndef LOG11_SPLITSTRING_HPP
#define LOG11_SPLITSTRING_HPP

#include <cstddef>
#include <type_traits>


namespace log11
{

#if 0
class StringLiteral
{
public:
    template <typename T,
              typename = std::enable_if_t<std::is_same<std::decay_t<T>, const char*>::value>>
    StringLiteral(T str)
        : m_data(str),
          m_size(str ? std::strlen(str) : 0)
    {
    }

    StringLiteral(const char* str, std::size_t length)
        : m_data(str),
          m_size(length)
    {
    }

    template <std::size_t N>
    StringLiteral(const char (&str)[N])
        : m_data(str),
          m_size(N)
    {
    }

    const char* data() const noexcept
    {
        return m_data;
    }

    std::size_t size() const noexcept
    {
        return m_size;
    }

private:
    const char* m_data;
    std::size_t m_size;
};
#endif

namespace log11_detail
{

struct SplitString
{
    const char* begin1;
    const char* begin2;
    unsigned length1;
    unsigned length2;
};

} // namespace log11_detail
} // namespace log11

#endif // LOG11_SPLITSTRING_HPP
