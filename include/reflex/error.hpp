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
@file      error.hpp
@brief     RE/flex regex errors
@author    Robert van Engelen - engelen@genivia.com
@copyright (c) 2016-2020, Robert van Engelen, Genivia Inc. All rights reserved.
@copyright (c) BSD-3 License - see LICENSE.txt
*/

#ifndef REFLEX_ERROR_H
#define REFLEX_ERROR_H

#include <cstdio>
#include <stdexcept>
#include <string>

namespace reflex {

inline std::string ztoa(size_t n)
{
  char buf[24];
#if (defined(__WIN32__) || defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(__BORLANDC__)) && !defined(__CYGWIN__) && !defined(__MINGW32__) && !defined(__MINGW64__)
  sprintf_s(buf, sizeof(buf), "%zu", n);
#else
  snprintf(buf, sizeof(buf), "%zu", n);
#endif
  return std::string(buf);
}

/// Regex syntax error exceptions.
class regex_error : public std::runtime_error {
 public:
  enum error_code{
    mismatched_parens,    ///< mismatched ( )
    mismatched_braces,    ///< mismatched { }
    mismatched_brackets,  ///< mismatched [ ]
    mismatched_quotation, ///< mismatched `\Q...\E` or `"..."` quotation
    empty_expression,     ///< regex (sub)expression should not be empty
    empty_class,          ///< class `[...]` is empty, e.g. `[a&&[b]]`
    invalid_class,        ///< invalid character class name or code point
    invalid_class_range,  ///< invalid character class range, e.g. `[Z-A]`
    invalid_escape,       ///< invalid escape character
    invalid_anchor,       ///< invalid anchor
    invalid_repeat,       ///< invalid repeat range, e.g. `{10,1}`
    invalid_quantifier,   ///< invalid lazy/possessive quantifier
    invalid_modifier,     ///< invalid `(?ismx:)` modifier
    invalid_collating,    ///< invalid collating element [[.name.]]
    invalid_backreference,///< invalid backreference
    invalid_syntax,       ///< invalid regex syntax
    exceeds_length,       ///< regex exceeds length limit (reflex::Pattern class only)
    exceeds_limits,       ///< regex exceeds complexity limits
    undefined_name,       ///< undefined macro name (reflex tool only)
  };
  regex_error(
      error_code   code,
      std::string_view pattern,
      size_t           pos = 0)
    :
      std::runtime_error(regex_error_message_code(code, pattern, pos)),
      code_(code),
      pos_(pos)
  { }
  regex_error(
      std::string_view message,
      std::string_view pattern,
      size_t           pos = 0)
    :
      std::runtime_error(regex_error_message(message, pattern, pos)),
      code_(invalid_syntax),
      pos_(pos)
  { }
  /// @returns error code, a reflex::error_code constant.
  error_code code() const
  {
    return code_;
  }
  /// @returns position of the error in the regex.
  size_t pos() const
  {
    return pos_;
  }
 private:
  static std::string regex_error_message_code(error_code code, std::string_view pattern, size_t pos)
  {
    static constexpr const char* messages[19] = {
      "mismatched ( )",
      "mismatched { }",
      "mismatched [ ]",
      "mismatched quotation",
      "empty expression",
      "empty character class",
      "invalid character class",
      "invalid character class range",
      "invalid escape",
      "invalid anchor or boundary",
      "invalid repeat",
      "invalid quantifier",
      "invalid modifier",
      "invalid collating element",
      "invalid backreference",
      "invalid syntax",
      "exceeds length limit",
      "exceeds complexity limits",
      "undefined name",
    };
    return regex_error_message(messages[code], pattern, pos);
  }

  static std::string regex_error_message(std::string_view message, std::string_view pattern, size_t pos)
  {
    size_t l = pattern.size();
    if (pos > l)
      pos = l;
    l = message.size();
    size_t n = pos / 40;
    size_t k = pos % 40 + (n == 0 ? 0 : 20);
    const char* pattern_d = pattern.data();
    const char *p = n == 0 ? pattern_d : pattern_d + 40 * n - 20;
    while (p > pattern && (*p & 0xc0) == 0x80)
    {
      --p;
      ++k;
    }
    size_t m = disppos(p, 79).data() - p;
    size_t r = displen(p, k);
    std::string what("error in regex at position ");
    what.append(ztoa(pos)).append("\n").append(p, m).append("\n");
    if (r >= l + 4)
      what.append(r - l - 4, ' ').append(message).append("___/\n");
    else
      what.append(r, ' ').append("\\___").append(message).append("\n");
    return what;
  }

  static size_t displen(std::string_view s, size_t k)
  {
    size_t n = 0;
    while (k > 0 && s.size()>0)
    {
      unsigned char c = s[0]; s.remove_prefix(1);
      if (c >= 0x80)
      {
        if (c >= 0xf0 &&
            (c > 0xf0 ||
             (static_cast<unsigned char>(s[0]) >= 0x9f &&
              (static_cast<unsigned char>(s[0]) > 0x9f ||
               (static_cast<unsigned char>(s[1]) >= 0x86 &&
                (static_cast<unsigned char>(s[1]) > 0x86 ||
                 static_cast<unsigned char>(s[2]) >= 0x8e))))))
        {
          // U+1F18E (UTF-8 F0 9F 86 8E) and higher is usually double width
          ++n;
          if (k < 4)
            break;
          s.remove_prefix((s[0] != '\0') + (s[1] != '\0') + (s[2] != 0));
          k -= 3;
        }
        else
        {
          while (k > 1 && (s[0] & 0xc0) == 0x80)
          {
            s.remove_prefix(1);
            --k;
          }
        }
      }
      ++n;
      --k;
    }
    return n;
  }

  static std::string_view disppos(std::string_view s, size_t k)
  {
    while (k > 0 && s.size()>0)
    {
      unsigned char c = s[0]; s.remove_prefix(1);
      if (c >= 0x80)
      {
        if (c >= 0xf0 &&
            (c > 0xf0 ||
             (static_cast<unsigned char>(s[0]) >= 0x9f &&
              (static_cast<unsigned char>(s[0]) > 0x9f ||
               (static_cast<unsigned char>(s[1]) >= 0x86 &&
                (static_cast<unsigned char>(s[1]) > 0x86 ||
                 static_cast<unsigned char>(s[2]) >= 0x8e))))))
        {
          // U+1F18E (UTF-8 F0 9F 86 8E) and higher is usually double width
          if (k < 4)
            break;
          s.remove_prefix((s[0] != '\0') + (s[1] != '\0') + (s[2] != 0));
          k -= 3;
        }
        else
        {
          while (k > 1 && (s[0] & 0xc0) == 0x80)
          {
            s.remove_prefix(1);
            --k;
          }
        }
      }
      --k;
    }
    return s;
  }

  error_code code_;
  size_t pos_;
};

} // namespace reflex

#endif
