/*-
 * Copyright (c) 2021 Samantha Payson
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of its contributors may
 *    be used to endorse or promote products derived from this software
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS "AS IS" AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */

#ifndef CRPG_UTIL_H
#define CRPG_UTIL_H

#include <cstdlib>

#include <string>

#define __CRPG_32_BIT_PRIME 4294967291

template <size_t N> 
static inline constexpr uint32_t ID(char const (&s)[N]) noexcept {
  uint64_t val = 0;

  for (size_t i = 0; i < N - 1; i++) {
    val  = (val << 8) + (uint32_t)s[i];
    val %= __CRPG_32_BIT_PRIME;
  }

  return (uint32_t)(val & 0xFFFFFFFF);
}

static inline  uint32_t ID(std::string const &s) {
  uint64_t val = 0;

  for (auto c : s) {
    val  = (val << 8) + (uint32_t)c;
    val %= __CRPG_32_BIT_PRIME;
  }

  return (uint32_t)(val & 0xFFFFFFFF);
}

#endif // util.h
