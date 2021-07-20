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
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <cwchar>
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

extern const unsigned short codepages[][256];

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
    reflex::Input input(fopen("legacy.txt", "r"), reflex::Input::file_encoding::ebcdic);
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
    reflex::Input input(fopen("legacy.txt", "r"), reflex::Input::file_encoding::ebcdic);
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
  enum struct Source_Type : unsigned char {NIL,FILE_P,STD_ISTREAM_P,CCHAR_P};
  /// Common file_encoding constants.
  enum struct file_encoding : unsigned char  {
    plain, ///< plain octets: 7-bit ASCII, 8-bit binary or UTF-8 without BOM detected
    utf8, ///< UTF-8 with BOM detected
    utf16be, ///< UTF-16 big endian
    utf16le, ///< UTF-16 little endian
    utf32be, ///< UTF-32 big endian
    utf32le, ///< UTF-32 little endian
    latin, ///< ISO-8859-1, Latin-1
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
    custom, ///< custom code page
  };
  /// FILE* handler functor base class to handle FILE* errors and non-blocking FILE* reads
  struct Handler { virtual int operator()() = 0; };
  /// Construct empty input character sequence.
  Input()
  {
    init();
  }
  /// Copy constructor. Since the source input is read-only, sharing the source should not be a problem.
  Input(const Input& input) ///< an Input object to share state with
    :
      source_type_(input.source_type_),
      source_(input.source_),
      size_(input.size_),
      uidx_(input.uidx_),
      ulen_(input.ulen_),
      utfx_(input.utfx_),
      page_(input.page_),
      handler(input.handler)
  {
    std::memcpy(utf8_, input.utf8_, sizeof(utf8_));
  }
  /// Construct input character sequence from a char* string
  Input(
      const char *cstring, ///< char string
      size_t      size)    ///< length of the string
    :
      source_type_(Source_Type::CCHAR_P),
      source_(cstring),
      size_(size)
  {
    init();
  }
  /// Construct input character sequence from a NUL-terminated string.
  Input(const char *cstring) ///< NUL-terminated char* string
    :
    Input(cstring,cstring != nullptr ? std::strlen(cstring) : 0)
  {
    init();
  }
  /// Construct input character sequence from a std::string.
  Input(const std::string& string) ///< input string
    :
      Input(string.c_str(),string.size())
  {
    init();
  }
  /// Construct input character sequence from an open FILE* file descriptor, supports UTF-8 conversion from UTF-16 and UTF-32.
  Input(FILE *file) ///< input file
    :
      source_type_(Source_Type::FILE_P),
      source_(file),
      size_(0)
  {
    init();
  }
  /// Construct input character sequence from an open FILE* file descriptor, using the specified file encoding
  Input(
      FILE                 *file,        ///< input file
      file_encoding         enc,         ///< file_encoding (when UTF BOM is not present)
      const unsigned short *page = nullptr) ///< code page for file_encoding::custom
    :
      Input(file)
  {
    init(enc);
    if (get_file_encoding() == file_encoding::plain)
      set_file_encoding(enc, page);
  }
  /// Construct input character sequence from a std::istream.
  Input(std::istream& istream) ///< input stream
    :
      source_type_(Source_Type::STD_ISTREAM_P),
      source_(&istream),
      size_(0)
  {
    init();
  }
  /// Copy assignment operator.
  Input& operator=(const Input& input)
  {
    source_type_=input.source_type_;
    source_=input.source_;
    size_ = input.size_;
    uidx_ = input.uidx_;
    ulen_ = input.ulen_;
    utfx_ = input.utfx_;
    page_ = input.page_;
    handler = input.handler;
    std::memcpy(utf8_, input.utf8_, sizeof(utf8_));
    return *this;
  }
  // Cast this Input object to bool, same as checking good().
  explicit operator bool() const
    /// @returns true if a non-empty sequence of characters is available to get
  {
    return good();
  }
  /// Get the remaining string of this Input object, returns nullptr when this Input is not a string.
  const char *cstring() const
    /// @returns remaining unbuffered part of the NUL-terminated string or nullptr
  {
    assert(source_type_==Source_Type::CCHAR_P);
    return source_.cstring_;
  }
  /// Get the FILE* of this Input object, returns nullptr when this Input is not a FILE*.
  FILE *file() const
    /// @returns pointer to current file descriptor or nullptr
  {
    assert(source_type_==Source_Type::FILE_P);
    return source_.file_;
  }
  /// Get the std::istream of this Input object, returns nullptr when this Input is not a std::istream.
  std::istream *istream() const
    /// @returns pointer to current std::istream or nullptr
  {
    assert(source_type_==Source_Type::STD_ISTREAM_P);
    return source_.istream_;
  }
  /// Get the size of the input character sequence in number of ASCII/UTF-8 bytes (zero if size is not determinable from a `FILE*` or `std::istream` source).
  size_t size()
    /// @returns the nonzero number of ASCII/UTF-8 bytes available to read, or zero when source is empty or if size is not determinable e.g. when reading from standard input
  {
    switch(source_type_){
      case Source_Type::CCHAR_P :
        return size_;
      case Source_Type::FILE_P :
        if (size_ == 0)
          file_size();
        return size_;
      case Source_Type::STD_ISTREAM_P :
        if (size_ == 0)
          istream_size();
        return size_;
      default :
        return size_;
    }
  }
  /// Check if this Input object was assigned a character sequence.
  bool assigned() const
    /// @returns true if this Input object was assigned (not default constructed or cleared)
  {
    return source_type_!=Source_Type::NIL;
  }
  /// Clear this Input.
  void clear()
  {
    source_type_=Source_Type::NIL;
    size_ = 0;
    init();
  }
  /// Check if input is available.
  bool good() const
    /// @returns true if a non-empty sequence of characters is available to get
  {
    switch(source_type_){
      case Source_Type::CCHAR_P :
        return size_ > 0;
      case Source_Type::FILE_P :
        return !::feof(source_.file_) && !::ferror(source_.file_);
      case Source_Type::STD_ISTREAM_P :
        return (source_.istream_)->good();
      default :
        return false;
    }
  }
  /// Check if input reached EOF.
  bool eof() const
    /// @returns true if input is at EOF and no characters are available
  {
    switch(source_type_){
      case Source_Type::CCHAR_P :
        return size_ == 0;
      case Source_Type::FILE_P :
        return ::feof(source_.file_) != 0;
      case Source_Type::STD_ISTREAM_P :
        return (source_.istream_)->eof();
      default :
        return true;
    }
  }
  /// Get a single character (unsigned char 0..255) or EOF (-1) when end-of-input is reached.
  int get()
  {
    char c;
    if (get(&c, 1))
      return static_cast<unsigned char>(c);
    return EOF;
  }
  /// Copy character sequence data into buffer.
  size_t get(
      char  *s, ///< points to the string buffer to fill with input
      size_t n) ///< size of buffer pointed to by s
    /// @returns the nonzero number of (less or equal to n) 8-bit characters added to buffer s from the current input, or zero when EOF
  {
    switch(source_type_){
      case Source_Type::CCHAR_P : {
        size_t k = size_;
        if (k > n)
          k = n;
        std::memcpy(s, source_.cstring_, k);
        source_.cstring_ += k;
        size_ -= k;
        return k;
      }
      case Source_Type::FILE_P : {
        while (true)
        {
          size_t k = file_get(s, n);
          if (k > 0 || feof(source_.file_) || handler == nullptr || (*handler)() == 0)
            return k;
        }
      }
      case Source_Type::STD_ISTREAM_P : {
        size_t k = static_cast<size_t>(n == 1 ? source_.istream_->get(s[0]).gcount() : source_.istream_->read(s, static_cast<std::streamsize>(n)) ? n : source_.istream_->gcount());
        if (size_ >= k)
          size_ -= k;
        return k;
      }
      default :
        return 0;
    }
  }
  /// Set encoding for `FILE*` input.
  void set_file_encoding(
      file_encoding    enc,         ///< file_encoding
      const unsigned short *page = nullptr) ///< custom code page for file_encoding::custom
    ;
  /// Get encoding of the current `FILE*` input.
  file_encoding get_file_encoding() const
    /// @returns current file_encoding constant
  {
    return utfx_;
  }
 private:
 /// Initialize the state after (re)setting the input source, auto-detects UTF BOM in FILE* input if the file size is known.
  void init(file_encoding enc = file_encoding::plain)
  {
    std::memset(utf8_, 0, sizeof(utf8_));
    uidx_ = 0;
    ulen_ = 0;
    page_ = nullptr;
    if (source_type_==Source_Type::FILE_P)
      file_init(enc);
  }
  /// Called by init() for a FILE*.
  void file_init(file_encoding enc);
  /// Called by size() for a FILE*.
  void file_size();
  /// Called by size() for a std::istream.
  void istream_size();
  /// Implements get() on a FILE*.
  size_t file_get(
      char  *s, ///< points to the string buffer to fill with input
      size_t n) ///< size of buffer pointed to by s
      ;
  Source_Type source_type_ = Source_Type::NIL;
  union Source_Union_{
    FILE                 *file_=nullptr;    ///< FILE* input (when non-null)
    std::istream         *istream_; ///< stream input (when non-null)
    const char           *cstring_; ///< char string input (when non-null) of length reflex::Input::size_
    constexpr Source_Union_() noexcept = default;
    constexpr Source_Union_(FILE* file_other) noexcept : file_(file_other) {}
    constexpr Source_Union_(std::istream* istream_other) noexcept : istream_(istream_other) {}
    constexpr Source_Union_(const char* cstring_other) noexcept : cstring_(cstring_other) {}
  } source_;
  size_t                size_=0;    ///< size of the remaining input in bytes (size_ == 0 may indicate size is not set)
  char                  utf8_[8]={}; ///< UTF-8 normalization buffer, >=8 bytes
  unsigned short        uidx_=0;    ///< index in utf8_[]
  unsigned short        ulen_=0;    ///< length of data in utf8_[] or 0 if no data
  file_encoding    utfx_=file_encoding::plain;    ///< file_encoding
  const unsigned short *page_=nullptr;    ///< custom code page
 public:
  Handler              *handler=nullptr; ///< to handle FILE* errors and non-blocking FILE* reads
};

/// Buffered input.
class BufferedInput : private Input {
 public:
  using Input::Source_Type;
  using Input::file_encoding;
  using Input::Handler;
  // `explicit operator bool() const` is overriden
  using Input::cstring;
  using Input::file;
  using Input::istream;
  // `size()` is overriden.
  using Input::assigned;
  // `clear()` is overriden
  // `good()` is overriden
  // `eof()` is overriden
  // `int get()` is overriden
  // `size_t get(char*,size_t)` is overriden
  using Input::set_file_encoding;
  using Input::get_file_encoding;
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
