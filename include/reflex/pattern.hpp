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
@file      pattern.hpp
@brief     RE/flex regular expression pattern
@author    Robert van Engelen - engelen@genivia.com
@copyright (c) 2016-2020, Robert van Engelen, Genivia Inc. All rights reserved.
@copyright (c) BSD-3 License - see LICENSE.txt
*/

#pragma once

namespace reflex {
class Lexer;
/// Pattern class holds a regex pattern and its compiled FSM opcode table or code for the reflex::Matcher engine.
struct Pattern{
  typedef unsigned char  Pred;   ///< predict match bits
  typedef unsigned short Hash;   ///< hash value type, max value is Const::HASH
  typedef uint_least32_t Index;  ///< index into opcodes array Pattern::opc_ and subpattern indexing
  typedef uint_least32_t Accept; ///< group capture index
  typedef uint_least32_t Opcode; ///< 32 bit opcode word
  typedef unsigned short Lookahead;
  typedef void (*FSM)(Lexer&); ///< function pointer to FSM code
  /// Common constants.
  struct Const {
    static constexpr Index  IMAX = 0xFFFFFFFF; ///< max index, also serves as a marker
    static constexpr Index  LONG = 0xFFFE;     ///< LONG marker for 64 bit opcodes, must be HALT-1
    static constexpr Index  HALT = 0xFFFF;     ///< HALT marker for GOTO opcodes, must be 16 bit max
    static constexpr Hash   HASH = 0x1000;     ///< size of the predict match array
  };
  /// Construct an unset pattern.
  Pattern() noexcept :
    fsm_(nullptr),
    opc_(nullptr)
  {}
  /// Construct a pattern object given a function pointer to FSM code.
  explicit Pattern(FSM fsm, const Pred pred[] = nullptr) :
    fsm_(fsm),
    opc_(nullptr),
    pred_(pred)
  {}
  /// Construct a pattern object given an opcode table.
  explicit Pattern(const Opcode *code, const Pred pred[] = nullptr) :
    fsm_(nullptr),
    opc_(code),
    pred_(pred)
  {}

  struct Predictor{
    char      pref_[256]; ///< pattern prefix, shorter or equal to 255 bytes
    unsigned  len_; ///< prefix length of pre_[], less or equal to 255
    unsigned  min_; ///< patterns after the prefix are at least this long but no more than 8
    bool      one_; ///< true if matching one string in pre_[] without meta/anchors
    Pred      bit_[256];         ///< bitap array
    Pred      pmh_[Const::HASH]; ///< predict-match hash array
    Pred      pma_[Const::HASH]; ///< predict-match array

    Predictor() noexcept = default;
    Predictor(const Predictor& other) noexcept : len_(other.len_),min_(other.min_),one_(other.one_) {
      std::memcpy(pref_,other.pref_,256);
      std::memcpy(bit_,other.bit_,256 * sizeof(Pred));
      std::memcpy(pmh_,other.pmh_,Const::HASH * sizeof(Pred));
      std::memcpy(pma_,other.pma_,Const::HASH * sizeof(Pred));
    }
    Predictor(const Pred pred[]) noexcept {set(pred);}

    Predictor& operator=(const Predictor& other) noexcept {
      if(this!=&other){
        len_ = other.len_;
        min_ = other.min_;
        one_ = other.one_;
        std::memcpy(pref_,other.pref_,256);
        std::memcpy(bit_,other.bit_,256 * sizeof(Pred));
        std::memcpy(pmh_,other.pmh_,Const::HASH * sizeof(Pred));
        std::memcpy(pma_,other.pma_,Const::HASH * sizeof(Pred));
      }
      return *this;
    }

    void set(const Pred pred[]){
      if(pred!=nullptr){
        len_ = pred[0];
        min_ = pred[1] & 0x0f;
        one_ = pred[1] & 0x10;
        std::memcpy(pref_, pred + 2, len_);
        if (min_ > 0)
        {
          size_t n = len_ + 2;
          if (min_ > 1 && len_ == 0)
          {
            for (size_t i = 0; i < 256; ++i)
              bit_[i] = ~pred[i + n];
            n += 256;
          }
          if (min_ >= 4)
          {
            for (size_t i = 0; i < Const::HASH; ++i)
              pmh_[i] = ~pred[i + n];
          }
          else
          {
            for (size_t i = 0; i < Const::HASH; ++i)
              pma_[i] = ~pred[i + n];
          }
        }
      }
    }
  };

