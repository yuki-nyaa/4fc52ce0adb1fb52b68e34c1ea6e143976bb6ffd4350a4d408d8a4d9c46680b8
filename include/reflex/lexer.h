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
@file      lexer.h (originally matcher.h)
@brief     RE/flex lexer
@author    Robert van Engelen - engelen@genivia.com
@copyright (c) 2016-2020, Robert van Engelen, Genivia Inc. All rights reserved.
@copyright (c) BSD-3 License - see LICENSE.txt
*/

#ifndef REFLEX_LEXER_H
#define REFLEX_LEXER_H

#include<reflex/abslexer.hpp>
#include<reflex/pattern.hpp>
#include<reflex/convert.h>
#include<vector>

namespace reflex {

/// RE/flex lexer engine class, implements reflex::AbstractLexer pattern matching interface with scan, find, split functors and iterators.
class Lexer : public AbstractLexer {
 public:
  /// Convert a regex to an acceptable form, given the specified regex library signature `"[decls:]escapes[?+]"`, see reflex::convert.
  template<typename T>
  static std::string convert(T regex, convert_flag_type flags = convert_flag::none)
  {
    return reflex::convert(regex, "imsx#=^:abcdefhijklnrstuvwxzABDHLNQSUW<>?", flags);
  }

  struct Option {
    bool A = false; ///< accept any/all (?^X) negative patterns as Const::REDO accept index codes
    bool W = false; ///< half-check for "whole words", check only left of \< and right of \> for non-word character
    typedef size_t char_col_map(char);
    char_col_map* T = default_char_col_map<4>;
  };

  Lexer(const Option& opt = {false,false,default_char_col_map<4>}) noexcept :
    AbstractLexer(),
    opt_(opt),
    got_(UNK),
    peek_(0),
    mrk_(false),
    ind_(0),
    ded_(0),
    col_(0),
    tab_(),
    patterns(),
    pattern_current(0)
  {}

  template<typename T>
  explicit Lexer(T&& t,const Option& opt = {false,false,default_char_col_map<4>}) noexcept :
    AbstractLexer(std::forward<T>(t)),
    opt_(opt),
    got_(UNK),
    peek_(0),
    mrk_(false),
    ind_(0),
    ded_(0),
    col_(0),
    tab_(),
    patterns(),
    pattern_current(0)
  {}

  template<typename T,typename U>
  explicit Lexer(T&& t,U&& u,const Option& opt = {false,false,default_char_col_map<4>}) noexcept :
    AbstractLexer(std::forward<T>(t),std::forward<U>(u)),
    opt_(opt),
    got_(UNK),
    peek_(0),
    mrk_(false),
    ind_(0),
    ded_(0),
    col_(0),
    tab_(),
    patterns(),
    pattern_current(0)
  {}

  Lexer(const Lexer&) = delete;
  Lexer(Lexer&& other) noexcept :
    AbstractLexer(static_cast<AbstractLexer&&>(other)),
    opt_(other.opt_),
    got_(other.got_),
    peek_(other.peek_),
    mrk_(other.mrk_),
    ind_(other.ind_),
    ded_(other.ded_),
    col_(other.col_),
    tab_(std::move(other.tab_)),
    patterns(std::move(other.patterns)),
    pattern_current(other.pattern_current)
  {other.reset();}

  virtual ~Lexer() noexcept override = default;

  Lexer& operator=(const Lexer&) = delete;
  Lexer& operator=(Lexer&& other) noexcept {
    if(this!=&other){
      AbstractLexer::operator=(static_cast<AbstractLexer&&>(other));
      opt_=other.opt_;
      got_=other.got_;
      peek_=other.peek_;
      mrk_=other.mrk_;
      ind_=other.ind_;
      ded_=other.ded_;
      col_=other.col_;
      tab_=std::move(other.tab_);
      patterns=std::move(other.patterns);
      pattern_current=other.pattern_current;
      other.reset();
    }
    return *this;
  }

  /// Reset this lexer's state to the initial state.
  void reset()
  {
    REFLEX_DBGLOG("Lexer::reset()");
    AbstractLexer::reset();
    got_ = UNK;
    peek_ = 0;
    mrk_ = false;
    ind_ = 0;
    ded_ = 0;
    col_ = 0;
    tab_.clear();
  }

