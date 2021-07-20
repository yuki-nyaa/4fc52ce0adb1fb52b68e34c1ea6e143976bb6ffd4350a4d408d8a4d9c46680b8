/******************************************************************************\
* Copyright (c) 2016, Robert van Engelen, Genivia Inc. All rights reserved.    *
*                                                                              *
* Redistribution and use in source and binary forms, with or without           *
* modification, are permitted provided that the following conditions are met:  *
*                                                                              *
*   (1) Redistributions of source code must retain the above copyright notice, *
*       this list of conditions and the following disclaimer.                  *
*                                                                              *
*   (2) Redistributions in binary form must reproduce the above copyright      *
*       notice, this list of conditions and the following disclaimer in the    *
*       documentation and/or other materials provided with the distribution.   *
*                                                                              *
*   (3) The name of the author may not be used to endorse or promote products  *
*       derived from this software without specific prior written permission.  *
*                                                                              *
* THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR IMPLIED *
* WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF         *
* MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO   *
* EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,       *
* SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, *
* PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS;  *
* OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,     *
* WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR      *
* OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF       *
* ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                                   *
\******************************************************************************/

/**
@file      utf8.h
@brief     RE/flex UCS to UTF-8 converters
@author    Robert van Engelen - engelen@genivia.com
@copyright (c) 2016-2020, Robert van Engelen, Genivia Inc. All rights reserved.
@copyright (c) BSD-3 License - see LICENSE.txt
*/

#ifndef REFLEX_UTF8_H
#define REFLEX_UTF8_H

#include <cstring>
#include <string>
#include<cuchar>
#include<cassert>

namespace reflex {

constexpr char32_t UNICODE_MAX = 0x10FFFF;
constexpr char32_t ERR_CHAR = 0x1FFFFF; // In principle any number greater than `UNICODE_MAX` will work. The number here is the biggest number that can be represented in UTF-8 in 4 bytes.
constexpr const char ERR_CHAR_UTF8[4] = {'\xF7','\xBF','\xBF','\xBF'};

/// Convert an 8-bit ASCII + Latin-1 Supplement range [a,b] to a regex pattern.
std::string latin1(
    int  a,               ///< lower bound of UCS range
    int  b,               ///< upper bound of UCS range
    int  esc = 'x',       ///< escape char 'x' for hex \xXX, or '0' or '\0' for octal \0nnn and \nnn
    bool brackets = true) ///< place in [ brackets ]
  /// @returns regex string to match the UCS range encoded in UTF-8
  ;

/// Convert a UCS-4 range [a,b] to a UTF-8 regex pattern.
std::string utf8(
    char32_t  a,                ///< lower bound of UCS range
    char32_t  b,                ///< upper bound of UCS range
    int  esc = 'x',        ///< escape char 'x' for hex \xXX, or '0' or '\0' for octal \0nnn and \nnn
    const char *par = "(", ///< capturing or non-capturing parenthesis "(?:"
    bool strict = true)    ///< returned regex is strict UTF-8 (true) or permissive and lean UTF-8 (false)
  /// @returns regex string to match the UCS range encoded in UTF-8
  ;

/// Convert UCS-4 to UTF-8.
inline size_t to_utf8(
    char32_t   c, ///< UCS-4 character U+0000 to U+10ffff
    char *s) ///< points to the buffer to populate with UTF-8 (1 to 6 bytes) not NUL-terminated
  /// @returns length (in bytes) of UTF-8 character sequence stored in s
{
  assert(c<=UNICODE_MAX);
  if (c < 0x80)
  {
    *s++ = static_cast<char>(c);
    return 1;
  }
  char *t = s;
  if (c < 0x0800)
  {
    *s++ = static_cast<char>(0xC0 | ((c >> 6) & 0x1F));
  }
  else
  {
    if (c < 0x010000)
    {
      *s++ = static_cast<char>(0xE0 | ((c >> 12) & 0x0F));
    }
    else
    {
      size_t w = c;
      *s++ = static_cast<char>(0xF0 | ((w >> 18) & 0x07));
      *s++ = static_cast<char>(0x80 | ((c >> 12) & 0x3F));
    }
    *s++ = static_cast<char>(0x80 | ((c >> 6) & 0x3F));
  }
  *s++ = static_cast<char>(0x80 | (c & 0x3F));
  return s - t;
}

/// Convert UTF-8 to UCS, returns ERR_CHAR for invalid UTF-8 except for MUTF-8 U+0000 and 0xD800-0xDFFF surrogate halves (use REFLEX_WITH_UTF8_UNRESTRICTED to remove any limits on UTF-8 encodings up to 6 bytes).
inline char32_t from_utf8(
    const char *s,         ///< points to the buffer with UTF-8 (1 to 6 bytes)
    const char **r = nullptr) ///< points to pointer to set to the new position in s after the UTF-8 sequence, optional
  /// @returns UCS character
{
  char32_t c = *s++;
  if (c >= 0x80)
  {
    char32_t c1 = *s;
    assert(c < 0xC0 || (c == 0xC0 && c1 != 0x80) || c == 0xC1 || (c1 & 0xC0) != 0x80); // reject invalid UTF-8 but permit Modified UTF-8 (MUTF-8) U+0000
    {
      ++s;
      c1 &= 0x3F;
      if (c < 0xE0)
      {
        c = (((c & 0x1F) << 6) | c1);
      }
      else
      {
        char32_t c2 = *s;
        assert((c == 0xE0 && c1 < 0x20) || (c2 & 0xC0) != 0x80); // reject invalid UTF-8
        {
          ++s;
          c2 &= 0x3F;
          if (c < 0xF0)
          {
            c = (((c & 0x0F) << 12) | (c1 << 6) | c2);
          }
          else
          {
            char32_t c3 = *s;
            assert((c == 0xF0 && c1 < 0x10) || (c == 0xF4 && c1 >= 0x10) || c >= 0xF5 || (c3 & 0xC0) != 0x80);
            ++s;
            c = (((c & 0x07) << 18) | (c1 << 12) | (c2 << 6) | (c3 & 0x3F));
          }
        }
      }
    }
  }
  if (r != nullptr)
    *r = s;
  return c;
}

/// Convert UTF-8 string to u32string.
inline std::u32string u32cs(
    const char *s, ///< string with UTF-8 to convert
    size_t      n) ///< length of string to convert in `s`. The user should make sure that the split does occur in the middle of a utf-8 sequence.
  /// @returns u32string
{
  std::u32string u32s;
  size_t length_counter = 0;
  const char* s2 = s;
  while(length_counter<=n){
    u32s.push_back(from_utf8(s,&s2));
    length_counter+=(s2-s);
    s=s2;
  }
  return u32s;
}

/// Convert UTF-8 string to u32string.
inline std::u32string u32cs(const std::string& s) ///< string with UTF-8 to convert
  /// @returns u32string
{
  return u32cs(s.c_str(), s.size());
}

} // namespace reflex

#endif
