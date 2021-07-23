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

inline constexpr char32_t UNICODE_MAX = 0x10FFFF;
inline constexpr char32_t ERR_CHAR = 0x1FFFFF; // In principle any number greater than `UNICODE_MAX` will work. The number here is the biggest number that can be represented in UTF-8 in 4 bytes.
inline constexpr const char ERR_CHAR_UTF8[4] = {'\xF7','\xBF','\xBF','\xBF'};

template<typename T>
struct is_char {static constexpr bool value = false;};
template<>
struct is_char<char> {static constexpr bool value = true;};
template<>
struct is_char<unsigned char> {static constexpr bool value = true;};
template<>
struct is_char<signed char> {static constexpr bool value = true;};
template<typename T>
inline constexpr bool is_char_v = is_char<T>::value;

template<typename D,typename S>
void char_copy(D* dst,const S* src,size_t count){
  static_assert(is_char_v<std::remove_cv_t<D>> && is_char_v<std::remove_cv_t<S>>);
  if constexpr(std::is_same_v<std::remove_cv_t<D>,std::remove_cv_t<S>>){
    std::memcpy(dst,src,count*sizeof(D));
  }else{
    for(size_t i=0;i<count;++i)
      *dst++ = static_cast<D>(*src++);
  }
}

/// Convert `a` to a null-terminated regex str at `buf`. Non-printable chars as well as regex keychars are converted to octal escape "\0xxx".
inline const char *regex_char(char *buf, char a, char esc, size_t *n = nullptr)
{
  static constexpr const char digits[17] = "0123456789abcdef";
  if (a >= '!' && a <= '~' && a != '#' && a != '-' && a != '[' && a != '\\' && a != ']' && a != '^' &&
      (n != nullptr || (a <= 'z' && a != '$' && a != '(' && a != ')' && a != '*' && a != '+' && a != '.' && a != '?')) //QUES `n!=nullptr` ?
     )
  {
    buf[0] = a;
    buf[1] = '\0';
    if (n)
      *n = 1;
  }
  else
  {
    unsigned char ua = static_cast<unsigned char>(a);
    buf[0] = '\\';
    switch(esc){
      case 'x' :
        buf[1] = 'x';
        buf[2] = digits[ua >> 4 & 0xf];
        buf[3] = digits[ua & 0xf];
        buf[4] = '\0';
        if (n)
  	      *n = 4;
        break;
      case '0' :
        buf[1] = '0';
        buf[2] = digits[ua >> 6 & 7];
        buf[3] = digits[ua >> 3 & 7];
        buf[4] = digits[ua & 7];
        buf[5] = '\0';
        if (n)
  	      *n = 5;
        break;
      default:
        buf[1] = digits[ua >> 6 & 7];
        buf[2] = digits[ua >> 3 & 7];
        buf[3] = digits[ua & 7];
        buf[4] = '\0';
        if (n)
  	      *n = 4;
    }
  }
  return buf;
}

inline const char *regex_range(char *buf, char a, char b, char esc, bool brackets = true)
{
  if (a == b)
    return regex_char(buf, a, esc);
  char *s = buf;
  if (brackets)
    *s++ = '[';
  size_t n;
  regex_char(s, a, esc, &n);
  s += n;
  if (b > a + 1)
    *s++ = '-';
  regex_char(s, b, esc, &n);
  s += n;
  if (brackets)
    *s++ = ']';
  *s++ = '\0';
  return buf;
}

/// Convert an 8-bit ASCII + Latin-1 Supplement range [a,b] to a regex pattern.
inline std::string latin1(
    char  a,               ///< lower bound of UCS range
    char  b,               ///< upper bound of UCS range
    char  esc = 'x',       ///< escape char 'x' for hex \xXX, or '0' or '\0' for octal \0nnn and \nnn
    bool brackets = true) ///< place in [ brackets ]
  /// @returns regex string to match the UCS range encoded in UTF-8
{
  if (static_cast<unsigned char>(a) > static_cast<unsigned char>(b))
    b = a;
  char buf[16];
  return regex_range(buf, a, b, esc, brackets);
}

