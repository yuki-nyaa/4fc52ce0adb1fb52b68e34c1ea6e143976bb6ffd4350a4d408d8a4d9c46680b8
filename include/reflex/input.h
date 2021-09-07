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
@file      input.h
@brief     RE/flex input character sequence class and HW accelleration
@author    Robert van Engelen - engelen@genivia.com
@copyright (c) 2016-2020, Robert van Engelen, Genivia Inc. All rights reserved.
@copyright (c) BSD-3 License - see LICENSE.txt
*/

#ifndef REFLEX_INPUT_H
#define REFLEX_INPUT_H

#include <reflex/utf8.h>
#include <cstdio>
#include <cstring>
#include <cassert>
#include <iostream>
#include <string>
#include <deque>

#if defined(HAVE_AVX512BW)
# include <immintrin.h>
#elif defined(HAVE_AVX2)
# include <immintrin.h>
#elif defined(HAVE_SSE2)
# include <emmintrin.h>
#elif defined(HAVE_NEON)
# include <arm_neon.h>
#endif

#if defined(HAVE_AVX512BW) || defined(HAVE_AVX2) || defined(HAVE_SSE2)
# ifdef _MSC_VER
#  include <intrin.h>
# endif
#endif

namespace reflex {
/// A simple wrapper for various input sources.
class Input {
 public:
  /// Input type
  enum struct Source_Type : unsigned char {NIL,FILE_P,STD_ISTREAM_P,CCHAR_P,SV,CUCHAR_P,USV,INPUT};
  typedef unsigned short codepage_unit_t; // 0-65535
  static const codepage_unit_t predefined_codepages[38][256];
  /// Common encoding constants.
  enum struct encoding : unsigned char  {
    // Do not change the order of the following enumerators!
    cp437, ///< DOS CP 437
    cp850, ///< DOS CP 850
    cp858, ///< DOS CP 858
    ebcdic, ///< EBCDIC
    cp1250, ///< Windows CP 1250
    cp1251, ///< Windows CP 1251
    cp1252, ///< Windows CP 1252
    cp1253, ///< Windows CP 1253
    cp1254, ///< Windows CP 1254
    cp1255, ///< Windows CP 1255
    cp1256, ///< Windows CP 1256
    cp1257, ///< Windows CP 1257
    cp1258, ///< Windows CP 1258
    iso8859_2, ///< ISO-8859-2, Latin-2
    iso8859_3, ///< ISO-8859-3, Latin-3
    iso8859_4, ///< ISO-8859-4, Latin-4
    iso8859_5, ///< ISO-8859-5, Cyrillic
    iso8859_6, ///< ISO-8859-6, Arabic
    iso8859_7, ///< ISO-8859-7, Greek
    iso8859_8, ///< ISO-8859-8, Hebrew
    iso8859_9, ///< ISO-8859-9, Latin-5
    iso8859_10, ///< ISO-8859-10, Latin-6
    iso8859_11, ///< ISO-8859-11, Thai
    iso8859_13, ///< ISO-8859-13, Latin-7
    iso8859_14, ///< ISO-8859-14, Latin-8
    iso8859_15, ///< ISO-8859-15, Latin-9
    iso8859_16, ///< ISO-8859-16
    macroman, ///< Macintosh Roman with CR to LF translation
    koi8_r, ///< KOI8-R
    koi8_u, ///< KOI8-U
    koi8_ru, ///< KOI8-RU
    // Do not change the order of the preceding enumerators!

    latin1,

    utf8, ///< Naturally this also covers plain ASCII
    utf16be, ///< UTF-16 big endian
    utf16le, ///< UTF-16 little endian
    utf32be, ///< UTF-32 big endian
    utf32le, ///< UTF-32 little endian