  void set_ccm(Option::char_col_map f)
  {
    opt_.T = f;
  }
  Option::char_col_map* get_ccm()
  {
    return opt_.T;
  }

  int got() const noexcept {return got_;}

  ///// Inserts or appends an indent stop position, keeping indent stops sorted.
  //void insert_stop(size_t n)
  //{
  //  if (n > 0)
  //  {
  //    if (tab_.empty() || tab_.back() < n)
  //    {
  //      tab_.push_back(n);
  //    }
  //    else
  //    {
  //      for (std::vector<size_t>::reverse_iterator i = tab_.rbegin(); i != tab_.rend(); ++i)
  //      {
  //        if (*i == n)
  //          return;
  //        if (*i < n)
  //        {
  //          tab_.insert(i.base(), n);
  //          return;
  //        }
  //      }
  //      tab_.insert(tab_.begin(), n);
  //    }
  //  }
  //}
  ///// Remove all stop positions from position n and up until the last.
  //void delete_stop(size_t n)
  //{
  //  if (!tab_.empty())
  //  {
  //    for (std::vector<size_t>::reverse_iterator i = tab_.rbegin(); i != tab_.rend(); ++i)
  //    {
  //      if (*i < n)
  //      {
  //        tab_.erase(i.base(), tab_.end());
  //        return;
  //      }
  //    }
  //    tab_.clear();
  //  }
  //}
  ///// Clear indent stop positions.
  //void clear_stops()
  //{
  //  tab_.clear();
  //}
  ///// Push current indent stops and clear current indent stops.
  //void push_stops()
  //{
  //  stk_.push_back(std::move(tab_));
  //}
  ///// Pop indent stops.
  //void pop_stops()
  //{
  //  tab_ = std::move(stk_.back());
  //  stk_.pop_back();
  //}
  ///// Returns the position of the last indent stop.
  //size_t last_stop()
  //{
  //  if (tab_.empty())
  //    return 0;
  //  return tab_.back();
  //}

  int FSM_CHAR()
  {
    got_ = in.get_utf8_byte();
    if(got_!=EOF)
      str_.push_back(static_cast<char>(got_));
    return got_;
  }
  void FSM_TAKE(Accept cap)
  {
    cap_ = cap;
  }
  /// FSM code TAKE.
  void FSM_TAKE(Accept cap, int c)
  {
    cap_ = cap;
    if (c != EOF)
      in.unget(static_cast<unsigned char>(c));
  }
  void FSM_RPEEK()
  {
    peek_=0;
  }
  int FSM_PEEK()
  {
    got_ = in.peek_utf8_byte(peek_++);
    return got_;
  }

  bool FSM_DENT()
  {
    return ded_>0;
  }
  #if !defined(REFLEX_WITH_NO_INDENT)
  bool FSM_META_DED()
  {
    return dedent();
  }
  bool FSM_META_IND()
  {
    return indent();
  }
  bool FSM_META_UND()
  {
    bool mrk = mrk_ && !nodent();
    mrk_ = false;
    ded_ = 0;
    return mrk;
  }
#endif
  bool FSM_META_EOB()
  {
    if(in.peek_utf8_byte()==EOF){
      got_ = Pattern::Meta::EOB;
      return true;
    }
    return false;
  }
  bool FSM_META_BOB()
  {
    if(in.at_begin()){
      got_ = Pattern::Meta::BOB;
      return true;
    }
    return false;
  }
  bool FSM_META_EOL()
  {
    if(in.at_eol()){
      got_ = Pattern::Meta::EOL;
      return true;
    }
    return false;
  }
  bool FSM_META_BOL()
  {
    if(in.at_bol()){
      got_ = Pattern::Meta::BOL;
      return true;
    }
    return false;
  }
  bool FSM_META_EWE(int c0, int c1)
  {
    return (isword(c0) || opt_.W) && !isword(c1);
  }
  bool FSM_META_BWE(int c0, int c1)
  {
    return !isword(c0) && isword(c1);
  }
  bool FSM_META_EWB()
  {
    return isword(got_) && !isword(in.peek_utf8_byte(peek_));
  }
  bool FSM_META_BWB()
  {
    return !isword(got_) && (opt_.W || isword(in.peek_utf8_byte(peek_)));
  }
  bool FSM_META_NWE(int c0, int c1)
  {
    return isword(c0) == isword(c1);
  }
  bool FSM_META_NWB()
  {
    return isword(got_) == isword(in.peek_utf8_byte(peek_));
  }
  void FSM_REDO()
  {
    cap_ = REDO;
  }
  void FSM_REDO(int c)
  {
    cap_ = REDO;
    if (c != EOF)
      in.unget(c);
  }
  ///// FSM extra code POSN returns current position.
  //size_t FSM_POSN()
  //{
  //  return pos_ - (txt_ - buf_);
  //}
  ///// FSM extra code BACK position to a previous position returned by FSM_POSN().
  //void FSM_BACK(size_t pos)
  //{
  //  cur_ = txt_ - buf_ + pos;
  //}

