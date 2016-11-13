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

#ifndef IO11_IO11_HPP
#define IO11_IO11_HPP

#include <cstdint>
#include <type_traits>


namespace log11
{

template <typename T>
struct TypeTraits;

//! The first tag, which can be used for a user-defined type.
//! All user defined tags must be in the range
//! <tt>user_defined_type_tag_begin <= tag < user_defined_type_tag_end</tt>.
static constexpr std::uint32_t user_defined_type_tag_begin = 1024;

//! The last tag, which cannot be used for a user-defined type anymore.
//! All user defined tags must be in the range
//! <tt>user_defined_type_tag_begin <= tag < user_defined_type_tag_end</tt>.
static constexpr std::uint32_t user_defined_type_tag_end = 4096;

//! Automatically create a serializer for the given \p type.
#define IO11_AUTO_SERIALIZE(type)                                              \
    namespace log11                                                             \
    {                                                                          \
    template <>                                                                \
    struct TypeTraits<type>                                                    \
    {                                                                          \
        static                                                                 \
        std::uint32_t typeTag();                                               \
        static                                                                 \
        void write(::log11::BinaryStream& s, const type& v);                   \
    };                                                                         \
    }



template <typename T>
struct _io11_AutoSerialize
{
};

} // namespace log11

#endif // IO11_IO11_HPP