    auto_detect, ///< Try detecting one of the utf BOMs, consuming the BOM if successful. If none found, then utf8 is chosen.
    custom, ///< custom code page
  };
  static constexpr size_t get_raw_temp_default_size = 4; // Size just enough for normal use. You might increase it if you have special needs.
 private:
  unsigned char* allocate_get_raw_temp(size_t s) {return new unsigned char[s];}
  void free_get_raw_temp(unsigned char* p) {delete[] p;}
  void resize_get_raw_temp(size_t s){
    if(get_raw_temp_size_==s)
      return;
    else{
      free_get_raw_temp(get_raw_temp_);
      get_raw_temp_=allocate_get_raw_temp(s);
      get_raw_temp_size_=s;
    }
  }
  void detect_and_skip_bom();
  template<typename T>
  Input(Source_Type st,T t,encoding enc,const codepage_unit_t* page) noexcept :
    source_type_(st),
    source_(t),
    get_raw_temp_(nullptr), // Allocated when needed
    get_raw_temp_size_(0)
  {
    set_encoding(enc,page);
  }
 public:
  /// Construct empty input character sequence.
  Input() noexcept : Input(Source_Type::NIL,nullptr,encoding::utf8,nullptr) {}
  Input(const Input&) = delete;
  Input(Input&& other) noexcept
    :
      source_type_(other.source_type_),
      source_(other.source_),
      enc_(other.enc_),
      page_(other.page_),
      get_raw_temp_(other.get_raw_temp_),
      get_raw_temp_size_(other.get_raw_temp_size_)
  {
    other.set_source();
    other.get_raw_temp_=nullptr;
    other.get_raw_temp_size_=0;
  }
  /// Construct input character sequence from a char* string and a size.
  Input(
      const char *cstring, ///< char string
      size_t      size,    ///< length of the string
      encoding    enc = encoding::auto_detect,
      const codepage_unit_t* page = nullptr) noexcept
    :
      source_type_(Source_Type::SV),
      source_(cstring,size),
      get_raw_temp_(nullptr), // Allocated when needed
      get_raw_temp_size_(0)
  {
    set_encoding(enc,page);
  }
  /// Construct input character sequence from a NUL-terminated string.
  Input(const char *cstring, ///< NUL-terminated char* string
      encoding    enc = encoding::auto_detect,
      const codepage_unit_t* page = nullptr) noexcept
    :
    Input(Source_Type::CCHAR_P,cstring,enc,page)
  {}
  /// Construct input character sequence from a std::string.
  Input(const std::string& string,
      encoding    enc = encoding::auto_detect,
      const codepage_unit_t* page = nullptr) noexcept
    :
      Input(string.c_str(),string.size(),enc,page)
  {}
  /// Construct input character sequence from an open FILE* file descriptor.
  Input(FILE *file,
      encoding    enc = encoding::auto_detect,
      const codepage_unit_t* page = nullptr) noexcept
    :
    Input(Source_Type::FILE_P,file,enc,page)
  {}
  /// Construct input character sequence from a std::istream.
  Input(std::istream& istream,
      encoding    enc = encoding::auto_detect,
      const codepage_unit_t* page = nullptr) noexcept
    :
      Input(Source_Type::STD_ISTREAM_P,&istream,enc,page)
  {}
  /// Construct input character sequence from a unsigned char* string and a size.
  Input(
      const unsigned char *ucstring, ///< char string
      size_t      size,    ///< length of the string
      encoding    enc = encoding::auto_detect,
      const codepage_unit_t* page = nullptr) noexcept
    :
      source_type_(Source_Type::USV),
      source_(ucstring,size),
      get_raw_temp_(nullptr), // Allocated when needed
      get_raw_temp_size_(0)
  {
    set_encoding(enc,page);
  }
  /// Construct input character sequence from a NUL-terminated string.
  Input(const unsigned char *ucstring, ///< NUL-terminated char* string
      encoding    enc = encoding::auto_detect,
      const codepage_unit_t* page = nullptr) noexcept
    :
    Input(Source_Type::CUCHAR_P,ucstring,enc,page)
  {}
  /// Construct input character sequence from a std::basic_string<unsigned char>.
  Input(const std::basic_string<unsigned char>& ustring,
      encoding    enc = encoding::auto_detect,
      const codepage_unit_t* page = nullptr) noexcept
    :
      Input(ustring.c_str(),ustring.size(),enc,page)
  {}
  /// Construct input character sequence from another `reflex::Input`.
  Input(Input& in,
      encoding    enc = encoding::auto_detect,
      const codepage_unit_t* page = nullptr) noexcept
    :
      Input(Source_Type::INPUT,&in,enc,page)
  {}
  Input& operator=(const Input&) = delete;
  Input& operator=(Input&& other) noexcept
  {
    source_type_= other.source_type_;
    source_ = other.source_;
    enc_ = other.enc_;
    page_ = other.page_;
    get_raw_temp_=other.get_raw_temp_;
    get_raw_temp_size_=other.get_raw_temp_size_;
    other.set_source();
    other.get_raw_temp_=nullptr;
    other.get_raw_temp_size_=0;
    return *this;
  }
  ~Input() noexcept {
    free_get_raw_temp(get_raw_temp_);
  }
  Source_Type get_source_type() const {return source_type_;}
  /// Get the `const char*` of this Input object.
  const char *c_str() const
  {
    assert(source_type_==Source_Type::CCHAR_P || source_type_==Source_Type::SV);
    switch(source_type_){
      case Source_Type::CCHAR_P : return source_.cstring_;
      case Source_Type::SV : return source_.sv_.data_;
      default : return nullptr;
    }
  }
  /// Get the `FILE*` of this Input object.
  FILE *file() const
  {
    assert(source_type_==Source_Type::FILE_P);
    return source_.file_;
  }
  /// Get the `std::istream` of this Input object.
  std::istream& istream() const
  {
    assert(source_type_==Source_Type::STD_ISTREAM_P);
    return *(source_.istream_);
  }
  /// Get the `const unsigned char*` of this Input object.
  const unsigned char *u_c_str() const
  {
    assert(source_type_==Source_Type::CUCHAR_P || source_type_==Source_Type::USV);
    switch(source_type_){
      case Source_Type::CUCHAR_P : return source_.ucstring_;
      case Source_Type::USV : return source_.usv_.data_;
      default : return nullptr;
    }
  }
  /// Get the `reflex::Input` of this Input object.
  Input& input() const
  {
    assert(source_type_==Source_Type::INPUT);
    return *(source_.input_);
  }
  /// Get the remaining input size. Only meaningful for string view.
  size_t remaining_size() const {
    assert(source_type_==Source_Type::SV || source_type_==Source_Type::USV);
    switch(source_type_){
      case Source_Type::SV : return source_.sv_.size_;
      case Source_Type::USV : return source_.usv_.size_;
      default : return 0;
    }
  }
  /// Check if `get_raw()` can be performed.
  bool get_raw_able() const;
  explicit operator bool() const
    /// @returns `get_raw_able()`.
  {
    return get_raw_able();
  }
  /// Get unconverted bytes to `buf`. Has the same semantics as `fread`, except that a cast may be performed when the source type and `C` differ.
  template<typename C>
  size_t get_raw(C* buf,size_t size,size_t count);
  /// Get one unconverted byte. Has the same semantics as `fgetc`.
  int get_raw();
  /// Peek one unconverted byte.
  int peek_raw();
  /// Write to `s` one utf-8 converted code point (1-4 bytes).
  /// @returns utf-8 byte length.
  template<typename C>
  unsigned get(C* s);
  encoding get_encoding() const {return enc_;}
  void set_encoding(
      encoding    enc,         ///< encoding
      const codepage_unit_t *page = nullptr) ///< custom code page for encoding::custom
  {
    assert(enc == encoding::custom || page == nullptr); // Predefined encoding must NOT come with custom codepage.
    assert(enc != encoding::custom || page != nullptr); // Custom encoding must come with custom codepage.
    assert(source_type_!=Source_Type::CCHAR_P || (enc!=encoding::utf32be && enc!=encoding::utf32le)); // Since UTF-32 naturally contains lots of null bytes, determining end of input by null byte does not work.
    enc_ = enc;
    page_  = page;
    if(enc_==encoding::auto_detect)
      detect_and_skip_bom();
    if(enc_!=encoding::custom && enc_!=encoding::utf8 && enc_!=encoding::utf16be && enc_!=encoding::utf16le && enc_!=encoding::utf32be && enc_!=encoding::utf32le)
      page_ = predefined_codepages[static_cast<unsigned>(enc_)];
  }
  const codepage_unit_t* get_page() const {return page_;}
  void set_source() {source_type_ = Source_Type::NIL;}
  void set_source(FILE* f){
    source_type_ = Source_Type::FILE_P;
    source_.file_ = f;
  }
  void set_source(const char* s){
    source_type_ = Source_Type::CCHAR_P;
    source_.cstring_ = s;
  }
  void set_source(const char* s,size_t sz){
    source_type_ = Source_Type::SV;
    source_.sv_.data_ = s;
    source_.sv_.size_ = sz;
  }
  void set_source(std::istream& is){
    source_type_ = Source_Type::STD_ISTREAM_P;
    source_.istream_ = &is;
  }
  void set_source(const unsigned char* s){
    source_type_ = Source_Type::CUCHAR_P;
    source_.ucstring_ = s;
  }
  void set_source(const unsigned char* s,size_t sz){
    source_type_ = Source_Type::USV;
    source_.usv_.data_ = s;
    source_.usv_.size_ = sz;
  }
  void set_source(Input& in){
    source_type_ = Source_Type::INPUT;
    source_.input_ = &in;
  }
 protected:
  Source_Type source_type_;
  encoding              enc_;        ///< encoding
  const codepage_unit_t* page_;      ///< custom code page
  union Source_Union_{
    FILE                 *file_;    ///< FILE* input (when non-null)
    std::istream         *istream_; ///< stream input (when non-null)
    const char           *cstring_; ///< char string input (when non-null) of length reflex::Input::size_
    struct{const char* data_; size_t size_;} sv_;
    const unsigned char* ucstring_;
    struct{const unsigned char* data_; size_t size_;} usv_;
    Input* input_;
    constexpr Source_Union_(FILE* file_other) noexcept : file_(file_other) {}
    constexpr Source_Union_(std::istream* istream_other) noexcept : istream_(istream_other) {}
    constexpr Source_Union_(const char* cstring_other) noexcept : cstring_(cstring_other) {}
    constexpr Source_Union_(const char* d,size_t s) noexcept : sv_{d,s} {}
    constexpr Source_Union_(const unsigned char* ucstring_other) noexcept : ucstring_(ucstring_other) {}
    constexpr Source_Union_(const unsigned char* d,size_t s) noexcept : usv_{d,s} {}
    constexpr Source_Union_(Input* i) noexcept : input_(i) {}
    constexpr Source_Union_(std::nullptr_t = nullptr) noexcept : Source_Union_((FILE*)nullptr) {}
  } source_;
 private:
  unsigned char         utf8_buf_[4]; ///< Buffer for `to_utf8(char32_t,C* c)`. Also used as a temporary storage for `get(C*)`.
  unsigned char*        get_raw_temp_; ///< Temporary storage for `get_raw(C*,size_t,size_t)` in the case of `std::istream` and `FILE*`.
  size_t                get_raw_temp_size_;
}; // class Input

inline void Input::detect_and_skip_bom(){
  // TODO
  enc_ = encoding::utf8;
}

inline bool Input::get_raw_able() const
{
  switch(source_type_){
    case Source_Type::CCHAR_P :
      return *source_.cstring_!='\0';
    case Source_Type::SV :
      return source_.sv_.size_>0;
    case Source_Type::FILE_P :
      return feof(source_.file_)==0 && ferror(source_.file_)==0;
    case Source_Type::STD_ISTREAM_P :
      return !((source_.istream_)->good());
    case Source_Type::CUCHAR_P :
      return *source_.ucstring_!='\0';
    case Source_Type::USV :
      return source_.usv_.size_>0;
    case Source_Type::INPUT :
      return source_.input_->get_raw_able();
    default :
      return false;
  }
}

template<typename C>
size_t Input::get_raw(C* buf,size_t size,size_t count){
  static_assert(is_char_v<std::remove_cv_t<C>> && !std::is_const_v<C>);

  switch(source_type_){
    case Source_Type::CCHAR_P:{
      for(size_t i = 0;i<count;++i){
        for(size_t j = 0;j<size;++j)
          if(source_.cstring_[j]=='\0')
            return i;
        reflex::char_copy(buf,source_.cstring_,size);
        buf+=size;
        source_.cstring_+=size;
      }
      return count;
    }
    case Source_Type::SV:{
      for(size_t i = 0;i<count;++i){
        if(size>source_.sv_.size_)
          return i;
        else{
          reflex::char_copy(buf,source_.sv_.data_,size);
          buf+=size;
          source_.sv_.data_+=size;
          source_.sv_.size_-=size;
        }
      }
      return count;
    }
    case Source_Type::FILE_P:{
      if constexpr(std::is_same_v<std::remove_cv_t<C>,unsigned char>)
        return fread(buf,size,count,source_.file_);
      else{
        if(get_raw_temp_size_<size)
          resize_get_raw_temp(size>get_raw_temp_default_size ? size : get_raw_temp_default_size);
        for(size_t i=0;i<count;++i){
          if(fread(get_raw_temp_,size,1,source_.file_)==1){
            reflex::char_copy(buf,get_raw_temp_,size);
            buf+=size;
          }else
            return i;
        }
        return count;
      }
    }
    case Source_Type::STD_ISTREAM_P:{
      if(get_raw_temp_size_<size)
        resize_get_raw_temp(size>get_raw_temp_default_size ? size : get_raw_temp_default_size);
      for(size_t i = 0;i<count;++i){
        source_.istream_->read(reinterpret_cast<char*>(get_raw_temp_),size); // `unsigned char` and `char` can alias.
        if(static_cast<size_t>(source_.istream_->gcount())!=size)
          return i;
        else{
          reflex::char_copy(buf,reinterpret_cast<char*>(get_raw_temp_),size); // `unsigned char` and `char` can alias.
          buf+=size;
        }
      }
      return count;
    }
    case Source_Type::CUCHAR_P:{
      for(size_t i = 0;i<count;++i){
        for(size_t j = 0;j<size;++j)
          if(source_.ucstring_[j]=='\0')
            return i;
        reflex::char_copy(buf,source_.ucstring_,size);
        buf+=size;
        source_.ucstring_+=size;
      }
      return count;
    }
    case Source_Type::USV:{
      for(size_t i = 0;i<count;++i){
        if(size>source_.usv_.size_)
          return i;
        else{
          reflex::char_copy(buf,source_.usv_.data_,size);
          buf+=size;
          source_.usv_.data_+=size;
          source_.usv_.size_-=size;
        }
      }
      return count;
    }
    case Source_Type::INPUT:{
      return source_.input_->get_raw(buf,size,count);
    }
    default : return 0;
  }
} // size_t Input::get_raw(C* buf,size_t size,size_t count)

/// Get one unconverted byte. Has the same semantics as `fgetc`.
inline int Input::get_raw(){
  switch(source_type_){
    case Source_Type::CCHAR_P:
      if(source_.cstring_[0]!='\0')
        return static_cast<unsigned char>(*source_.cstring_++);
      else
        return EOF;
    case Source_Type::SV:
      if(source_.sv_.size_>0)
        return --source_.sv_.size_,static_cast<unsigned char>(*source_.sv_.data_++);
      else
        return EOF;
    case Source_Type::FILE_P:
      return fgetc(source_.file_);
    case Source_Type::STD_ISTREAM_P:
      return source_.istream_->get();
    case Source_Type::CUCHAR_P:
      if(source_.ucstring_[0]!=0)
        return *source_.ucstring_++;
      else
        return EOF;
    case Source_Type::USV:
      if(source_.usv_.size_>0)
        return --source_.usv_.size_,*source_.usv_.data_++;
      else
        return EOF;
    case Source_Type::INPUT:
      return source_.input_->get_raw();
    default: return EOF;
  }
}

/// Peek one unconverted byte.
inline int Input::peek_raw(){
  switch(source_type_){
    case Source_Type::CCHAR_P :
      return (*source_.cstring_!='\0') ? static_cast<unsigned char>(*source_.cstring_) : EOF;
    case Source_Type::SV :
      return (source_.sv_.size_>0) ? static_cast<unsigned char>(*source_.sv_.data_) : EOF;
    case Source_Type::FILE_P:{
      int c = fgetc(source_.file_);
      return (c!=EOF) ? (ungetc(c,source_.file_),c) : EOF;
    }
    case Source_Type::STD_ISTREAM_P :
      return source_.istream_->peek();
    case Source_Type::CUCHAR_P :
      return (*source_.ucstring_!=0) ? *source_.ucstring_ : EOF;
    case Source_Type::USV :
      return (source_.usv_.size_>0) ? *source_.usv_.data_ : EOF;
    case Source_Type::INPUT :
      return source_.input_->peek_raw();
    default:
      return EOF;
  }
}

template<typename C>
unsigned Input::get(C* s){
  unsigned l = 0;
  switch (enc_)
  {
    case encoding::utf8:{
      int c = get_raw();
      if(c==EOF || c>=0xF8)
        l=0;
      else if(c<0x80)
        l=1,*s = static_cast<C>(c);
      else{
        utf8_buf_[0] = static_cast<unsigned char>(c);
        if(c>=0xF0){
          if(get_raw(utf8_buf_+1, 3, 1) == 1)
            l=4,reflex::char_copy(s,utf8_buf_,4);
        }else if(c>=0xE0){
          if(get_raw(utf8_buf_+1, 2, 1) == 1)
            l=3,reflex::char_copy(s,utf8_buf_,3);
        }else if(c>=0xC0){
          if(get_raw(utf8_buf_+1, 1, 1) == 1)
            l=2,reflex::char_copy(s,utf8_buf_,2);
        }
      }
      break;
    }
    case encoding::utf16be:{
      if (get_raw(utf8_buf_, 2, 1) == 1)
      {
        char32_t c = utf8_buf_[0] << 8 | utf8_buf_[1];
        if (c < 0x80)
          l=1,*s = static_cast<C>(c);
        else
        {
          if (c >= 0xD800 && c < 0xE000)
          {
            // UTF-16 surrogate pair
            if (c < 0xDC00 && get_raw(utf8_buf_ + 2, 2, 1) == 1 && (utf8_buf_[2] & 0xFC) == 0xDC)
              c = 0x010000 - 0xDC00 + ((c - 0xD800) << 10) + (utf8_buf_[2] << 8 | utf8_buf_[3]);
            else
              c = ERR_CHAR;
          }
          l = to_utf8(c, utf8_buf_);
          reflex::char_copy(s,utf8_buf_,l);
        }
      }
      break;
    }
    case encoding::utf16le:{
      if (get_raw(utf8_buf_, 2, 1) == 1)
      {
        char32_t c = utf8_buf_[0] | utf8_buf_[1] << 8;
        if (c < 0x80)
          l=1,*s = static_cast<C>(c);
        else
        {
          if (c >= 0xD800 && c < 0xE000)
          {
            // UTF-16 surrogate pair
            if (c < 0xDC00 && get_raw(utf8_buf_ + 2, 2, 1) == 1 && (utf8_buf_[3] & 0xFC) == 0xDC)
              c = 0x010000 - 0xDC00 + ((c - 0xD800) << 10) + (utf8_buf_[2] | utf8_buf_[3] << 8);
            else
              c = ERR_CHAR;
          }
          l = to_utf8(c, utf8_buf_);
          reflex::char_copy(s,utf8_buf_,l);
        }
      }
      break;
    }
    case encoding::utf32be:{
      if (get_raw(utf8_buf_, 4, 1) == 1)
      {
        char32_t c = utf8_buf_[0] << 24 | utf8_buf_[1] << 16 | utf8_buf_[2] << 8 | utf8_buf_[3];
        if (c < 0x80)
          l=1,*s = static_cast<C>(c);
        else{
          l = to_utf8(c, utf8_buf_);
          reflex::char_copy(s,utf8_buf_,l);
        }
      }
      break;
    }
    case encoding::utf32le:{
      if (get_raw(utf8_buf_, 4, 1) == 1)
      {
        char32_t c = utf8_buf_[0] | utf8_buf_[1] << 8 | utf8_buf_[2] << 16 | utf8_buf_[3] << 24;
        if (c < 0x80)
          l=1,*s = static_cast<C>(c);
        else{
          l = to_utf8(c, utf8_buf_);
          reflex::char_copy(s,utf8_buf_,l);
        }
      }
      break;
    }
    case encoding::latin1:{
      int byte = get_raw();
      if(byte!=EOF){
        unsigned char c = byte;
        if (c < 0x80)
          l=1,*s = static_cast<C>(c);
        else{
          l = to_utf8(c, utf8_buf_);
          reflex::char_copy(s,utf8_buf_,l);
        }
      }
      break;
    }
    case encoding::cp437:
    case encoding::cp850:
    case encoding::cp858:
    case encoding::ebcdic:
    case encoding::cp1250:
    case encoding::cp1251:
    case encoding::cp1252:
    case encoding::cp1253:
    case encoding::cp1254:
    case encoding::cp1255:
    case encoding::cp1256:
    case encoding::cp1257:
    case encoding::cp1258:
    case encoding::iso8859_2:
    case encoding::iso8859_3:
    case encoding::iso8859_4:
    case encoding::iso8859_5:
    case encoding::iso8859_6:
    case encoding::iso8859_7:
    case encoding::iso8859_8:
    case encoding::iso8859_9:
    case encoding::iso8859_10:
    case encoding::iso8859_11:
    case encoding::iso8859_13:
    case encoding::iso8859_14:
    case encoding::iso8859_15:
    case encoding::iso8859_16:
    case encoding::macroman:
    case encoding::koi8_r:
    case encoding::koi8_u:
    case encoding::koi8_ru:
    case encoding::custom:{
      int byte = get_raw();
      if(byte!=EOF){
        codepage_unit_t c = page_[static_cast<unsigned char>(byte)];
        if (c < 0x80)
          l=1,*s = static_cast<C>(c);
        else{
          l = to_utf8(c, utf8_buf_);
          reflex::char_copy(s,utf8_buf_,l);
        }
      }
      break;
    }
  }
  return l;
} // size_t Input::get(C* s)




class Input1_ : protected Input {using Input::Input;};
class Input2_ : protected Input {using Input::Input;};

/// Augmented `Input` capable of doing unget, lookahead, and position tracking.
class BufferedInput : private Input1_, private Input2_{
 public:
  using Input1_::Source_Type;
  using Input1_::codepage_unit_t;
  using Input1_::predefined_codepages;
  using Input1_::encoding;
  Source_Type get_source_type() const {return Input1_::source_type_;}
  const char* c_str() const {return Input1_::c_str();}
  FILE *file() const {return Input1_::file();}
  std::istream& istream() const {return Input1_::istream();}
  const unsigned char* u_c_str() const {return Input1_::u_c_str();}
  Input& input() const {return Input1_::input();}
  size_t remaining_size() const { return Input1_::remaining_size();}
  encoding get_encoding() const {return Input1_::enc_;}
  const codepage_unit_t* get_page() const {return Input1_::page_;}
 private:
  unsigned char* allocate_buffer(size_t s) {return new unsigned char[s];}
  void free_buffer(unsigned char* p) {delete[] p;}
  bool no_buffer_needed() const {
    return Input1_::source_type_==Source_Type::CCHAR_P || Input1_::source_type_==Source_Type::SV || Input1_::source_type_==Source_Type::CUCHAR_P || Input1_::source_type_==Source_Type::USV;
  }
 public:
  BufferedInput() noexcept :
    Input1_(),
    Input2_(),
    buf_raw(nullptr),
    size_buf_raw(0),
    pos_buf_u8(0),
    dend_buf_u8(0),
    buf_unget(),
    lineno(1),
    colno(1)
  {
    Input2_::set_source(static_cast<Input1_&>(*this));
  }

