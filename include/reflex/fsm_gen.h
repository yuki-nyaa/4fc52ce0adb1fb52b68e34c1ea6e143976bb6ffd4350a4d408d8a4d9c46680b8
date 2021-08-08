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
@file      fsm_gen.h
@brief     RE/flex FSM generator
@author    Robert van Engelen - engelen@genivia.com
@copyright (c) 2016-2020, Robert van Engelen, Genivia Inc. All rights reserved.
@copyright (c) BSD-3 License - see LICENSE.txt
*/

#pragma once

#include<string>
#include<vector>
#include<map>
#include<set>
#include<list>
#include<reflex/ranges.h>
#include<reflex/pattern.hpp>

namespace reflex{
class FSM_Generator{
 public:
  /// Construct a pattern object given a regex string.
  FSM_Generator(std::string_view rex)
  {
    generate(rex);
  }
  /// Construct a pattern object given a regex string.
  FSM_Generator(std::string_view rex, Option&& opt):
    opt_(std::move(opt))
  {
    generate(rex);
  }

  /// Global modifier modes, syntax flags, and compiler options.
  struct Option{
    bool  disable_escapes_in_bra = false; ///< disable escapes in bracket lists // b
    Char  escape_char = (unsigned char)`\\`; ///< escape character, or > 255 for none, '\\' default // e
    bool  case_insensitive = false; ///< case insensitive mode, also `(?i:X)` // i
    bool  multi_line = false; ///< multi-line mode, also `(?m:X)` // m
    bool  optimize = false; ///< generate optimized FSM code for option f // o
    bool  predict_match = false; ///< with option f also output predict match array for fast search with find() // p
    bool  verbatim_content = false; ///< enable "X" quotation of verbatim content, also `(?q:X)` // q
    bool  throw_error = false; ///< raise syntax errors // r
    bool  single_line = false; ///< single-line mode (dotall mode), also `(?s:X)` // s
    bool  print_error = false; ///< write error message to stderr // w
    bool  free_space = false; ///< free-spacing mode, also `(?x:X)` // x

    std::vector<std::string> files; ///< output to files // f
    std::string pattern_name; ///< pattern name (for use in generated code) // n
    std::string namespace_name; ///< namespace (NAME1.NAME2.NAME3) // z
  };

  void generate(std::string_view rex){
    rex_ = rex;
    Positions startpos;
    Follow    followpos;
    Map       modifiers;
    Map       lookahead;
    // parse the regex pattern to construct the followpos NFA without epsilon transitions
    parse(startpos, followpos, modifiers, lookahead);
    // start state = startpos = firstpost of the followpos NFA, also merge the tree DFA root when non-nullptr
    DFA::State *start = dfa_.state(tfa_.tree, startpos);
    // compile the NFA into a DFA
    compile(start, followpos, modifiers, lookahead);
    // assemble DFA opcode tables or direct code
    assemble(start);
    // delete the DFA
    dfa_.clear();
  }

  struct Const{
    static constexpr Pattern::Accept AMAX = 0xFDFFFF; ///< max accept
    static constexpr Pattern::Index  GMAX = 0xFEFFFF;   ///< max goto index
    static constexpr Pattern::Index  LMAX = 0xFAFFFF;   ///< max lookahead index
  };

 private:
  typedef unsigned short Char; // 8 bit char and meta chars up to META_MAX-1
  typedef unsigned char Lazy;
  typedef unsigned short Iter;
  typedef uint_least32_t Location;
  typedef ORanges<Location> Locations;
  typedef std::map<int,Locations> Map;
  typedef std::set<Pattern::Lookahead> Lookaheads;

