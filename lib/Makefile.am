AUTOMAKE_OPTIONS = subdir-objects
reflexincludedir        = $(includedir)/reflex

reflexinclude_HEADERS   = $(top_srcdir)/include/reflex/lexer.h $(top_srcdir)/include/reflex/abslexer.hpp $(top_srcdir)/include/reflex/boostmatcher.h $(top_srcdir)/include/reflex/convert.h $(top_srcdir)/include/reflex/debug.h $(top_srcdir)/include/reflex/error.hpp $(top_srcdir)/include/reflex/flexlexer.h $(top_srcdir)/include/reflex/input.h $(top_srcdir)/include/reflex/pattern.hpp $(top_srcdir)/include/reflex/posix.h $(top_srcdir)/include/reflex/ranges.hpp $(top_srcdir)/include/reflex/setop.hpp $(top_srcdir)/include/reflex/stdmatcher.h $(top_srcdir)/include/reflex/timer.hpp $(top_srcdir)/include/reflex/unicode.h $(top_srcdir)/include/reflex/utf8.hpp $(top_srcdir)/include/reflex/fsm_gen.h

lib_LIBRARIES           = libreflex.a libreflexmin.a

libreflex_a_CPPFLAGS    = -I$(top_srcdir)/include $(SIMD_FLAGS)
libreflex_a_SOURCES     = convert.cpp debug.cpp input.cpp fsm_gen.cpp posix.cpp unicode.cpp $(top_srcdir)/unicode/block_scripts.cpp $(top_srcdir)/unicode/language_scripts.cpp $(top_srcdir)/unicode/letter_scripts.cpp

libreflexmin_a_CPPFLAGS = -I$(top_srcdir)/include $(SIMD_FLAGS)
libreflexmin_a_SOURCES  = debug.cpp input.cpp fsm_gen.cpp

# removed to avoid Max OS X libtool issues, alas...
# lib_LTLIBRARIES       = libreflex.la libreflexmin.a
#
# libreflex_la_CPPFLAGS = -I$(top_srcdir)/include
# libreflex_la_SOURCES  = convert.cpp debug.cpp error.cpp input.cpp lexer.cpp pattern.cpp posix.cpp unicode.cpp $(top_srcdir)/unicode/block_scripts.cpp $(top_srcdir)/unicode/language_scripts.cpp $(top_srcdir)/unicode/letter_scripts.cpp
#
# libreflexmin_la_CPPFLAGS = -I$(top_srcdir)/include
# libreflexmin_la_SOURCES  = debug.cpp error.cpp input.cpp lexer.cpp pattern.cpp