  template<typename... Args>
  explicit BufferedInput(Args&&... args) noexcept :
    Input1_(std::forward<Args>(args)...),
    Input2_(),
    buf_raw(nullptr),
    size_buf_raw(0),
    pos_buf_u8(0),
    dend_buf_u8(0),
    buf_unget(),
    lineno(1),
    colno(1)
  {
    Input2_::set_source(static_cast<Input1_&>(*this));
    Input2_::set_encoding(Input1_::enc_,Input1_::page_);
  }

  BufferedInput(const BufferedInput&) = delete;

  BufferedInput(BufferedInput&& other) noexcept :
    Input1_(std::move(other)),
    Input2_(std::move(other)),
    buf_raw(other.buf_raw),
    size_buf_raw(other.size_buf_raw),
    pos_buf_u8(other.pos_buf_u8),
    dend_buf_u8(other.dend_buf_u8),
    buf_unget(std::move(other.buf_unget)),
    lineno(other.lineno),
    colno(other.colno)
  {
    std::memcpy(buf_u8,other.buf_u8,4);

    other.buf_raw=nullptr;
    other.size_buf_raw=0;
    other.pos_buf_u8=0;
    other.dend_buf_u8=0;
    other.Input2_::set_source(static_cast<Input1_&>(other));
    other.reset_pos();
  }