  /// Finite state machine construction position information.
  struct Position{
    typedef unsigned long long  value_type;
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
    Pattern::Accept accepts()        const { return static_cast<Pattern::Accept>(k); }
    Iter     iter()                  const { return static_cast<Pattern::Index>((k >> 32) & 0xFFFF); }
    bool     negate()                const { return (k & NEGATE) != 0; }
    bool     ticked()                const { return (k & TICKED) != 0; }
    bool     greedy()                const { return (k & GREEDY) != 0; }
    bool     anchor()                const { return (k & ANCHOR) != 0; }
    bool     accept()                const { return (k & ACCEPT) != 0; }
    Lazy     lazy()                  const { return static_cast<Lazy>(k >> 56); }
    value_type k;
  };

  typedef std::set<Position> Positions;
  typedef std::map<Position,Positions> Follow;

  /// Tree DFA constructed from string patterns.
  struct Tree
  {
    struct Node {
      Node  *edge[256] = {}; ///< 256 edges, one per 8-bit char
      Pattern::Accept accept = 0;    ///< nonzero if final state, the index of an accepted/captured subpattern
    };
    static constexpr unsigned short ALLOC_SIZE = 64; ///< allocate 64 nodes at a time, to improve performance
    Tree():
      tree(nullptr),
      next(ALLOC_SIZE)
    {}
    ~Tree()
    {
      clear();
    }
    /// delete the tree DFA.
    void clear()
    {
      for (std::list<Node*>::iterator i = list.begin(); i != list.end(); ++i)
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
      if (next >= ALLOC_SIZE)
      {
        list.push_back(new Node[ALLOC_SIZE]);
        next = 0;
      }
      return &list.back()[next++];
    }
    Node    *tree; ///< root of the tree or nullptr
    std::list<Node*> list; ///< block allocation list
    unsigned short next; ///< block allocation, next available slot in last block
  }; // struct Tree

  /// DFA created by subset construction from regex patterns.
  struct DFA {
    struct State : Positions {
      typedef std::map<Char,std::pair<Char,State*> > Edges;
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
      State      *next = nullptr;   ///< points to next state in the list of states allocated depth-first by subset construction
      State      *left = nullptr;   ///< left pointer for O(log N) node insertion in the hash table overflow tree
      State      *right = nullptr;  ///< right pointer for O(log N) node insertion in the hash table overflow tree
      Tree::Node *tnode = nullptr;  ///< the corresponding tree DFA node, when applicable
      Edges       edges;  ///< state transitions
      Pattern::Index  first = 0;  ///< index of this state in the opcode table, determined by the first assembly pass
      Pattern::Index  index = 0;  ///< index of this state in the opcode table
      Pattern::Accept accept = 0; ///< nonzero if final state, the index of an accepted/captured subpattern
      Lookaheads  heads;  ///< lookahead head set
      Lookaheads  tails;  ///< lookahead tail set
      bool        redo = false;   ///< true if this is a final state of a negative pattern
    };
    static constexpr unsigned short ALLOC_SIZE = 256; ///< allocate 256 states at a time, to improve performance.
    DFA():
      next(ALLOC_SIZE)
    {}
    ~DFA()
    {
      clear();
    }
    /// delete DFA
    void clear()
    {
      for (std::list<State*>::iterator i = list.begin(); i != list.end(); ++i)
        delete[] *i;
      list.clear();
    }
    /// new DFA state with optional tree DFA node.
    State *state(Tree::Node *node)
    {
      if (next >= ALLOC_SIZE)
      {
        list.push_back(new State[ALLOC_SIZE]);
        next = 0;
      }
      return list.back()[next++].assign(node);
    }
    /// new DFA state with optional tree DFA node and positions, destroys pos.
    State *state(Tree::Node *node, Positions& pos)
    {
      if (next >= ALLOC_SIZE)
      {
        list.push_back(new State[ALLOC_SIZE]);
        next = 0;
      }
      return list.back()[next++].assign(node, pos);
    }
    std::list<State*> list; ///< block allocation list
    unsigned short next; ///< block allocation, next available slot in last block
  }; // struct DFA

