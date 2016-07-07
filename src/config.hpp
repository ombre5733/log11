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

#ifndef LOG11_CONFIG_HPP
#define LOG11_CONFIG_HPP

// If the macro LOG11_USER_CONFIG is set, it points to the user's
// configuration file. If the macro is not set, we assume that the
// user configuration is somewhere in the path.
#if defined(LOG11_USER_CONFIG)
    #include LOG11_USER_CONFIG
#else
    #include "log11_user_config.hpp"
#endif // LOG11_USER_CONFIG

// Check the version of the user configuration file.
#if LOG11_USER_CONFIG_VERSION != 1
    #error "Version 1 of the log11 user configuration is required."
#endif // LOG11_USER_CONFIG_VERSION

// ----=====================================================================----
//     WEOS integration
// ----=====================================================================----

#ifdef LOG11_USE_WEOS
    #define LOG11_EXCEPTION(x)   WEOS_EXCEPTION(x)
#else
    #define LOG11_EXCEPTION(x)   x
#endif // LOG11_USE_WEOS

#endif // LOG11_CONFIG_HPP