  BufferedInput& operator=(const BufferedInput&) = delete;

  BufferedInput& operator=(BufferedInput&& other) noexcept {
    if(this!=&other){
      Input1_::operator=(std::move(other)),
      Input2_::operator=(std::move(other)),
      buf_raw=other.buf_raw;
      size_buf_raw=other.size_buf_raw;
      pos_buf_u8=other.pos_buf_u8;
      dend_buf_u8=other.dend_buf_u8;
      buf_unget = std::move(other.buf_unget);
      lineno = other.lineno;
      colno = other.colno;
      std::memcpy(buf_u8,other.buf_u8,4);

      other.buf_raw=nullptr;
      other.size_buf_raw=0;
      other.pos_buf_u8=0;
      other.dend_buf_u8=0;
      other.Input2_::set_source(static_cast<Input1_&>(other));
      other.reset_pos();
    }
    return *this;
  }

  ~BufferedInput() noexcept {free_buffer(buf_raw);}

  void resize_buffer(size_t size_new){
    if(size_new==size_buf_raw)
      return;
    else{
      if(size_new==0){
        for(size_t i=0;i!=Input2_::source_.usv_.size_;++i)
          buf_unget.push_back(Input2_::source_.usv_.data_[i]);
        free_buffer(buf_raw);
        buf_raw=nullptr;
        size_buf_raw=0;
        Input2_::set_source(static_cast<Input1_&>(*this));
      }else{
        unsigned char* buf_new = allocate_buffer(size_new);
        if(size_buf_raw!=0){
          size_t head = size_new<Input2_::source_.usv_.size_ ? Input2_::source_.usv_.size_-size_new : 0 ;
          std::memcpy(buf_new,Input2_::source_.usv_.data_+head,Input2_::source_.usv_.size_-head);
          for(size_t i=0;i!=head;++i)
            buf_unget.push_back(Input2_::source_.usv_.data_[i]);
          free_buffer(buf_raw);
          buf_raw = buf_new;
          size_buf_raw = size_new;
          Input2_::set_source(buf_new,Input2_::source_.usv_.size_-head);
        }else{
          buf_raw = buf_new;
          size_buf_raw = size_new;
          Input2_::set_source(buf_raw,0);
        }
      }
    }
  }