/// Convert UCS-4 to UTF-8.
template<typename C>
size_t to_utf8(
    char32_t   c, ///< UCS-4 character U+0000 to U+10ffff
    C *s) ///< points to the buffer to populate with UTF-8 (1 to 6 bytes) not NUL-terminated
  /// @returns length (in bytes) of UTF-8 character sequence stored in s
{
  static_assert(is_char_v<std::remove_cv_t<C>> && !std::is_const_v<C>);

  assert(c<=UNICODE_MAX);
  if (c < 0x80)
  {
    *s++ = static_cast<C>(c);
    return 1;
  }
  C *t = s;
  if (c < 0x0800)
  {
    *s++ = static_cast<C>(0xC0 | ((c >> 6) & 0x1F));
  }
  else
  {
    if (c < 0x010000)
    {
      *s++ = static_cast<C>(0xE0 | ((c >> 12) & 0x0F));
    }
    else
    {
      char32_t w = c;
      *s++ = static_cast<C>(0xF0 | ((w >> 18) & 0x07));
      *s++ = static_cast<C>(0x80 | ((c >> 12) & 0x3F));
    }
    *s++ = static_cast<C>(0x80 | ((c >> 6) & 0x3F));
  }
  *s++ = static_cast<C>(0x80 | (c & 0x3F));
  return s - t;
}

