/*
   mkvtoolnix - A set of programs for manipulating Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Cross platform compatibility definitions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __OS_H
#define __OS_H

#include "config.h"

// For PRId64 and PRIu64:
#define __STDC_FORMAT_MACROS

#if defined(_WIN32) || defined(WIN32) || defined(__CYGWIN__) || defined(__MINGW32__)
# define SYS_WINDOWS
# if defined __MINGW32__
#  define COMP_MINGW
# elif defined __CYGWIN__
#  define COMP_CYGWIN
# else
#  define COMP_MSC
# endif
#elif defined(__APPLE__)
# define SYS_APPLE
#else
# define COMP_GCC
# define SYS_UNIX
# if defined(__bsdi__) || defined(__FreeBSD__)
#  define SYS_BSD
# elif defined(__sun) && defined(__SVR4)
#  define SYS_SOLARIS
# else
#  define SYS_LINUX
# endif
#endif

#if defined(COMP_MSC)
# define strncasecmp _strnicmp
# define strcasecmp _stricmp
# define nice(a)
# define vsnprintf _vsnprintf
typedef __int64 int64_t;
typedef __int32 int32_t;
typedef __int16 int16_t;
typedef __int8  int8_t;
typedef unsigned __int64 uint64_t;
typedef unsigned __int32 uint32_t;
typedef unsigned __int16 uint16_t;
typedef unsigned __int8 uint8_t;
#include <io.h>
typedef _fsize_t ssize_t;

#define PACKED_STRUCTURE
#else
#define PACKED_STRUCTURE   __attribute__((__packed__))
#endif // COMP_MSC

#if defined(HAVE_SYS_TYPES_H)
# include <sys/types.h>
#endif // HAVE_SYS_TYPES_H
#if defined(HAVE_STDINT_H)
# include <stdint.h>
#endif // HAVE_STDINT_H
#if defined(HAVE_INTTYPES_H)
# include <inttypes.h>
#endif // HAVE_INTTYPES_H

#if defined(COMP_MINGW) || defined(COMP_MSC)

// For DLL stuff...
# if defined(MTX_DLL)
#  if defined(MTX_DLL_EXPORT)
#   define MTX_DLL_API __declspec(dllexport)
#  else // MTX_DLL_EXPORT
#   define MTX_DLL_API __declspec(dllimport)
#  endif
# else // MTX_DLL
#  define MTX_DLL_API
# endif

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

#else  // COMP_MINGW || COMP_MSC

# define MTX_DLL_API

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

// MSVC doesn't provide vsscanf. So let's use our own frontend.
#if !defined(HAVE_VSSCANF) || (HAVE_VSSCANF != 1)
# include <stdio.h>
int vsscanf(const char *, const char *, va_list);
#endif // !HAVE_VSSCANF...

int MTX_DLL_API fs_entry_exists(const char *path);
void MTX_DLL_API create_directory(const char *path);
int64_t MTX_DLL_API get_current_time_millis();

#endif
