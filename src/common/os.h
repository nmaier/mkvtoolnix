/*
   mkvtoolnix - A set of programs for manipulating Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Cross platform compatibility definitions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_OS_H
#define MTX_COMMON_OS_H

#undef __STRICT_ANSI__

#include "config.h"

// For PRId64 and PRIu64:
#define __STDC_FORMAT_MACROS

#if defined(HAVE_COREC_H)
#include "corec.h"

#if defined(TARGET_WIN)
# define SYS_WINDOWS
#elif defined(TARGET_OSX)
# define SYS_APPLE
#elif defined(TARGET_LINUX)
# define SYS_UNIX
# if defined(__sun) && defined(__SVR4)
#  define SYS_SOLARIS
# else
#  define SYS_LINUX
# endif
#endif

#else // HAVE_COREC_H
#if defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
# define SYS_WINDOWS
#elif defined(__APPLE__)
# define SYS_APPLE
#else
# define COMP_GCC
# define SYS_UNIX
# if defined(__bsdi__) || defined(__FreeBSD__)
#  define SYS_BSD
# elif defined(__sun) && defined(__SUNPRO_CC)
#  undef COMP_GCC
#  define COMP_SUNPRO
#  define SYS_SOLARIS
# elif defined(__sun) && defined(__SVR4)
#  define SYS_SOLARIS
# else
#  define SYS_LINUX
# endif
#endif

#endif // HAVE_COREC_H

#if defined(SYS_WINDOWS)
# if defined __MINGW32__
#  define COMP_MINGW
# elif defined __CYGWIN__
#  define COMP_CYGWIN
# else
#  define COMP_MSC
#  define NOMINMAX
# endif
#endif

#if (defined(SYS_WINDOWS) && defined(_WIN64)) || (!defined(SYS_WINDOWS) && (defined(__x86_64__) || defined(__ppc64__)))
# define ARCH_64BIT
#else
# define ARCH_32BIT
#endif

#if defined(COMP_MSC)

#if !defined(HAVE_COREC_H)
# define strncasecmp _strnicmp
# define strcasecmp _stricmp
typedef __int64 int64_t;
typedef __int32 int32_t;
typedef __int16 int16_t;
typedef __int8  int8_t;
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8 uint8_t;
#endif // HAVE_COREC_H

# define nice(a)
#include <io.h>
typedef _fsize_t ssize_t;

#define PACKED_STRUCTURE
#else // COMP_MSC
#define PACKED_STRUCTURE   __attribute__((__packed__))
#endif // COMP_MSC

#if defined(COMP_MINGW) || defined(COMP_MSC)

# if defined(COMP_MINGW)
// mingw is a special case. It does have inttypes.h declaring
// PRId64 to be I64d, but it warns about "int type format, different
// type argument" if %I64d is used with a int64_t. The mx* functions
// convert %lld to %I64d on mingw/msvc anyway.
#  undef PRId64
#  define PRId64 "lld"
#  undef PRIu64
#  define PRIu64 "llu"
#  undef PRIx64
#  define PRIx64 "llx"
# else
// MSVC doesn't have inttypes, nor the PRI?64 defines.
#  define PRId64 "I64d"
#  define PRIu64 "I64u"
#  define PRIx64 "I64x"
# endif // defined(COMP_MINGW)

#endif // COMP_MINGW || COMP_MSC

#define LLD "%" PRId64
#define LLU "%" PRIu64

#if defined(HAVE_NO_INT64_T)
typedef INT64_TYPE int64_t;
#endif
#if defined(HAVE_NO_UINT64_T)
typedef UINT64_TYPE uint64_t;
#endif

#if defined(SYS_WINDOWS)
# define PATHSEP '\\'
#else
# define PATHSEP '/'
#endif

#if defined(WORDS_BIGENDIAN) && (WORDS_BIGENDIAN == 1)
# define ARCH_BIGENDIAN
#else
# define ARCH_LITTLEENDIAN
#endif

#endif