  size_t fill_buffer(){
    size_t filled = 0;
    if(size_buf_raw!=0){
      std::memmove(buf_raw,Input2_::source_.usv_.data_,Input2_::source_.usv_.size_);
      filled = Input1_::get_raw(buf_raw+Input2_::source_.usv_.size_,1,size_buf_raw-Input2_::source_.usv_.size_);
      Input2_::set_source(buf_raw,Input2_::source_.usv_.size_+filled);
    }
    return filled;
  }

  size_t buffer_size() const {return size_buf_raw;}

  static constexpr size_t BUFFER_SIZE_DEFAULT = 512*1024;
 private:
  int get_utf8_byte_ignoring_unget(){
    if(pos_buf_u8==dend_buf_u8 || dend_buf_u8==0){
      if((dend_buf_u8=Input2_::get(buf_u8))==0){
        if(fill_buffer()==0){
          return EOF;
        }else if((dend_buf_u8=Input2_::get(buf_u8))==0){
          return EOF;
        }
      }
      pos_buf_u8=0;
    }
    int c = buf_u8[pos_buf_u8++];
    if(c==static_cast<unsigned char>('\n')){
        ++lineno;
        colno=1;
      }else
        ++colno;
      return c;
  }
 public:
  int get_utf8_byte(){
    if(buf_unget.size()!=0){
      unsigned char c = buf_unget.front();
      buf_unget.pop_front();
      if(c==static_cast<unsigned char>('\n')){
        ++lineno;
        colno=1;
      }else
        ++colno;
      return c;
    }
    if(size_buf_raw==0 && !no_buffer_needed())
      resize_buffer(BUFFER_SIZE_DEFAULT);
    return get_utf8_byte_ignoring_unget();
  }