 public:
  virtual Accept scan() override
  {
    begin:
    REFLEX_DBGLOG("BEGIN Lexer::scan()");
    str_.clear();
    cap_ = 0;
    if(patterns[pattern_current].fsm_!=nullptr)
      patterns[pattern_current].fsm_(*this);
    if(cap_==REDO && !opt_.A)
      goto begin;
    REFLEX_DBGLOG("Return: cap = %zu str = '%s' len = %zu got = %d", cap_, str_.c_str(), str_.size(), got_);
    REFLEX_DBGLOG("END scan()");
    return cap_;
  }
 protected:
#if !defined(REFLEX_WITH_NO_INDENT)
  /// Update indentation column counter for indent() and dedent().
  void update_col()
  {
    mrk_ = true;
    for(;ind_ < str_.size();++ind_)
      col_ += opt_.T(str_[ind_]);
    REFLEX_DBGLOG("Update col = %zu", col_);
  }
  /// @returns true if looking at indent.
  bool indent()
  {
    update_col();
    return col_ > 0 && (tab_.empty() || tab_.back() < col_);
  }
  /// @returns true if looking at dedent.
  bool dedent()
  {
    update_col();
    return !tab_.empty() && tab_.back() > col_;
  }
  /// @returns true if nodent.
  bool nodent()
  {
    update_col();
    return (col_ <= 0 || (!tab_.empty() && tab_.back() >= col_)) && (tab_.empty() || tab_.back() <= col_);
  }
#endif
  ///// @returns true if able to advance to next possible match
  //bool advance();
  ///// Boyer-Moore preprocessing of the given pattern pat of length len, generates bmd_ > 0 and bms_[] shifts.
  ///// @param pat pattern string
  ///// @param len nonzero length of the pattern string, should be less than 256
  //void boyer_moore_init(const char* pat,size_t len);

 protected:
  static constexpr Accept REDO  = 0x7FFFFFFF; ///< reflex::Matcher::accept() returns "redo" with reflex::Matcher option "A"
  static constexpr Accept EMPTY = 0xFFFFFFFF; ///< accept() returns "empty" last split at end of input

  Option opt_; ///< Options for the lexer.

  int    got_; ///< last unsigned character we looked at (to determine anchors and boundaries)

  size_t peek_;

  bool              mrk_;      ///< indent \i or dedent \j in pattern found: should check and update indent stops
  size_t            ind_;      ///< current indent position
  size_t            ded_;      ///< dedent count
  size_t            col_;      ///< column counter for indent matching, updated by newline(), indent(), and dedent()
  std::vector<size_t> tab_;    ///< tab stops set by detecting indent margins
  //std::vector<decltype(tab_)> stk_; ///< stack to push/pop stops

  //unsigned short    lcp_;      ///< primary least common character position in the pattern prefix or 0xffff for pure Boyer-Moore
  //unsigned short    lcs_;      ///< secondary least common character position in the pattern prefix or 0xffff for pure Boyer-Moore
  //size_t            bmd_;      ///< Boyer-Moore jump distance on mismatch, B-M is enabled when bmd_ > 0
  //unsigned char     bms_[256]; ///< Boyer-Moore skip array
 public:
  std::vector<reflex::Pattern> patterns;
  size_t pattern_current;
};

} // namespace reflex

#endif