/// Convert UTF-8 to UCS, returns ERR_CHAR for invalid UTF-8 except for MUTF-8 U+0000 and 0xD800-0xDFFF surrogate halves (use REFLEX_WITH_UTF8_UNRESTRICTED to remove any limits on UTF-8 encodings up to 6 bytes).
template<typename C,typename C2>
char32_t from_utf8(
    const C *s,         ///< points to the buffer with UTF-8 (1 to 6 bytes)
    C2 **r = nullptr) ///< points to pointer to set to the new position in s after the UTF-8 sequence, optional
  /// @returns UCS character
{
  static_assert(is_char_v<C>);
  static_assert(std::is_same_v<std::remove_cv_t<C>,std::remove_cv_t<C2>>);

  char32_t c[4] = {static_cast<unsigned char>(*s),0,0,0};
  if(c[0]<0x80)
    ++s;
  else if(c[0]>=0xF0){
    c[1] = static_cast<unsigned char>(s[1]);
    c[2] = static_cast<unsigned char>(s[2]);
    c[3] = static_cast<unsigned char>(s[3]);
    (c[0] &= 0b00000111) <<= 18;
    (c[1] &= 0b00111111) <<= 12;
    (c[2] &= 0b00111111) <<= 6;
    c[3] &= 0b00111111;
    c[0] = (c[0] | c[1] | c[2] | c[3]);
    s+=4;
  }else if(c[0]>=0xE0){
    c[1] = static_cast<unsigned char>(s[1]);
    c[2] = static_cast<unsigned char>(s[2]);
    (c[0] &= 0b00001111) <<= 12;
    (c[1] &= 0b00111111) <<= 6;
    c[2] &= 0b00111111;
    c[0] = (c[0] | c[1] | c[2]);
    s+=3;
  }else if(c[0]>=0xC0){
    c[1] = static_cast<unsigned char>(s[1]);
    (c[0] &= 0b00011111) <<= 6;
    c[1] &= 0b00111111;
    c[0] = (c[0] | c[1]);
    s+=2;
  }
  if (r != nullptr)
    *r = const_cast<C2*>(s);
  return c[0];
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

/// Convert a UCS-4 range [a,b] to a UTF-8 regex pattern.
inline std::string utf8(
    char32_t  a,                ///< lower bound of UCS range
    char32_t  b,                ///< upper bound of UCS range
    char  esc = 'x',        ///< escape char 'x' for hex \xXX, or '0' or '\0' for octal \0nnn and \nnn
    const char *par = "(", ///< capturing or non-capturing parenthesis "(?:"
    bool strict = true)    ///< returned regex is strict UTF-8 (true) or permissive and lean UTF-8 (false)
  /// @returns regex string to match the UCS range encoded in UTF-8
{
  if (static_cast<unsigned char>(a) > static_cast<unsigned char>(b))
    b = a;
  static constexpr const char *min_utf8_strict[6] = { // strict: pattern is strict, matching only strictly valid UTF-8
    "\x00",
    "\xc2\x80",
    "\xe0\xa0\x80",
    "\xf0\x90\x80\x80",
    "\xf8\x88\x80\x80\x80",
    "\xfc\x84\x80\x80\x80\x80"
  };
  static constexpr const char *min_utf8_lean[6] = { // lean: pattern is permissive, matching also some invalid UTF-8 but more tightly compressed UTF-8
    "\x00",
    "\xc2\x80",
    "\xe0\x80\x80",
    "\xf0\x80\x80\x80",
    "\xf8\x80\x80\x80\x80",
    "\xfc\x80\x80\x80\x80\x80"
  };
  static constexpr const char *max_utf8[6] = {
    "\x7f",
    "\xdf\xbf",
    "\xef\xbf\xbf",
    "\xf7\xbf\xbf\xbf",
    "\xfb\xbf\xbf\xbf\xbf",
    "\xfd\xbf\xbf\xbf\xbf\xbf"
  };
  const char * const *min_utf8 = (strict ? min_utf8_strict : min_utf8_lean);
  char any[16];
  char buf[16];
  char at[6];
  char bt[6];
  size_t n = to_utf8(a, at);
  size_t m = to_utf8(b, bt);
  const unsigned char *as = reinterpret_cast<const unsigned char*>(at);
  const unsigned char *bs = nullptr;
  std::string regex;
  if (strict)
  {
    regex_range(any, 0x80, 0xbf, esc);
  }
  else
  {
    any[0] = '.';
    any[1] = '\0';
  }
  while (n <= m)
  {
    if (n < m)
      bs = reinterpret_cast<const unsigned char*>(max_utf8[n - 1]);
    else
      bs = reinterpret_cast<const unsigned char*>(bt);
    size_t i;
    for (i = 0; i < n && as[i] == bs[i]; ++i)
      regex.append(regex_char(buf, as[i], esc));
    int l = 0; // pattern compression: l == 0 -> as[i+1..n-1] == UTF-8 lower bound
    for (size_t k = i + 1; k < n && l == 0; ++k)
      if (as[k] != 0x80)
        l = 1;
    int h = 0; // pattern compression: h == 0 -> bs[i+1..n-1] == UTF-8 upper bound
    for (size_t k = i + 1; k < n && h == 0; ++k)
      if (bs[k] != 0xbf)
        h = 1;
    if (i + 1 < n)
    {
      size_t j = i;
      if (i != 0)
        regex.append(par);
      if (l != 0)
      {
        size_t p = 0;
        regex.append(regex_char(buf, as[i], esc));
        ++i;
        while (i + 1 < n)
        {
          if (as[i + 1] == 0x80) // pattern compression
          {
            regex.append(regex_range(buf, as[i], 0xbf, esc));
            for (++i; i < n && as[i] == 0x80; ++i)
              regex.append(any);
          }
          else
          {
            if (as[i] != 0xbf)
            {
              ++p;
              regex.append(par).append(regex_range(buf, as[i] + 1, 0xbf, esc));
              for (size_t k = i + 1; k < n; ++k)
                regex.append(any);
              regex.append("|");
            }
            regex.append(regex_char(buf, as[i], esc));
            ++i;
          }
        }
        if (i < n)
          regex.append(regex_range(buf, as[i], 0xbf, esc));
        for (size_t k = 0; k < p; ++k)
          regex.append(")");
        i = j;
      }
      if (i + 1 < n && as[i] + l <= bs[i] - h)
      {
        if (l != 0)
          regex.append("|");
        regex.append(regex_range(buf, as[i] + l, bs[i] - h, esc));
        for (size_t k = i + 1; k < n; ++k)
          regex.append(any);
      }
      if (h != 0)
      {
        size_t p = 0;
        regex.append("|").append(regex_char(buf, bs[i], esc));
        ++i;
        while (i + 1 < n)
        {
          if (bs[i + 1] == 0xbf) // pattern compression
          {
            regex.append(regex_range(buf, 0x80, bs[i], esc));
            for (++i; i < n && bs[i] == 0xbf; ++i)
              regex.append(any);
          }
          else
          {
            if (bs[i] != 0x80)
            {
              ++p;
              regex.append(par).append(regex_range(buf, 0x80, bs[i] - 1, esc));
              for (size_t k = i + 1; k < n; ++k)
                regex.append(any);
              regex.append("|");
            }
            regex.append(regex_char(buf, bs[i], esc));
            ++i;
          }
        }
        if (i < n)
          regex.append(regex_range(buf, 0x80, bs[i], esc));
        for (size_t k = 0; k < p; ++k)
          regex.append(")");
      }
      if (j != 0)
        regex.append(")");
    }
    else if (i < n)
    {
      regex.append(regex_range(buf, as[i], bs[i], esc));
    }
    if (n < m)
    {
      as = reinterpret_cast<const unsigned char*>(min_utf8[n]);
      regex.append("|");
    }
    ++n;
  }
  return regex;
}

} // namespace reflex

#endif
