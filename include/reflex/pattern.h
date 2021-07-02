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
@file      pattern.h
@brief     RE/flex regular expression pattern compiler
@author    Robert van Engelen - engelen@genivia.com
@copyright (c) 2016-2020, Robert van Engelen, Genivia Inc. All rights reserved.
@copyright (c) BSD-3 License - see LICENSE.txt
*/

#ifndef REFLEX_PATTERN_H
#define REFLEX_PATTERN_H

#include <reflex/debug.h>
#include <reflex/error.h>
#include <reflex/input.h>
#include <reflex/ranges.h>
#include <reflex/setop.h>
#include <cctype>
#include <cstring>
#include <iostream>
#include <string>
#include <list>
#include <map>
#include <set>
#include <vector>

#if (defined(__WIN32__) || defined(_WIN32) || defined(WIN32) || defined(_WIN64) || defined(__BORLANDC__)) && !defined(__CYGWIN__) && !defined(__MINGW32__) && !defined(__MINGW64__)
# pragma warning( disable : 4290 )
#endif

namespace reflex {
  class Lexer;
/// Pattern class holds a regex pattern and its compiled FSM opcode table or code for the reflex::Matcher engine.
class Pattern final {
  friend class Lexer;
  friend class FuzzyMatcher;
 public:
  typedef uint_least8_t  Pred;   ///< predict match bits
  typedef uint_least16_t Hash;   ///< hash value type, max value is Const::HASH
  typedef uint_least32_t Index;  ///< index into opcodes array Pattern::opc_ and subpattern indexing
  typedef uint_least32_t Accept; ///< group capture index
  typedef uint_least32_t Opcode; ///< 32 bit opcode word
  typedef void (*FSM)(Lexer&); ///< function pointer to FSM code
  /// Common constants.
  struct Const {
    static const Index  IMAX = 0xFFFFFFFF; ///< max index, also serves as a marker
    static const Index  GMAX = 0xFEFFFF;   ///< max goto index
    static const Accept AMAX = 0xFDFFFF;   ///< max accept
    static const Index  LMAX = 0xFAFFFF;   ///< max lookahead index
    static const Index  LONG = 0xFFFE;     ///< LONG marker for 64 bit opcodes, must be HALT-1
    static const Index  HALT = 0xFFFF;     ///< HALT marker for GOTO opcodes, must be 16 bit max
    static const Hash   HASH = 0x1000;     ///< size of the predict match array
  };
  /// Construct an unset pattern.
  Pattern() :
    opc_(nullptr),
    nop_(0),
    fsm_(nullptr)
  { }
  /// Construct a pattern object given a regex string.
  template<typename R,typename = typename std::enable_if<std::is_constructible<std::string,R&&>::value>::type>
  Pattern(R&& regex, const char *options = nullptr) :
    rex_(std::forward<R>(regex)),
    opc_(nullptr),
    fsm_(nullptr)
  {
    init(options);
  }
  /// Construct a pattern object given a regex string.
  template<typename R>
  Pattern(R&& regex, const std::string& options) :
    rex_(std::forward<R>(regex)),
    opc_(nullptr),
    fsm_(nullptr)
  {
    init(options.c_str());
  }
  /// Construct a pattern object given an opcode table.
  explicit Pattern(const Opcode *code, const uint_least8_t *pred = nullptr) :
    opc_(code),
    nop_(0),
    fsm_(nullptr)
  {
    init(nullptr, pred);
  }
  /// Construct a pattern object given a function pointer to FSM code.
  explicit Pattern(FSM fsm, const uint_least8_t *pred = nullptr) :
    opc_(nullptr),
    nop_(0),
    fsm_(fsm)
  {
    init(nullptr, pred);
  }
  /// Copy constructor.
  Pattern(const Pattern& pattern) :
    opt_(pattern.opt_),
    rex_(pattern.rex_),
    end_(pattern.end_),
    acc_(pattern.acc_),
    vno_(pattern.vno_),
    eno_(pattern.eno_),
    opc_(nullptr),
    nop_(0),
    fsm_(nullptr)
  {
    if (pattern.nop_ > 0 && pattern.opc_ != nullptr)
    {
      nop_ = pattern.nop_;
      Opcode *code = new Opcode[nop_];
      for (size_t i = 0; i < nop_; ++i)
        code[i] = pattern.opc_[i];
      opc_ = code;
    }
    else
    {
      fsm_ = pattern.fsm_;
    }
  }
   /// Move constructor.
  Pattern(Pattern&& pattern) :
    opt_(std::move(pattern.opt_)),
    rex_(std::move(pattern.rex_)),
    end_(std::move(pattern.end_)),
    acc_(std::move(pattern.acc_)),
    vno_(std::move(pattern.vno_)),
    eno_(std::move(pattern.eno_)),
    opc_(nullptr),
    nop_(0),
    fsm_(nullptr)
  {
    if (pattern.nop_ > 0 && pattern.opc_ != nullptr)
    {
      nop_ = pattern.nop_;
      opc_ = pattern.opc_;
      pattern.nop_ = 0;
      pattern.opc_ = nullptr;
    }
    else
    {
      fsm_ = pattern.fsm_;
      pattern.fsm_=nullptr;
    }
  }
  /// Destructor, deletes internal code array when owned and allocated.
  ~Pattern() noexcept
  {
    clear();
  }
  /// Clear and delete pattern data.
  void clear()
  {
    rex_.clear();
    if (nop_ > 0 && opc_ != nullptr)
      delete[] opc_;
    opc_ = nullptr;
    nop_ = 0;
    fsm_ = nullptr;
  }
  /// Assign a (new) pattern.
  Pattern& operator=(const Pattern& pattern)
  {
    clear();
    opt_ = pattern.opt_;
    rex_ = pattern.rex_;
    end_ = pattern.end_;
    acc_ = pattern.acc_;
    vno_ = pattern.vno_;
    eno_ = pattern.eno_;
    if (pattern.nop_ > 0 && pattern.opc_ != nullptr)
    {
      nop_ = pattern.nop_;
      Opcode *code = new Opcode[nop_];
      for (size_t i = 0; i < nop_; ++i)
        code[i] = pattern.opc_[i];
      opc_ = code;
    }
    else
    {
      fsm_ = pattern.fsm_;
    }
    return *this;
  }
/// Assign a (new) pattern.
  Pattern& operator=(Pattern&& pattern)
  {
    if (nop_ > 0 && opc_ != nullptr)
      delete[] opc_;
    opc_ = nullptr;
    nop_ = 0;
    fsm_ = nullptr;

    opt_ = std::move(pattern.opt_);
    rex_ = std::move(pattern.rex_);
    end_ = std::move(pattern.end_);
    acc_ = std::move(pattern.acc_);
    vno_ = std::move(pattern.vno_);
    eno_ = std::move(pattern.eno_);
    if (pattern.nop_ > 0 && pattern.opc_ != nullptr)
    {
      nop_ = pattern.nop_;
      opc_ = pattern.opc_;
      pattern.nop_ = 0;
      pattern.opc_ = nullptr;
    }
    else
    {
      fsm_ = pattern.fsm_;
      pattern.fsm_=nullptr;
    }
    return *this;
  }
  /// Assign a (new) pattern.
  Pattern& assign(
      const char *regex,
      const char *options = nullptr)
  {
    clear();
    rex_ = regex;
    init(options);
    return *this;
  }
  /// Assign a (new) pattern.
  Pattern& assign(
      const char        *regex,
      const std::string& options)
  {
    return assign(regex, options.c_str());
  }
  /// Assign a (new) pattern.
  Pattern& assign(
      const std::string& regex,
      const char        *options = nullptr)
  {
    return assign(regex.c_str(), options);
  }
  /// Assign a (new) pattern.
  Pattern& assign(
      const std::string& regex,
      const std::string& options)
  {
    return assign(regex.c_str(), options.c_str());
  }
  /// Assign a (new) pattern.
  Pattern& assign(
      const Opcode  *code,
      const uint_least8_t *pred = nullptr)
  {
    clear();
    opc_ = code;
    init(nullptr, pred);
    return *this;
  }
  /// Assign a (new) pattern.
  Pattern& assign(
      FSM            fsm,
      const uint_least8_t *pred = nullptr)
  {
    clear();
    fsm_ = fsm;
    init(nullptr, pred);
    return *this;
  }
  /// Assign a (new) pattern.
  Pattern& operator=(const char *regex)
  {
    return assign(regex);
  }
  /// Assign a (new) pattern.
  Pattern& operator=(const std::string& regex)
  {
    return assign(regex);
  }
  /// Assign a (new) pattern.
  Pattern& operator=(const Opcode *code)
  {
    return assign(code);
  }
  /// Assign a (new) pattern.
  Pattern& operator=(FSM fsm)
  {
    return assign(fsm);
  }
  /// Get the number of subpatterns of this pattern object.
  Accept size() const
    /// @returns number of subpatterns
  {
    return static_cast<Accept>(end_.size());
  }
  /// Return true if this pattern is not assigned.
  bool empty() const
    /// @return true if this pattern is not assigned
  {
    return opc_ == nullptr && fsm_ == nullptr;
  }
  /// Get subpattern regex of this pattern object or the whole regex with index 0.
  const std::string operator[](Accept choice) const
    /// @returns subpattern string or "" when not set
    ;
  /// Check if subpattern is reachable by a match.
  bool reachable(Accept choice) const
    /// @returns true if subpattern is reachable
  {
    return choice >= 1 && choice <= size() && acc_.at(choice - 1);
  }
  /// Get the number of finite state machine nodes (vertices).
  size_t nodes() const
    /// @returns number of nodes or 0 when no finite state machine was constructed by this pattern
  {
    return nop_ > 0 ? vno_ : 0;
  }
  /// Get the number of finite state machine edges (transitions on input characters).
  size_t edges() const
    /// @returns number of edges or 0 when no finite state machine was constructed by this pattern
  {
    return nop_ > 0 ? eno_ : 0;
  }
  /// Get the code size in number of words.
  size_t words() const
    /// @returns number of words or 0 when no code was generated by this pattern
  {
    return nop_;
  }
  /// Get elapsed regex parsing and analysis time.
  float parse_time() const
  {
    #ifdef REFLEX_PATTERN_TIMER
    return pms_;
    #else
    return 0;
    #endif
  }
  /// Get elapsed DFA vertices construction time.
  float nodes_time() const
  {
    #ifdef REFLEX_PATTERN_TIMER
    return vms_;
    #else
    return 0;
    #endif
  }
  /// Get elapsed DFA edges construction time.
  float edges_time() const
  {
    #ifdef REFLEX_PATTERN_TIMER
    return ems_;
    #else
    return 0;
    #endif
  }
  /// Get elapsed code words assembly time.
  float words_time() const
  {
    #ifdef REFLEX_PATTERN_TIMER
    return wms_;
    #else
    return 0;
    #endif
  }
  /// Returns true when match is predicted, based on s[0..3..e-1] (e >= s + 4).
  static inline bool predict_match(const Pred pmh[], const char *s, size_t n)
  {
    Hash h = static_cast<uint_least8_t>(*s);
    if (pmh[h] & 1)
      return false;
    h = hash(h, static_cast<uint_least8_t>(*++s));
    if (pmh[h] & 2)
      return false;
    h = hash(h, static_cast<uint_least8_t>(*++s));
    if (pmh[h] & 4)
      return false;
    h = hash(h, static_cast<uint_least8_t>(*++s));
    if (pmh[h] & 8)
      return false;
    Pred m = 16;
    const char *e = s + n - 3;
    while (++s < e)
    {
      h = hash(h, static_cast<uint_least8_t>(*s));
      if (pmh[h] & m)
        return false;
      m <<= 1;
    }
    return true;
  }
  /// Returns zero when match is predicted or nonzero shift value, based on s[0..3].
  static inline size_t predict_match(const Pred pma[], const char *s)
  {
    uint_least8_t b0 = s[0];
    uint_least8_t b1 = s[1];
    uint_least8_t b2 = s[2];
    uint_least8_t b3 = s[3];
    Hash h1 = hash(b0, b1);
    Hash h2 = hash(h1, b2);
    Hash h3 = hash(h2, b3);
    Pred a0 = pma[b0];
    Pred a1 = pma[h1];
    Pred a2 = pma[h2];
    Pred a3 = pma[h3];
    Pred p = (a0 & 0xc0) | (a1 & 0x30) | (a2 & 0x0c) | (a3 & 0x03);
    Pred m = ((((((p >> 2) | p) >> 2) | p) >> 1) | p);
    if (m != 0xff)
      return 0;
    if ((pma[b1] & 0xc0) != 0xc0)
      return 1;
    if ((pma[b2] & 0xc0) != 0xc0)
      return 2;
    if ((pma[b3] & 0xc0) != 0xc0)
      return 3;
    return 4;
  }
 protected:
  /// Throw an error.
  virtual void error(
      regex_error_type code,    ///< error code
      size_t           pos = 0) ///< optional location of the error in regex string Pattern::rex_
    const;
 private:
  typedef uint_least16_t                Char; // 8 bit char and meta chars up to META_MAX-1
  typedef uint_least8_t                 Lazy;
  typedef uint_least16_t                Iter;
  typedef uint_least16_t                Lookahead;
  typedef std::set<Lookahead>     Lookaheads;
  typedef uint_least32_t                Location;
  typedef ORanges<Location>       Locations;
  typedef std::map<int,Locations> Map;
  /// Set of chars and meta chars
  struct Chars {
    Chars()                                 { clear(); }
    Chars(const Chars& c)                   { b[0] = c.b[0]; b[1] = c.b[1]; b[2] = c.b[2]; b[3] = c.b[3]; b[4] = c.b[4]; }
    Chars(const uint_least64_t c[5])              { b[0] = c[0]; b[1] = c[1]; b[2] = c[2]; b[3] = c[3]; b[4] = c[4]; }
    void   clear()                          { b[0] = b[1] = b[2] = b[3] = b[4] = 0ULL; }
    bool   any()                      const { return b[0] || b[1] || b[2] || b[3] || b[4]; }
    bool   intersects(const Chars& c) const { return (b[0] & c.b[0]) || (b[1] & c.b[1]) || (b[2] & c.b[2]) || (b[3] & c.b[3]) || (b[4] & c.b[4]); }
    bool   contains(const Chars& c)   const { return !(c - *this).any(); }
    bool   contains(Char c)           const { return b[c >> 6] & (1ULL << (c & 0x3F)); }
    Chars& insert(Char c)                   { b[c >> 6] |= 1ULL << (c & 0x3F); return *this; }
    Chars& insert(Char lo, Char hi)         { while (lo <= hi) insert(lo++); return *this; }
    Chars& flip()                           { b[0] = ~b[0]; b[1] = ~b[1]; b[2] = ~b[2]; b[3] = ~b[3]; b[4] = ~b[4]; return *this; }
    Chars& flip256()                        { b[0] = ~b[0]; b[1] = ~b[1]; b[2] = ~b[2]; b[3] = ~b[3]; return *this; }
    Chars& swap(Chars& c)                   { Chars t = c; c = *this; return *this = t; }
    Chars& operator+=(const Chars& c)       { return operator|=(c); }
    Chars& operator-=(const Chars& c)       { b[0] &=~c.b[0]; b[1] &=~c.b[1]; b[2] &=~c.b[2]; b[3] &=~c.b[3]; b[4] &=~c.b[4]; return *this; }
    Chars& operator|=(const Chars& c)       { b[0] |= c.b[0]; b[1] |= c.b[1]; b[2] |= c.b[2]; b[3] |= c.b[3]; b[4] |= c.b[4]; return *this; }
    Chars& operator&=(const Chars& c)       { b[0] &= c.b[0]; b[1] &= c.b[1]; b[2] &= c.b[2]; b[3] &= c.b[3]; b[4] &= c.b[4]; return *this; }
    Chars& operator^=(const Chars& c)       { b[0] ^= c.b[0]; b[1] ^= c.b[1]; b[2] ^= c.b[2]; b[3] ^= c.b[3]; b[4] ^= c.b[4]; return *this; }
    Chars  operator+(const Chars& c)  const { return Chars(*this) += c; }
    Chars  operator-(const Chars& c)  const { return Chars(*this) -= c; }
    Chars  operator|(const Chars& c)  const { return Chars(*this) |= c; }
    Chars  operator&(const Chars& c)  const { return Chars(*this) &= c; }
    Chars  operator^(const Chars& c)  const { return Chars(*this) ^= c; }
    Chars  operator~()                const { return Chars(*this).flip(); }
           operator bool()            const { return any(); }
    Chars& operator=(const Chars& c)        { b[0] = c.b[0]; b[1] = c.b[1]; b[2] = c.b[2]; b[3] = c.b[3]; b[4] = c.b[4]; return *this; }
    bool   operator==(const Chars& c) const { return b[0] == c.b[0] && b[1] == c.b[1] && b[2] == c.b[2] && b[3] == c.b[3] && b[4] == c.b[4]; }
    bool   operator<(const Chars& c)  const { return b[0] < c.b[0] || (b[0] == c.b[0] && (b[1] < c.b[1] || (b[1] == c.b[1] && (b[2] < c.b[2] || (b[2] == c.b[2] && (b[3] < c.b[3] || (b[3] == c.b[3] && b[4] < c.b[4]))))))); }
    bool   operator>(const Chars& c)  const { return c < *this; }
    bool   operator<=(const Chars& c) const { return !(c < *this); }
    bool   operator>=(const Chars& c) const { return !(*this < c); }
    Char   lo()                       const { for (Char i = 0; i < 5; ++i) if (b[i]) for (Char j = 0; j < 64; ++j) if (b[i] & (1ULL << j)) return (i << 6) + j; return 0; }
    Char   hi()                       const { for (Char i = 0; i < 5; ++i) if (b[4-i]) for (Char j = 0; j < 64; ++j) if (b[4-i] & (1ULL << (63-j))) return ((4-i) << 6) + (63-j); return 0; }
    uint_least64_t b[4+1]; ///< 256 bits to store a set of 8-bit chars + extra bits for meta
  };
  /// Finite state machine construction position information.
  struct Position {
    typedef uint_least64_t        value_type;
    static constexpr Iter       MAXITER = 0xFFFF;
    static constexpr Location   MAXLOC  = 0xFFFFFFFFUL;
    static constexpr value_type NPOS    = 0xFFFFFFFFFFFFFFFFULL;
    static constexpr value_type RES1    = 1ULL << 48; ///< reserved
    static constexpr value_type RES2    = 1ULL << 49; ///< reserved
    static constexpr value_type RES3    = 1ULL << 50; ///< reserved
    static constexpr value_type NEGATE  = 1ULL << 51; ///< marks negative patterns
    static constexpr value_type TICKED  = 1ULL << 52; ///< marks lookahead ending ) in (?=X)
    static constexpr value_type GREEDY  = 1ULL << 53; ///< force greedy quants
    static constexpr value_type ANCHOR  = 1ULL << 54; ///< marks begin of word (\b,\<,\>) and buffer (\A,^) anchors
    static constexpr value_type ACCEPT  = 1ULL << 55; ///< accept, not a regex position