  /// Set of chars and meta chars
  struct Chars {
    Chars()                                 { clear(); }
    Chars(const Chars& c)                   { b[0] = c.b[0]; b[1] = c.b[1]; b[2] = c.b[2]; b[3] = c.b[3]; b[4] = c.b[4]; }
    Chars(const unsigned long long c[5])    { b[0] = c[0]; b[1] = c[1]; b[2] = c[2]; b[3] = c[3]; b[4] = c[4]; }
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
    unsigned long long b[4+1]; ///< 256 bits to store a set of 8-bit chars + extra bits for meta
  };

  struct Move{
    Chars first;
    Positions second;
  };
  typedef std::list<Move> Moves;

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
      std::set<Lazy>& lazyset,
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
      std::set<Lazy>& lazyset,
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
      std::set<Lazy>& lazyset,
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
      std::set<Lazy>& lazyset,
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
      const std::set<Lazy>& lazyset,
      Positions&     pos) const;
  void lazy(
      const std::set<Lazy>& lazyset,
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
  void predict_match_dfa(DFA::State *start);
  void gen_predict_match(DFA::State *state);
  void gen_predict_match_transitions(DFA::State *state, std::map<DFA::State*,ORanges<Pattern::Hash> >& states);
  void gen_predict_match_transitions(size_t level, DFA::State *state, ORanges<Pattern::Hash>& labels, std::map<DFA::State*,ORanges<Pattern::Hash>>& states);
  void export_dfa(const DFA::State *start) const;
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
  void export_code() const;
  void write_predictor(FILE *fd) const;
  void write_namespace_open(FILE* fd) const;
  void write_namespace_close(FILE* fd) const;

  static constexpr const char* posix_class[14] = {"ASCII","Space","XDigit","Cntrl","Print","Alnum","Alpha","Blank","Digit","Graph","Lower","Punct","Upper","Word",};
  static constexpr const char* meta_label[14] = {nullptr,"NWB","NWE","BWB","EWB","BWE","EWE","BOL","EOL","BOB","EOB","UND","IND","DED",};

  /// Throw an error.
  void error(
      regex_error_type code,    ///< error code
      size_t           pos = 0) ///< optional location of the error in regex string Pattern::rex_
    const noexcept(false)
  {
    regex_error err(code, rex_, pos);
    if (opt_.print_error)
      fprintf(stderr,"%s",err.what());
    if (code == regex_error::exceeds_limits || opt_.throw_error)
      throw err;
 }

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

  static bool is_modified(
      Char       mode,
      const Map& modifiers,
      Location   loc)
  {
    Map::const_iterator i = modifiers.find(mode);
    return i != modifiers.end() && i->second.find(loc) != i->second.end();
  }
  static void update_modified(
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

  static constexpr bool is_meta(Char c)
  {
    return c > Pattern::Meta::MIN;
  }
  static constexpr Char lowercase(Char c)
  {
    return static_cast<unsigned char>(c | 0x20);
  }
  static constexpr Char uppercase(Char c)
  {
    return static_cast<unsigned char>(c & ~0x20);
  }
  static constexpr Char reversecase(Char c)
  {
    return static_cast<unsigned char>(c ^ 0x20);
  }

  static constexpr bool valid_goto_index(Pattern::Index index)
  {
    return index <= Const::GMAX;
  }
  static constexpr bool valid_lookahead_index(Pattern::Index index)
  {
    return index <= Const::LMAX;
  }

  static constexpr Pattern::Opcode opcode_head(Pattern::Index index)
  {
    return 0xFB000000 | (index & 0xFFFFFF); // index <= Const::LMAX (0xFAFFFF max)
  }
  static constexpr Pattern::Opcode opcode_goto(
      Char  lo,
      Char  hi,
      Pattern::Index index)
  {
    return is_meta(lo) ? (static_cast<Pattern::Opcode>(lo) << 24) | index : (static_cast<Pattern::Opcode>(lo) << 24) | (hi << 16) | index;
  }
  static constexpr Pattern::Opcode opcode_long(Pattern::Index index)
  {
    return 0xFF000000 | (index & 0xFFFFFF); // index <= Const::GMAX (0xFEFFFF max)
  }
  static constexpr Pattern::Opcode opcode_redo()
  {
    return 0xFD000000;
  }
  static constexpr Pattern::Opcode opcode_take(Pattern::Index index)
  {
    return 0xFE000000 | (index & 0xFFFFFF); // index <= Const::AMAX (0xFDFFFF max)
  }
  static constexpr Pattern::Opcode opcode_tail(Pattern::Index index)
  {
    return 0xFC000000 | (index & 0xFFFFFF); // index <= Const::LMAX (0xFAFFFF max)
  }

  static constexpr bool is_opcode_meta(Pattern::Opcode opcode)
  {
    return (opcode & 0x00FF0000) == 0x00000000 && (opcode >> 24) > 0;
  }
  static constexpr bool is_opcode_head(Pattern::Opcode opcode)
  {
    return (opcode & 0xFF000000) == 0xFB000000;
  }
  static constexpr bool is_opcode_redo(Pattern::Opcode opcode)
  {
    return opcode == 0xFD000000;
  }
  static constexpr bool is_opcode_take(Pattern::Opcode opcode)
  {
    return (opcode & 0xFE000000) == 0xFE000000;
  }
  static constexpr bool is_opcode_tail(Pattern::Opcode opcode)
  {
    return (opcode & 0xFF000000) == 0xFC000000;
  }
  static constexpr bool is_opcode_halt(Pattern::Opcode opcode)
  {
    return Pattern::is_opcode_halt(opcode);
  }
  static constexpr Char meta_of(Pattern::Opcode opcode)
  {
    return Pattern::Meta::MIN + (opcode >> 24);
  }

  static constexpr Char lo_of(Pattern::Opcode opcode)
  {
    return is_opcode_meta(opcode) ? meta_of(opcode) : opcode >> 24;
  }
  static constexpr Char hi_of(Pattern::Opcode opcode)
  {
    return is_opcode_meta(opcode) ? meta_of(opcode) : (opcode >> 16) & 0xFF;
  }
  static constexpr Pattern::Index index_of(Pattern::Opcode opcode)
  {
    return Pattern::index_of(opcode);
  }
  static constexpr Pattern::Index long_index_of(Pattern::Opcode opcode)
  {
    return Pattern::long_index_of(opcode);
  }

  static constexpr Pattern::Hash hash(Pattern::Hash h)
  {
    return h & ((Pattern::Const::HASH - 1) >> 3);
  }
  static unsigned short hash_pos(const Positions *pos)
  {
    unsigned short h = 0;
    for (Positions::const_iterator i = pos->begin(); i != pos->end(); ++i)
      h += static_cast<unsigned short>(*i ^ (*i >> 24)); // (Position{*i}.iter() << 4) unique hash for up to 16 chars iterated (abc...p){iter}
    return h;
  }

 public:
  Option opt_;
 private:
  std::string_view rex_;

  Pattern::Opcode      *opc_; ///< points to the generated opcode table
  Pattern::Index        nop_; ///< number of opcodes generated

  Pattern::Predictor pred_;

  Tree                  tfa_; ///< tree DFA constructed from strings (regex uses firstpos/lastpos/followpos)
  DFA                   dfa_; ///< DFA constructed from regex with subset construction using firstpos/lastpos/followpos
  size_t                vno_; ///< number of finite state machine vertices |V|
  size_t                eno_; ///< number of finite state machine edges |E|
  std::basic_string<bool> acc_; ///< true if subpattern n is accepting (state is reachable)
  std::basic_string<Location> end_; ///< entries point to the subpattern's ending '|' or '\0'

  float pms_; ///< ms elapsed time to parse regex
  float vms_; ///< ms elapsed time to compile DFA vertices
  float ems_; ///< ms elapsed time to compile DFA edges
  float wms_; ///< ms elapsed time to assemble code words
};
} // namespace reflex