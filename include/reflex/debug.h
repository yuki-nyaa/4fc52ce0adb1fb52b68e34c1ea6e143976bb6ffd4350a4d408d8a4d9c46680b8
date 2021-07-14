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
@file      debug.h
@brief     RE/flex debug logs and assertions
@author    Robert van Engelen - engelen@genivia.com
@copyright (c) 2016-2020, Robert van Engelen, Genivia Inc. All rights reserved.
@copyright (c) BSD-3 License - see LICENSE.txt

Exploiting macro magic to simplify debug logging.

Usage
-----

Enable macro REFLEX_DEBUG to debug the compiled source code:

| Source files compiled with	| REFLEX_DBGLOG(...) entry added to	|
| ----------------------------- | ----------------------------- |
| `c++ -DREFLEX_DEBUG`			| `REFLEX_DEBUG.log`			|
| `c++ -DREFLEX_DEBUG=TEST`		| `TEST.log`			|
| `c++ -DREFLEX_DEBUG= `		| `stderr`			|

`REFLEX_DBGLOG(format, ...)` creates a timestamped log entry with a printf-formatted
message. The log entry is added to a log file or sent to `stderr` as specified:

`REFLEX_DBGLOGN(format, ...)` creates a log entry without a timestamp.

`REFLEX_DBGLOGA(format, ...)` appends the formatted string to the previous log entry.

`REFLEX_DBGCHK(condition)` calls `assert(condition)` when compiled in REFLEX_DEBUG mode.

The utility macro `REFLEX_DBGSTR(const char *s)` returns string `s` or `"(null)"` when
`s == nullptr`.

@note to temporarily enable debugging a specific block of code without globally
debugging all code, use a leading underscore, e.g. `_DBGLOG(format, ...)`.
This appends the debugging information to `REFLEX_DEBUG.log`.

@warning Be careful to revert these statements by removing the leading
underscore for production-quality code.

Example
-------

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.cpp}
    #include <reflex/debug.h>

    int main(int argc, char *argv[])
    {
      FILE *fd;
      REFLEX_DBGLOG("Program start");
      if ((fd = fopen("foo.bar", "r")) == nullptr)
      {
        REFLEX_DBGLOG("Error %d: %s ", errno, REFLEX_DBGSTR(strerror(errno)));
        for (int i = 1; i < argc; ++i)
          REFLEX_DBGLOGA(" %s", argv[1]);
      }
      else
      {
        REFLEX_DBGCHK(fd != nullptr);
        // OK, so go ahead to read foo.bar ...
        // ...
        fclose(fd);
      }
      REFLEX_DBGLOG("Program end");
    }
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

Compiled with `-DDEBUG` this example logs the following messages in `REFLEX_DEBUG.log`:

~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~{.txt}
    140201/225654.692194   example.cpp:11   Program has started
    140201/225654.692564   example.cpp:15   Error 2: No such file or directory
    140201/225654.692577   example.cpp:17   Program ended
~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~

The first column records the date (140201 is February 1, 2014) and the time
(225654 is 10:56PM + 54 seconds) with microsecond fraction. The second column
records the source code file name and the line number of the `REFLEX_DBGLOG` command.
The third column shows the printf-formatted message.

The `REFLEX_DEBUG.log` file is created in the current directory when it does not
already exist.

Techniques used:

- Variadic macros with `__VA_ARGS__`.
- Standard predefined macros `__FILE__` and `__LINE__`.
- Macro "stringification": expand content of macro `REFLEX_DEBUG` as a string in a
  macro body.
- `#if REFLEX_DEBUG + 0` to test whether macro `REFLEX_DEBUG` is set to a value, since
  `REFLEX_DEBUG` is 1 when set without a value (for example at the command line).
- `"" __VA_ARGS__` forces `__VA_ARGS__` to start with a literal format string
  (printf security advisory).
*/

#ifndef REFLEX_DEBUG_H
#define REFLEX_DEBUG_H

#include <cassert>
#include <cstdio>

extern FILE *REFLEX_DBGFD_;

extern "C" void REFLEX_DBGOUT_(const char *log, const char *file, int line);

#define REFLEX_DBGXIFY(S) REFLEX_DBGIFY_(S)
#define REFLEX_DBGIFY_(S) #S
#if REFLEX_DEBUG + 0
# define REFLEX_DBGFILE "REFLEX_DEBUG.log"
#else
# define REFLEX_DBGFILE REFLEX_DBGXIFY(REFLEX_DEBUG) ".log"
#endif
#define REFLEX_DBGSTR(S) (S?S:"(nullptr)")
#define REFLEX_DBGLOG_IMPL_(...) \
( REFLEX_DBGOUT_(REFLEX_DBGFILE, __FILE__, __LINE__), ::fprintf(REFLEX_DBGFD_, "" __VA_ARGS__), ::fflush(REFLEX_DBGFD_))
#define REFLEX_DBGLOGN_IMPL_(...) \
( ::fprintf(REFLEX_DBGFD_, "\n                                        " __VA_ARGS__), ::fflush(REFLEX_DBGFD_) )
#define REFLEX_DBGLOGA_IMPL_(...) \
( ::fprintf(REFLEX_DBGFD_, "" __VA_ARGS__), ::fflush(REFLEX_DBGFD_) )

#ifdef REFLEX_DEBUG

#define REFLEX_DBGCHK(c) assert(c)

#define REFLEX_DBGLOG REFLEX_DBGLOG_IMPL_
#define REFLEX_DBGLOGN REFLEX_DBGLOGN_IMPL_
#define REFLEX_DBGLOGA REFLEX_DBGLOGA_IMPL_

#else

#define REFLEX_DBGCHK(c) (void)0

#define REFLEX_DBGLOG(...) (void)0
#define REFLEX_DBGLOGN(...) (void)0
#define REFLEX_DBGLOGA(...) (void)0

#endif

#endif
