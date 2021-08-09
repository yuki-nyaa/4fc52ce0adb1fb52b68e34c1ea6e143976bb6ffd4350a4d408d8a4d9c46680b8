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
/// Input character sequence class for unified access to sources of input text.
/**
Description
-----------

The Input class unifies access to a source of input text that constitutes a
sequence of characters:

- An Input object is instantiated and (re)assigned a (new) source input: either
  a `char*` string, a `std::string`, a `std::u32string`, a `FILE*` descriptor,
  or a `std::istream` object.

- When assigned a wide string source as input, the wide character content is
  automatically converted to an UTF-8 character sequence when reading with
  get().  Wide strings are UCS-2/UCS-4 and may contain UTF-16 surrogate pairs.

- When assigned a `FILE*` source as input, the file is checked for the presence
  of a UTF-8 or a UTF-16 BOM (Byte Order Mark). A UTF-8 BOM is ignored and will
  not appear on the input character stream (and size is adjusted by 3 bytes). A
  UTF-16 BOM is intepreted, resulting in the conversion of the file content
  automatically to an UTF-8 character sequence when reading the file with
  get(). Also, size() gives the content size in the number of UTF-8 bytes.

- An input object can be reassigned a new source of input for reading at any
  time.

- An input object obeys move semantics. That is, after assigning an input
  object to another, the former can no longer be used to read input. This
  prevents adding the overhead and complexity of file and stream duplication.

- `size_t Input::get(char *buf, size_t len);` reads source input and fills `buf`
  with up to `len` bytes, returning the number of bytes read or zero when a
  stream or file is bad or when EOF is reached.

- `size_t Input::size();` returns the number of ASCII/UTF-8 bytes available
  to read from the source input or zero (zero is also returned when the size is
  not determinable). Use this function only before reading input with get().
  Wide character strings and UTF-16 `FILE*` content is counted as the total
  number of UTF-8 bytes that will be produced by get(). The size of a
  `std::istream` cannot be determined.

- `bool Input::good();` returns true if the input is readable and has no
  EOF or error state.  Returns false on EOF or if an error condition is
  present.

- `bool Input::eof();` returns true if the input reached EOF. Note that
  good() == ! eof() for string source input only, since files and streams may
  have error conditions that prevent reading. That is, for files and streams
  eof() implies good() == false, but not vice versa. Thus, an error is
  diagnosed when the condition good() == false && eof() == false holds. Note
  that get(buf, len) == 0 && len > 0 implies good() == false.

- `class Input::streambbuf(const Input&)` creates a `std::istream` for the
  given `Input` object.

- Compile with `REFLEX_WITH_UTF8_UNRESTRICTED` to enable unrestricted UTF-8 beyond
  U+10FFFF, permitting lossless UTF-8 encoding of 32 bit words without limits.

Example
-------

The following example shows how to use the Input class to read a character
sequence in blocks from a `std::ifstream` to copy to stdout:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
    std::ifstream ifs;
    ifs.open("input.h", std::ifstream::in);
    reflex::Input input(ifs);
    char buf[1024];
    size_t len;
    while ((len = input.get(buf, sizeof(buf))) > 0)
      fwrite(buf, 1, len, stdout);
    if (!input.eof())
      std::cerr << "An IO error occurred" << std::endl;
    ifs.close();
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Example
-------

The following example shows how to use the Input class to store the entire
content of a file in a temporary buffer:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
    reflex::Input input(fopen("input.h", "r"));
    if (input.file() == nullptr)
      abort();
    size_t len = input.size(); // file size (minus any leading UTF BOM)
    char *buf = new char[len];
    input.get(buf, len);
    if (!input.eof())
      std::cerr << "An IO error occurred" << std::endl;
    fwrite(buf, 1, len, stdout);
    delete[] buf;
    fclose(input.file());
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

In the above, files with UTF-16 and UTF-32 content are converted to UTF-8 by
`get(buf, len)`.  Also, `size()` returns the total number of UTF-8 bytes to
copy in the buffer by `get(buf, len)`.  The size is computed depending on the
UTF-8/16/32 file content encoding, i.e. given a leading UTF BOM in the file.
This means that UTF-16/32 files are read twice, first internally with `size()`
and then again with get(buf, len)`.

Example
-------

The following example shows how to use the Input class to read a character
sequence in blocks from a file:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
    reflex::Input input(fopen("input.h", "r"));
    char buf[1024];
    size_t len;
    while ((len = input.get(buf, sizeof(buf))) > 0)
      fwrite(buf, 1, len, stdout);
    fclose(input);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Example
-------

The following example shows how to use the Input class to echo characters one
by one from stdin, e.g. reading input from a tty:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
    reflex::Input input(stdin);
    char c;
    while (input.get(&c, 1))
      fputc(c, stdout);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Or if you prefer to use an int character and check for EOF explicitly:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
    reflex::Input input(stdin);
    int c;
    while ((c = input.get()) != EOF)
      fputc(c, stdout);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Example
-------

The following example shows how to use the Input class to read a character
sequence in blocks from a wide character string, converting it to UTF-8 to copy
to stdout:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
    reflex::Input input(U"Copyright ©"); // © is unicode U+00A9 and UTF-8 C2 A9
    char buf[8];
    size_t len;
    while ((len = input.get(buf, sizeof(buf))) > 0)
      fwrite(buf, 1, len, stdout);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Example
-------

The following example shows how to use the Input class to convert a wide
character string to UTF-8:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
    reflex::Input input(U"Copyright ©"); // © is unicode U+00A9 and UTF-8 C2 A9
    size_t len = input.size(); // size of UTF-8 string
    char *buf = new char[len + 1];
    input.get(buf, len);
    buf[len] = '\0'; // make \0-terminated
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Example
-------

The following example shows how to switch source inputs while reading input
byte by byte (use a buffer as shown in other examples to improve efficiency):

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
    reflex::Input input = "Hello";
    std::string message;
    char c;
    while (input.get(&c, 1))
      message.append(c);
    input = U" world! To ∞ and beyond."; // switch input to a wide string
    while (input.get(&c, 1))
      message.append(c);
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Example
-------

The following examples shows how to use reflex::Input::streambuf to create an
unbuffered std::istream:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
    reflex::Input input(fopen("legacy.txt", "r"), reflex::Input::encoding::ebcdic);
    if (input.file() == nullptr)
      abort();
    reflex::Input::streambuf streambuf(input);
    std::istream stream(&streambuf);
    std::string data;
    int c;
    while ((c = stream.get()) != EOF)
      data.append(c);
    fclose(input.file());
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

With reflex::BufferedInput::streambuf to create a buffered std::istream:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
    reflex::Input input(fopen("legacy.txt", "r"), reflex::Input::encoding::ebcdic);
    if (input.file() == nullptr)
      abort();
    reflex::BufferedInput::streambuf streambuf(input);
    std::istream stream(&streambuf);
    std::string data;
    int c;
    while ((c = stream.get()) != EOF)
      data.append(c);
    fclose(input.file());
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
*/
class Input {
 public:
  /// Input type
  enum struct Source_Type : unsigned char {NIL,FILE_P,STD_ISTREAM_P,CCHAR_P,SV};
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
  /// FILE* handler functor base class to handle FILE* errors and non-blocking FILE* reads
  struct Handler { virtual int operator()() = 0; };
  static constexpr size_t get_raw_temp_default_size = 1024;
 private:
  char* allocate_get_raw_temp(size_t s) {return new char[s];}
  void free_get_raw_temp(char* p) {delete[] p;}
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
    get_raw_temp_(allocate_get_raw_temp(get_raw_temp_default_size)),
    get_raw_temp_size_(get_raw_temp_default_size)
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
      handler(other.handler),
      get_raw_temp_(other.get_raw_temp_),
      get_raw_temp_size_(other.get_raw_temp_size_)
  {
    other.set_source();
    other.get_raw_temp_=other.allocate_get_raw_temp(get_raw_temp_default_size);
    other.get_raw_temp_size_=get_raw_temp_default_size;
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
      get_raw_temp_(allocate_get_raw_temp(get_raw_temp_default_size)),
      get_raw_temp_size_(get_raw_temp_default_size)
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
  Input& operator=(const Input&) = delete;
  Input& operator=(Input&& other) noexcept
  {
    source_type_= other.source_type_;
    source_ = other.source_;
    enc_ = other.enc_;
    page_ = other.page_;
    handler = other.handler;
    get_raw_temp_=other.get_raw_temp_;
    get_raw_temp_size_=other.get_raw_temp_size_;
    other.set_source();
    other.get_raw_temp_=other.allocate_get_raw_temp(get_raw_temp_default_size);
    other.get_raw_temp_size_=get_raw_temp_default_size;
    return *this;
  }
  ~Input() noexcept {
    free_get_raw_temp(get_raw_temp_);
  }
  Source_Type get_source_type() const {return source_type_;}
  /// Get the `const char*` of this Input object.
  const char *c_str() const
    /// @returns current `const char*`
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
    /// @returns pointer to current file descriptor
  {
    assert(source_type_==Source_Type::FILE_P);
    return source_.file_;
  }
  /// Get the `std::istream` of this Input object.
  std::istream& istream() const
    /// @returns reference to current `std::istream`
  {
    assert(source_type_==Source_Type::STD_ISTREAM_P);
    return *(source_.istream_);
  }
  /// Get the remaining input size. Only meaningful for string view.
  size_t remaining_size() const {
    assert(source_type_==Source_Type::SV);
    return source_.sv_.size_;
  }
  /// Check if `get_raw()` can be performed.
  bool get_raw_able() const
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
      default :
        return false;
    }
  }
  explicit operator bool() const
    /// @returns `get_raw_able()`.
  {
    return get_raw_able();
  }
  /// Get unconverted bytes to `buf`. Has the same semantics as `fread`, except that a cast may be performed when the source type and `C` differ.
  template<typename C>
  size_t get_raw(C* buf,size_t size,size_t count);
  /// Get one unconverted byte. Has the same semantics as `fgetc`.
  int get_raw(){
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
      default: return EOF;
    }
  }
  /// Peek one unconverted byte.
  int peek_raw(){
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
      default:
        return EOF;
    }
  }
  /// Write to `s` one utf-8 converted code point (1-4 bytes).
  /// @returns utf-8 byte length.
  template<typename C>
  size_t get(C* s);
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
 protected:
  Source_Type source_type_;
 private:
  union Source_Union_{
    FILE                 *file_;    ///< FILE* input (when non-null)
    std::istream         *istream_; ///< stream input (when non-null)
    const char           *cstring_; ///< char string input (when non-null) of length reflex::Input::size_
    struct{const char* data_; size_t size_;} sv_;
    constexpr Source_Union_(FILE* file_other) noexcept : file_(file_other) {}
    constexpr Source_Union_(std::istream* istream_other) noexcept : istream_(istream_other) {}
    constexpr Source_Union_(const char* cstring_other) noexcept : cstring_(cstring_other) {}
    constexpr Source_Union_(const char* d,size_t s) noexcept : sv_{d,s} {}
    constexpr Source_Union_(std::nullptr_t = nullptr) noexcept : Source_Union_((FILE*)nullptr) {}
  } source_;
  encoding              enc_;        ///< encoding
  const codepage_unit_t* page_;      ///< custom code page

  unsigned char         utf8_buf_[4]; ///< Buffer for `to_utf8(char32_t,C* c)`
  unsigned char         get_temp_[4]; ///< Temporary storage for `get(C* c)`
  char                  *get_raw_temp_; ///< Temporary storage for `get_raw(C*,size_t,size_t)` in tha case of `std::istream` and `FILE*`.
  size_t                get_raw_temp_size_;
 public:
  Handler              *handler=nullptr; ///< to handle FILE* errors and non-blocking FILE* reads
};