  /// Returns true when match is predicted, based on s[0..3..e-1] (e >= s + 4).
  static bool predict_match(const Pred pmh[], const char *s, size_t n)
  {
    Hash h = static_cast<unsigned char>(*s);
    if (pmh[h] & 1)
      return false;
    h = hash(h, static_cast<unsigned char>(*++s));
    if (pmh[h] & 2)
      return false;
    h = hash(h, static_cast<unsigned char>(*++s));
    if (pmh[h] & 4)
      return false;
    h = hash(h, static_cast<unsigned char>(*++s));
    if (pmh[h] & 8)
      return false;
    Pred m = 16;
    const char *e = s + n - 3;
    while (++s < e)
    {
      h = hash(h, static_cast<unsigned char>(*s));
      if (pmh[h] & m)
        return false;
      m <<= 1;
    }
    return true;
  }

  /// Returns zero when match is predicted or nonzero shift value, based on s[0..3].
  static size_t predict_match(const Pred pma[], const char *s)
  {
    unsigned char b[4] = {s[0],s[1],s[2],s[3]};
    Hash h[3] = {hash(b[0], b[1])};
    h[1] = hash(h[0], b[2]);
    h[2] = hash(h[1], b[3]);
    Pred a[4] = {pma[b[0]],pma[h[0]],pma[h[1]],pma[h[2]]};
    Pred p = (a[0] & 0xc0) | (a[1] & 0x30) | (a[2] & 0x0c) | (a[3] & 0x03);
    Pred m = ((((((p >> 2) | p) >> 2) | p) >> 1) | p);
    if (m != 0xff)
      return 0;
    if ((pma[b[1]] & 0xc0) != 0xc0)
      return 1;
    if ((pma[b[2]] & 0xc0) != 0xc0)
      return 2;
    if ((pma[b[3]] & 0xc0) != 0xc0)
      return 3;
    return 4;
  }

  static constexpr Hash hash(Hash h, unsigned char b)
  {
    return ((h << 3) ^ b) & (Const::HASH - 1);
  }

  // Opcode related
  static constexpr bool is_opcode_goto(Opcode opcode)
  {
    return (opcode << 8) >= (opcode & 0xFF000000);
  }
  static constexpr bool is_opcode_halt(Opcode opcode)
  {
    return opcode == 0x00FFFFFF;
  }
  static constexpr Index index_of(Opcode opcode)
  {
    return opcode & 0xFFFF;
  }
  static constexpr Index long_index_of(Opcode opcode)
  {
    return opcode & 0xFFFFFF;
  }
  static constexpr Lookahead lookahead_of(Opcode opcode)
  {
    return opcode & 0xFFFF;
  }

  /// Meta characters.
  struct Meta{
    enum : unsigned{
      MIN = 0x100,
      NWB = 0x101, ///< non-word boundary at begin `\Bx`
      NWE = 0x102, ///< non-word boundary at end   `x\B`
      BWB = 0x103, ///< begin of word at begin     `\<x` where \bx=(\<|\>)x
      EWB = 0x104, ///< end of word at begin       `\>x`
      BWE = 0x105, ///< begin of word at end       `x\<` where x\b=x(\<|\>)
      EWE = 0x106, ///< end of word at end         `x\>`
      BOL = 0x107, ///< begin of line              `^`
      EOL = 0x108, ///< end of line                `$`
      BOB = 0x109, ///< begin of buffer            `\A`
      EOB = 0x10A, ///< end of buffer              `\Z`
      UND = 0x10B, ///< undent boundary            `\k`
      IND = 0x10C, ///< indent boundary            `\i` (must be one but the largest META code)
      DED = 0x10D, ///< dedent boundary            `\j` (must be the largest META code)
      MAX          ///< max meta characters
    };
  };

  const Predictor& get_pred() const noexcept {return pred_;}

 public:
  FSM           fsm_; ///< function pointer to FSM code
  const Opcode *opc_; ///< points to the opcode table
 private:
  Predictor pred_;
}; // struct Pattern

} // namespace reflex