  BufferedInput& operator>>(int& c) {c=get_utf8_byte();return *this;}

  int peek_utf8_byte(size_t i=0){
    if(i<buf_unget.size())
      return buf_unget[i];
    else
      i-=buf_unget.size();++i;

    // At this point, i>=1.

    if(size_buf_raw==0 && !no_buffer_needed())
      resize_buffer(BUFFER_SIZE_DEFAULT);

    int c=EOF;
    for(;i!=0;--i){
      c = get_utf8_byte_ignoring_unget();
      if(c==EOF)
        return c;
      else
        buf_unget.push_back(c);
    }
    return c;
  }

  int operator[](size_t i) {return peek_utf8_byte(i);}

  void set_encoding(encoding enc,const codepage_unit_t* page){
    Input1_::set_encoding(enc,page);
    Input2_::set_encoding(enc,page);
  }

  // The following functions does not affect the current buffer. Better use when EOF is hit.
  void set_source() {Input1_::source_type_ = Source_Type::NIL;}
  void set_source(FILE* f) {Input1_::set_source(f);}
  void set_source(const char* s) {resize_buffer(0);Input1_::set_source(s);}
  void set_source(const char* s,size_t sz) {resize_buffer(0);Input1_::set_source(s,sz);}
  void set_source(std::istream& is) {Input1_::set_source(is);}
  void set_source(const unsigned char* s) {resize_buffer(0);Input1_::set_source(s);}
  void set_source(const unsigned char* s,size_t sz) {resize_buffer(0);Input1_::set_source(s,sz);}
  void set_source(Input& in) {Input1_::set_source(in);}