inline void Input::detect_and_skip_bom(){
  // TODO
  enc_ = encoding::utf8;
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
          resize_get_raw_temp(size);
        for(size_t i=0;i<count;++i){
          if(fread(get_raw_temp_,size,1,source_.file_)==1){
            reflex::char_copy(buf,reinterpret_cast<unsigned char*>(get_raw_temp_),size); // `char` and `unsigned char` can alias.
            buf+=size;
          }else
            return i;
        }
        return count;
      }
    }
    case Source_Type::STD_ISTREAM_P:{
      if(get_raw_temp_size_<size)
        resize_get_raw_temp(size);
      for(size_t i = 0;i<count;++i){
        source_.istream_->read(get_raw_temp_,size);
        if(static_cast<size_t>(source_.istream_->gcount())!=size)
          return i;
        else{
          reflex::char_copy(buf,get_raw_temp_,size);
          buf+=size;
        }
      }
      return count;
    }
    default : return 0;
  }
}

template<typename C>
size_t Input::get(C* s){
  size_t l = 0;
  switch (enc_)
  {
    case encoding::utf8:{
      int c = peek_raw();
      if(c==EOF || c>=0xF8)
        l=0;
      else if(c<0x80)
        l=1,*s == static_cast<C>(get_raw());
      else if(c>=0xF0)
        if(get_raw(get_temp_, 4, 1) == 1)
          l=4,reflex::char_copy(s,get_temp_,4);
      else if(c>=0xE0)
        if(get_raw(get_temp_, 3, 1) == 1)
          l=3,reflex::char_copy(s,get_temp_,3);
      else if(c>=0xC0)
        if(get_raw(get_temp_, 2, 1) == 1)
          l=2,reflex::char_copy(s,get_temp_,2);
      break;
    }
    case encoding::utf16be:{
      if (get_raw(get_temp_, 2, 1) == 1)
      {
        char32_t c = get_temp_[0] << 8 | get_temp_[1];
        if (c < 0x80)
          l=1,*s = static_cast<C>(c);
        else
        {
          if (c >= 0xD800 && c < 0xE000)
          {
            // UTF-16 surrogate pair
            if (c < 0xDC00 && get_raw(get_temp_ + 2, 2, 1) == 1 && (get_temp_[2] & 0xFC) == 0xDC)
              c = 0x010000 - 0xDC00 + ((c - 0xD800) << 10) + (get_temp_[2] << 8 | get_temp_[3]);
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
      if (get_raw(get_temp_, 2, 1) == 1)
      {
        char32_t c = get_temp_[0] | get_temp_[1] << 8;
        if (c < 0x80)
          l=1,*s = static_cast<C>(c);
        else
        {
          if (c >= 0xD800 && c < 0xE000)
          {
            // UTF-16 surrogate pair
            if (c < 0xDC00 && get_raw(get_temp_ + 2, 2, 1) == 1 && (get_temp_[3] & 0xFC) == 0xDC)
              c = 0x010000 - 0xDC00 + ((c - 0xD800) << 10) + (get_temp_[2] | get_temp_[3] << 8);
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
      if (get_raw(get_temp_, 4, 1) == 1)
      {
        char32_t c = get_temp_[0] << 24 | get_temp_[1] << 16 | get_temp_[2] << 8 | get_temp_[3];
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
      if (get_raw(get_temp_, 4, 1) == 1)
      {
        char32_t c = get_temp_[0] | get_temp_[1] << 8 | get_temp_[2] << 16 | get_temp_[3] << 24;
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
}

/// Buffered input.
class BufferedInput : private Input {
 public:
  using Input::Source_Type;
  using Input::encoding;
  using Input::Handler;
  // `explicit operator bool() const` is overriden
  using Input::cstring;
  using Input::file;
  using Input::istream;
  // `clear()` is overriden
  // `good()` is overriden
  // `eof()` is overriden
  // `int get()` is overriden
  // `size_t get(char*,size_t)` is overriden
  using Input::set_encoding;
  using Input::get_encoding;
  /// Buffer size.
  static constexpr size_t SIZE = 16384;
  /// Copy constructor (with intended "move semantics" as internal state is shared, should not rely on using the rhs after copying).
  /// Construct empty buffered input.
  BufferedInput()
    :
      Input(),
      len_(0),
      pos_(0)
  { }
  /// Copy constructor.
  BufferedInput(const BufferedInput& input)
    :
      Input(input),
      len_(input.len_),
      pos_(input.pos_)
  {
    std::memcpy(buf_, input.buf_, len_);
  }
  /// Construct buffered input from unbuffered input.
  template<typename... Args>
  BufferedInput(Args&&... args)
    :
      Input(std::forward<Args>(args)...)
  {
    len_ = Input::get(buf_, SIZE);
    pos_ = 0;
  }
  /// Copy assignment operator.
  BufferedInput& operator=(const BufferedInput& input)
  {
    Input::operator=(input);
    len_ = input.len_;
    pos_ = input.pos_;
    std::memcpy(buf_, input.buf_, len_);
    return *this;
  }
  /// Assignment operator from unbuffered input.
  template<typename... Args>
  BufferedInput& operator=(Args&&... args)
  {
    Input::operator=(std::forward<Args>(args)...);
    len_ = Input::get(buf_, SIZE);
    pos_ = 0;
    return *this;
  }
  // Cast this Input object to bool, same as checking good().
  explicit operator bool() const
    /// @returns true if a non-empty sequence of characters is available to get
  {
    return good();
  }
  /// Get the size of the input character sequence in number of ASCII/UTF-8 bytes (zero if size is not determinable from a `FILE*` or `std::istream` source).
  size_t size()
    /// @returns the nonzero number of ASCII/UTF-8 bytes available to read, or zero when source is empty or if size is not determinable e.g. when reading from standard input
  {
    return len_ - pos_ + Input::size();
  }
  /// Clear this Input.
  void clear()
  {
    Input::clear();
    std::memset(buf_,0,SIZE);
    len_ = 0;
    pos_ = 0;
  }
  /// Check if input is available.
  bool good() const
    /// @returns true if a non-empty sequence of characters is available to get
  {
    return pos_ < len_ || Input::good();
  }
  /// Check if input reached EOF.
  bool eof() const
    /// @returns true if input is at EOF and no characters are available
  {
    return pos_ >= len_ && Input::eof();
  }
  /// Peek a single character (unsigned char 0..255) or EOF (-1) when end-of-input is reached.
  int peek()
  {
    while (true)
    {
      if (len_ == 0)
        return EOF;
      if (pos_ < len_)
        return static_cast<unsigned char>(buf_[pos_]);
      len_ = Input::get(buf_, SIZE);
      pos_ = 0;
    }
  }
  /// Get a single character (unsigned char 0..255) or EOF (-1) when end-of-input is reached.
  int get()
  {
    while (true)
    {
      if (len_ == 0)
        return EOF;
      if (pos_ < len_)
        return static_cast<unsigned char>(buf_[pos_++]);
      len_ = Input::get(buf_, SIZE);
      pos_ = 0;
    }
  }
  /// Copy character sequence data into buffer.
  size_t get(
      char  *s, ///< points to the string buffer to fill with input
      size_t n) ///< size of buffer pointed to by s
  {
    size_t k = n;
    while (k > 0)
    {
      if (pos_ < len_)
      {
        *s++ = buf_[pos_++];
        --k;
      }
      else if (len_ == 0)
      {
        break;
      }
      else
      {
        len_ = Input::get(buf_, SIZE);
        pos_ = 0;
      }
    }
    return n - k;
  }
 private:
  char   buf_[SIZE]; ///< Buffer
  size_t len_; ///< Length of data to be read, in the buffer. Can be smaller than `SIZE` when the source is smaller than the buffer.
  size_t pos_; ///< Current position in the buffer.
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