    constexpr Position(value_type k = NPOS) noexcept : k(k) {}

    operator value_type()            const { return k; }
    Position iter(Iter i)            const { return Position(k + (static_cast<value_type>(i) << 32)); }
    Position negate(bool b)          const { return b ? Position(k | NEGATE) : Position(k & ~NEGATE); }
    Position ticked(bool b)          const { return b ? Position(k | TICKED) : Position(k & ~TICKED); }
    Position greedy(bool b)          const { return b ? Position(k | GREEDY) : Position(k & ~GREEDY); }
    Position anchor(bool b)          const { return b ? Position(k | ANCHOR) : Position(k & ~ANCHOR); }
    Position accept(bool b)          const { return b ? Position(k | ACCEPT) : Position(k & ~ACCEPT); }
    Position lazy(Lazy l)            const { return Position((k & 0x00FFFFFFFFFFFFFFULL) | static_cast<value_type>(l) << 56); }
    Position pos()                   const { return Position(k & 0x0000FFFFFFFFFFFFULL); }
    Location loc()                   const { return static_cast<Location>(k); }
    Accept   accepts()               const { return static_cast<Accept>(k); }
    Iter     iter()                  const { return static_cast<Index>((k >> 32) & 0xFFFF); }
    bool     negate()                const { return (k & NEGATE) != 0; }
    bool     ticked()                const { return (k & TICKED) != 0; }
    bool     greedy()                const { return (k & GREEDY) != 0; }
    bool     anchor()                const { return (k & ANCHOR) != 0; }
    bool     accept()                const { return (k & ACCEPT) != 0; }
    Lazy     lazy()                  const { return static_cast<Lazy>(k >> 56); }
    value_type k;
  };
  typedef std::set<Lazy>               Lazyset;
  typedef std::set<Position>           Positions;
  typedef std::map<Position,Positions> Follow;
  typedef std::pair<Chars,Positions>   Move;
  typedef std::list<Move>              Moves;
  /// Tree DFA constructed from string patterns.
  struct Tree
  {
    struct Node {
      Node()
        :
          accept(0)
      {
        for (int i = 0; i < 256; ++i)
          edge[i] = nullptr;
      }
      Node  *edge[256]; ///< 256 edges, one per 8-bit char
      Accept accept;    ///< nonzero if final state, the index of an accepted/captured subpattern
    };
    typedef std::list<Node*> List;
    static const uint_least16_t ALLOC = 64; ///< allocate 64 nodes at a time, to improve performance
    Tree()
      :
        tree(nullptr),
        next(ALLOC)
    { }
    ~Tree()
    {
      clear();
    }
    /// delete the tree DFA.
    void clear()
    {
      for (List::iterator i = list.begin(); i != list.end(); ++i)
        delete[] *i;
      list.clear();
    }
    /// return the root of the tree.
    Node *root()
    {
      return tree != nullptr ? tree : (tree = leaf());
    }
    /// create an edge from a tree node to a target tree node, return the target tree node.
    Node *edge(Node *node, Char c)
    {
      return node->edge[c] != nullptr ? node->edge[c] : (node->edge[c] = leaf());
    }
    /// create a new leaf node.
    Node *leaf()
    {
      if (next >= ALLOC)
      {
        list.push_back(new Node[ALLOC]);
        next = 0;
      }
      return &list.back()[next++];
    }
    Node    *tree; ///< root of the tree or nullptr
    List     list; ///< block allocation list
    uint_least16_t next; ///< block allocation, next available slot in last block
  };
  /// DFA created by subset construction from regex patterns.
  struct DFA {
    struct State : Positions {
      typedef std::map<Char,std::pair<Char,State*> > Edges;
      State()
        :
          next(nullptr),
          left(nullptr),
          right(nullptr),
          tnode(nullptr),
          first(0),
          index(0),
          accept(0),
          redo(false)
      { }
      State *assign(Tree::Node *node)
      {
        tnode = node;
        return this;
      }
      State *assign(Tree::Node *node, Positions& pos)
      {
        tnode = node;
        this->swap(pos);
        return this;
      }
      State      *next;   ///< points to next state in the list of states allocated depth-first by subset construction
      State      *left;   ///< left pointer for O(log N) node insertion in the hash table overflow tree
      State      *right;  ///< right pointer for O(log N) node insertion in the hash table overflow tree
      Tree::Node *tnode;  ///< the corresponding tree DFA node, when applicable
      Edges       edges;  ///< state transitions
      Index       first;  ///< index of this state in the opcode table, determined by the first assembly pass
      Index       index;  ///< index of this state in the opcode table
      Accept      accept; ///< nonzero if final state, the index of an accepted/captured subpattern
      Lookaheads  heads;  ///< lookahead head set
      Lookaheads  tails;  ///< lookahead tail set
      bool        redo;   ///< true if this is a final state of a negative pattern
    };
    typedef std::list<State*> List;
    static const uint_least16_t ALLOC = 256; ///< allocate 256 states at a time, to improve performance.
    DFA()
      :
        next(ALLOC)
    { }
    ~DFA()
    {
      clear();
    }
    /// delete DFA
    void clear()
    {
      for (List::iterator i = list.begin(); i != list.end(); ++i)
        delete[] *i;
      list.clear();
    }
    /// new DFA state with optional tree DFA node.
    State *state(Tree::Node *node)
    {
      if (next >= ALLOC)
      {
        list.push_back(new State[ALLOC]);
        next = 0;
      }
      return list.back()[next++].assign(node);
    }
    /// new DFA state with optional tree DFA node and positions, destroys pos.
    State *state(Tree::Node *node, Positions& pos)
    {
      if (next >= ALLOC)
      {
        list.push_back(new State[ALLOC]);
        next = 0;
      }
      return list.back()[next++].assign(node, pos);
    }
    List     list; ///< block allocation list
    uint_least16_t next; ///< block allocation, next available slot in last block
  };
  /// Global modifier modes, syntax flags, and compiler options.
  struct Option {
    Option() : b(), e(), f(), i(), m(), n(), o(), p(), q(), r(), s(), w(), x(), z() { }
    Option(const Option&) noexcept = default;
    Option(Option&&) noexcept = default;
    Option& operator=(const Option&) noexcept = default;
    Option& operator=(Option&&) noexcept = default;
    bool                     b; ///< disable escapes in bracket lists
    Char                     e; ///< escape character, or > 255 for none, '\\' default
    std::vector<std::string> f; ///< output to files
    bool                     i; ///< case insensitive mode, also `(?i:X)`
    bool                     m; ///< multi-line mode, also `(?m:X)`
    std::string              n; ///< pattern name (for use in generated code)
    bool                     o; ///< generate optimized FSM code for option f
    bool                     p; ///< with option f also output predict match array for fast search with find()
    bool                     q; ///< enable "X" quotation of verbatim content, also `(?q:X)`
    bool                     r; ///< raise syntax errors
    bool                     s; ///< single-line mode (dotall mode), also `(?s:X)`
    bool                     w; ///< write error message to stderr
    bool                     x; ///< free-spacing mode, also `(?x:X)`
    std::string              z; ///< namespace (NAME1.NAME2.NAME3)
  };
  /// Meta characters.
  enum Meta {
    META_MIN = 0x100,
    META_NWB = 0x101, ///< non-word boundary at begin `\Bx`
    META_NWE = 0x102, ///< non-word boundary at end   `x\B`
    META_BWB = 0x103, ///< begin of word at begin     `\<x` where \bx=(\<|\>)x
    META_EWB = 0x104, ///< end of word at begin       `\>x`
    META_BWE = 0x105, ///< begin of word at end       `x\<` where x\b=x(\<|\>)
    META_EWE = 0x106, ///< end of word at end         `x\>`
    META_BOL = 0x107, ///< begin of line              `^`
    META_EOL = 0x108, ///< end of line                `$`
    META_BOB = 0x109, ///< begin of buffer            `\A`
    META_EOB = 0x10A, ///< end of buffer              `\Z`
    META_UND = 0x10B, ///< undent boundary            `\k`
    META_IND = 0x10C, ///< indent boundary            `\i` (must be one but the largest META code)
    META_DED = 0x10D, ///< dedent boundary            `\j` (must be the largest META code)
    META_MAX          ///< max meta characters
  };
  /// Initialize the pattern at construction.
  void init(
      const char    *options,
      const uint_least8_t *pred = nullptr);
  void init_options(const char *options);
  void parse(
      Positions& startpos,
      Follow&    followpos,
      Map&       modifiers,
      Map&       lookahead);
  void parse1(
      bool       begin,
      Location&  loc,
      Positions& firstpos,
      Positions& lastpos,
      bool&      nullable,
      Follow&    followpos,
      Lazy&      lazyidx,
      Lazyset&   lazyset,
      Map&       modifiers,
      Locations& lookahead,
      Iter&      iter);
  void parse2(
      bool       begin,
      Location&  loc,
      Positions& firstpos,
      Positions& lastpos,
      bool&      nullable,
      Follow&    followpos,
      Lazy&      lazyidx,
      Lazyset&   lazyset,
      Map&       modifiers,
      Locations& lookahead,
      Iter&      iter);
  void parse3(
      bool       begin,
      Location&  loc,
      Positions& firstpos,
      Positions& lastpos,
      bool&      nullable,
      Follow&    followpos,
      Lazy&      lazyidx,
      Lazyset&   lazyset,
      Map&       modifiers,
      Locations& lookahead,
      Iter&      iter);
  void parse4(
      bool       begin,
      Location&  loc,
      Positions& firstpos,
      Positions& lastpos,
      bool&      nullable,
      Follow&    followpos,
      Lazy&      lazyidx,
      Lazyset&   lazyset,
      Map&       modifiers,
      Locations& lookahead,
      Iter&      iter);
  Char parse_esc(
      Location& loc,
      Chars    *chars = nullptr) const;
  void compile(
      DFA::State *start,
      Follow&     followpos,
      const Map&  modifiers,
      const Map&  lookahead);
  void lazy(
      const Lazyset& lazyset,
      Positions&     pos) const;
  void lazy(
      const Lazyset&   lazyset,
      const Positions& pos,
      Positions&       pos1) const;
  void greedy(Positions& pos) const;
  void trim_anchors(Positions& follow, const Position p) const;
  void trim_lazy(Positions *pos) const;
  void compile_transition(
      DFA::State *state,
      Follow&     followpos,
      const Map&  modifiers,
      const Map&  lookahead,
      Moves&      moves) const;
  void transition(
      Moves&           moves,
      Chars&           chars,
      const Positions& follow) const;
  void compile_list(
      Location   loc,
      Chars&     chars,
      const Map& modifiers) const;
  void posix(
      size_t index,
      Chars& chars) const;
  void flip(Chars& chars) const;
  void assemble(DFA::State *start);
  void compact_dfa(DFA::State *start);
  void encode_dfa(DFA::State *start);
  void gencode_dfa(const DFA::State *start) const;
  void check_dfa_closure(
      const DFA::State *state,
      int               nest,
      bool&             peek,
      bool&             prev) const;
  void gencode_dfa_closure(
      FILE             *fd,
      const DFA::State *start,
      int               nest,
      bool              peek) const;
  void export_dfa(const DFA::State *start) const;
  void export_code() const;
  void predict_match_dfa(DFA::State *start);
  void gen_predict_match(DFA::State *state);
  void gen_predict_match_transitions(DFA::State *state, std::map<DFA::State*,ORanges<Hash> >& states);
  void gen_predict_match_transitions(size_t level, DFA::State *state, ORanges<Hash>& labels, std::map<DFA::State*,ORanges<Hash> >& states);
  void write_predictor(FILE *fd) const;
  void write_namespace_open(FILE* fd) const;
  void write_namespace_close(FILE* fd) const;
  size_t find_at(
      Location loc,
      char     c) const
  {
    return rex_.find_first_of(c, loc);
  }
  Char at(Location k) const
  {
    return static_cast<unsigned char>(rex_[k]);
  }
  bool eq_at(
      Location    loc,
      const char *s) const
  {
    return rex_.compare(loc, strlen(s), s) == 0;
  }
  Char escape_at(Location loc) const
  {
    if (at(loc) == opt_.e)
      return at(loc + 1);
    return '\0';
  }
  Char escapes_at(
      Location    loc,
      const char *escapes) const
  {
    if (at(loc) == opt_.e && std::strchr(escapes, at(loc + 1)))
      return at(loc + 1);
    return '\0';
  }
  static inline bool is_modified(
      Char       mode,
      const Map& modifiers,
      Location   loc)
  {
    Map::const_iterator i = modifiers.find(mode);
    return i != modifiers.end() && i->second.find(loc) != i->second.end();
  }
  static inline void update_modified(
      Char     mode,
      Map&     modifiers,
      Location from,
      Location to)
  {
    // mode modifiers i, m, s (enabled) I, M, S (disabled)
    if (modifiers.find(reversecase(mode)) != modifiers.end())
    {
      Locations modified(from, to);
      modified -= modifiers[reversecase(mode)];
      modifiers[mode] += modified;
    }
    else
    {
      modifiers[mode].insert(from, to);
    }
  }
  static inline uint_least16_t hash_pos(const Positions *pos)
  {
    uint_least16_t h = 0;
    for (Positions::const_iterator i = pos->begin(); i != pos->end(); ++i)
      h += static_cast<uint_least16_t>(*i ^ (*i >> 24)); // (Position{*i}.iter() << 4) unique hash for up to 16 chars iterated (abc...p){iter}
    return h;
  }
  static inline bool valid_goto_index(Index index)
  {
    return index <= Const::GMAX;
  }
  static inline bool valid_take_index(Index index)
  {
    return index <= Const::AMAX;
  }
  static inline bool valid_lookahead_index(Index index)
  {
    return index <= Const::LMAX;
  }
  static inline bool is_meta(Char c)
  {
    return c > META_MIN;
  }
  static inline Opcode opcode_long(Index index)
  {
    return 0xFF000000 | (index & 0xFFFFFF); // index <= Const::GMAX (0xFEFFFF max)
  }
  static inline Opcode opcode_take(Index index)
  {
    return 0xFE000000 | (index & 0xFFFFFF); // index <= Const::AMAX (0xFDFFFF max)
  }
  static inline Opcode opcode_redo()
  {
    return 0xFD000000;
  }
  static inline Opcode opcode_tail(Index index)
  {
    return 0xFC000000 | (index & 0xFFFFFF); // index <= Const::LMAX (0xFAFFFF max)
  }
  static inline Opcode opcode_head(Index index)
  {
    return 0xFB000000 | (index & 0xFFFFFF); // index <= Const::LMAX (0xFAFFFF max)
  }
  static inline Opcode opcode_goto(
      Char  lo,
      Char  hi,
      Index index)
  {
    return is_meta(lo) ? (static_cast<Opcode>(lo) << 24) | index : (static_cast<Opcode>(lo) << 24) | (hi << 16) | index;
  }
  static inline Opcode opcode_halt()
  {
    return 0x00FFFFFF;
  }
  static inline bool is_opcode_long(Opcode opcode)
  {
    return (opcode & 0xFF000000) == 0xFF000000;
  }
  static inline bool is_opcode_take(Opcode opcode)
  {
    return (opcode & 0xFE000000) == 0xFE000000;
  }
  static inline bool is_opcode_redo(Opcode opcode)
  {
    return opcode == 0xFD000000;
  }
  static inline bool is_opcode_tail(Opcode opcode)
  {
    return (opcode & 0xFF000000) == 0xFC000000;
  }
  static inline bool is_opcode_head(Opcode opcode)
  {
    return (opcode & 0xFF000000) == 0xFB000000;
  }
  static inline bool is_opcode_halt(Opcode opcode)
  {
    return opcode == 0x00FFFFFF;
  }
  static inline bool is_opcode_goto(Opcode opcode)
  {
    return (opcode << 8) >= (opcode & 0xFF000000);
  }
  static inline bool is_opcode_meta(Opcode opcode)
  {
    return (opcode & 0x00FF0000) == 0x00000000 && (opcode >> 24) > 0;
  }
  static inline bool is_opcode_goto(
      Opcode        opcode,
      unsigned char c)
  {
    return c >= (opcode >> 24) && c <= ((opcode >> 16) & 0xFF);
  }
  static inline Char meta_of(Opcode opcode)
  {
    return META_MIN + (opcode >> 24);
  }
  static inline Char lo_of(Opcode opcode)
  {
    return is_opcode_meta(opcode) ? meta_of(opcode) : opcode >> 24;
  }
  static inline Char hi_of(Opcode opcode)
  {
    return is_opcode_meta(opcode) ? meta_of(opcode) : (opcode >> 16) & 0xFF;
  }
  static inline Index index_of(Opcode opcode)
  {
    return opcode & 0xFFFF;
  }
  static inline Index long_index_of(Opcode opcode)
  {
    return opcode & 0xFFFFFF;
  }
  static inline Lookahead lookahead_of(Opcode opcode)
  {
    return opcode & 0xFFFF;
  }
  static inline Char lowercase(Char c)
  {
    return static_cast<unsigned char>(c | 0x20);
  }
  static inline Char uppercase(Char c)
  {
    return static_cast<unsigned char>(c & ~0x20);
  }
  static inline Char reversecase(Char c)
  {
    return static_cast<unsigned char>(c ^ 0x20);
  }
  static inline Hash hash(Hash h, uint_least8_t b)
  {
    return ((h << 3) ^ b) & (Const::HASH - 1);
  }
  static inline Hash hash(Hash h)
  {
    return h & ((Const::HASH - 1) >> 3);
  }
  Option                opt_; ///< pattern compiler options
  std::string           rex_; ///< regular expression string
  std::vector<Location> end_; ///< entries point to the subpattern's ending '|' or '\0'
  std::vector<bool>     acc_; ///< true if subpattern n is accepting (state is reachable)
  size_t                vno_; ///< number of finite state machine vertices |V|
  size_t                eno_; ///< number of finite state machine edges |E|
  const Opcode         *opc_; ///< points to the opcode table
  Index                 nop_; ///< number of opcodes generated
  FSM                   fsm_; ///< function pointer to FSM code
  #ifdef REFLEX_PATTERN_TIMER
  float                 pms_; ///< ms elapsed time to parse regex
  float                 vms_; ///< ms elapsed time to compile DFA vertices
  float                 ems_; ///< ms elapsed time to compile DFA edges
  float                 wms_; ///< ms elapsed time to assemble code words
  #endif

  Tree                  tfa_; ///< tree DFA constructed from strings (regex uses firstpos/lastpos/followpos)
  DFA                   dfa_; ///< DFA constructed from regex with subset construction using firstpos/lastpos/followpos
  size_t                len_; ///< prefix length of pre_[], less or equal to 255
  size_t                min_; ///< patterns after the prefix are at least this long but no more than 8
  char                  pre_[256];         ///< pattern prefix, shorter or equal to 255 bytes
  Pred                  bit_[256];         ///< bitap array
  Pred                  pmh_[Const::HASH]; ///< predict-match hash array
  Pred                  pma_[Const::HASH]; ///< predict-match array
  bool                  one_; ///< true if matching one string in pre_[] without meta/anchors
};

} // namespace reflex

#endif
