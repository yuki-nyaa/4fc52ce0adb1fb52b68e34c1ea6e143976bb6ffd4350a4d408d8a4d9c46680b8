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
@file      abslexer.hpp (originally absmatcher.h)
@brief     RE/flex abstract lexer base class.
@author    Robert van Engelen - engelen@genivia.com
@copyright (c) 2016-2020, Robert van Engelen, Genivia Inc. All rights reserved.
@copyright (c) BSD-3 License - see LICENSE.txt
*/

#ifndef REFLEX_ABSLEXER_H
#define REFLEX_ABSLEXER_H

#include<reflex/debug.h>
#include<reflex/input.h>
#include<reflex/accept.hpp>
#include<cassert>
#include<string>
#include<stdexcept>

namespace reflex {
/// The abstract lexer base class defines an interface for all pattern matcher engines.
class AbstractLexer {
 public:
  struct Lexer_Error;

  AbstractLexer():
    cap_(0),
    str_(),
    in()
  {}
  template<typename... Args>
  explicit AbstractLexer(Args&&... args):
    cap_(0),
    str_(),
    in(std::forward<Args>(args)...)
  {}
  AbstractLexer(const AbstractLexer&) = delete;
  AbstractLexer(AbstractLexer&& other):
    cap_(other.cap_),
    str_(std::move(other.str_)),
    in(std::move(other.in))
  {
    other.cap_=0;
  }
  AbstractLexer& operator=(const AbstractLexer&) = delete;
  AbstractLexer& operator=(AbstractLexer&& other){
    if(&other!=this){
      cap_ = other.cap_;
      str_ = std::move(other.str_);
      in = std::move(other.in);
      other.cap_=0;
    }
    return *this;
  }

  virtual ~AbstractLexer() noexcept = default;

  /// Reset this matcher's state to the initial state.
  void reset()
  {
    REFLEX_DBGLOG("AbstractLexer::reset()");
    cap_ = 0;
    str_.clear();
  }
  /// Returns a positive integer (true) indicating the capture index of the matched text in the pattern or zero (false) for a mismatch.
  size_t accept() const
    /// @returns nonzero capture index of the match in the pattern, which may be matcher dependent, or zero for a mismatch, or Const::EMPTY for the empty last split
  {
    return cap_;
  }
  const std::string& str() const
  {
    return str_;
  }
  std::string&& str_move()
  {
    return std::move(str_);
  }
  /// Returns the match as a u32string, converted from UTF-8 `str_`.
  std::u32string u32str() const
  {
    return u32cs(str_);
  }
  /// Returns the length of the matched text in number of u32 characters.
  size_t u32size() const
  {
    size_t n = 0;
    std::string::const_iterator e = str_.end();
    for (std::string::const_iterator b = str_.begin(); b!=e; ++b)
      n += (*b & 0xC0) != 0x80;
    return n;
  }
  /// Returns the first u32 character of the text matched.
  char32_t u32chr() const
  {
    return from_utf8(str_.data());
  }
  virtual Accept scan() = 0;
  Accept split(){
    Accept a = 0;
    while(in.peek_utf8_byte()!=EOF && (a=scan())==0)
      ;
    return a;
  }
 protected:
  Accept      cap_; ///< nonzero capture index of an accepted match or zero
  std::string str_; ///< The matched string.
 public:
  BufferedInput     in;   ///< input character sequence being matched by this matcher
};

struct AbstractLexer::Lexer_Error : std::runtime_error {
 private:
  std::string message;
  std::string m;
  size_t l;
  size_t c;
  int n;
 public:
  Lexer_Error(const std::string& matched_so_far,size_t lineno,size_t colno,int next) noexcept :
    std::runtime_error(""),
    m(matched_so_far),
    l(lineno),
    c(colno),
    n(next)
  {
    message.append("Lexer jammed at ").append(std::to_string(lineno)).append(":").append(std::to_string(colno)).append(" ! The matched text so far: ").append(matched_so_far).append(" . The next char is ");
    if(next==EOF)
      message.append("EOF");
    else
      message.push_back(static_cast<char>(next));
    message.push_back('\n');
  }
  virtual const char* what() const noexcept override {return message.c_str();}
};

} // namespace reflex

#endif