  void unget(char32_t c){
    if(c<=0x80){
      buf_unget.push_front(c);
      if(c==static_cast<unsigned char>('\n')){
        --lineno;
        colno=1;
      }else
        --colno;
      return;
    }
    unsigned char arr[4];
    unsigned l = to_utf8(c,arr);
    for(;l!=0;--l){
      buf_unget.push_front(arr[l-1]);
    }
    colno-=l;
  }

  BufferedInput& operator<<(char32_t c) {unget(c);return *this;}

  void reset_lineno() {lineno=1;}
  void reset_colno() {colno=1;}
  void reset_pos() {lineno=1;colno=1;}

  std::string get_line(){
    std::string s;
    int c=0;
    while(1){
      c = get_utf8_byte();
      if(c==EOF)
        return s;
      if(c==static_cast<unsigned char>('\n')){
        if(s.back()=='\r')
          s.pop_back();
        return s;
      }
      s.push_back(static_cast<char>(c));
    }
  }

 private:
  unsigned char* buf_raw;
  size_t size_buf_raw;
  unsigned char buf_u8[4];
  unsigned pos_buf_u8;
  unsigned dend_buf_u8;

  std::deque<unsigned char> buf_unget;
 public:
  size_t lineno;
  size_t colno;
};

#if defined(HAVE_AVX512BW) || defined(HAVE_AVX2) || defined(HAVE_SSE2)

#ifdef _MSC_VER
#pragma intrinsic(_BitScanForward)
inline uint_least32_t ctz(uint_least32_t x)
{
  unsigned long r;
  _BitScanForward(&r, x);
  return r;
}
inline uint_least32_t popcount(uint_least32_t x)
{
  return __popcnt(x);
}
#ifdef _WIN64
#pragma intrinsic(_BitScanForward64)
inline uint_least32_t ctzl(uint_least64_t x)
{
  unsigned long r;
  _BitScanForward64(&r, x);
  return r;
}
inline uint_least32_t popcountl(uint_least64_t x)
{
  return static_cast<uint_least32_t>(__popcnt64(x));
}
#endif
#else
inline uint_least32_t ctz(uint_least32_t x)
{
  return __builtin_ctz(x);
}
inline uint_least32_t ctzl(uint_least64_t x)
{
  return __builtin_ctzl(x);
}
inline uint_least32_t popcount(uint_least32_t x)
{
  return __builtin_popcount(x);
}
inline uint_least32_t popcountl(uint_least64_t x)
{
  return __builtin_popcountl(x);
}
#endif

#endif

extern uint_least64_t HW;

inline bool have_HW_AVX512BW()
{
  return HW & (1ULL << 62);
}

inline bool have_HW_AVX2()
{
  return HW & (1ULL << 37);
}

inline bool have_HW_SSE2()
{
  return HW & (1ULL << 26);
}

} // namespace reflex

#endif
