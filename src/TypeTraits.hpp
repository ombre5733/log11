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

#ifndef LOG11_TYPETRAITS_HPP
#define LOG11_TYPETRAITS_HPP

#include "io11.hpp"

#include <cstdint>
#include <type_traits>


namespace log11
{

//! \brief A traits class to make enum behave like integers.
//!
//! A specialization TreatAsInteger<E> must derive from <tt>std::true_type</tt>
//! in order to enable automatic enum to integer conversion.
template <typename T>
struct TreatAsInteger : public std::false_type
{
};

//! Makes the enum \p e behave like an integer when passed to the logger.
#define LOG11_TREAT_ENUM_AS_INTEGER(e)                                         \
    namespace log11                                                            \
    {                                                                          \
        template <>                                                            \
        struct TreatAsInteger<e> : public std::true_type                       \
        {                                                                      \
        };                                                                     \
    }

} // namespace log11

#endif // LOG11_TYPETRAITS_HPP
