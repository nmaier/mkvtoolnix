/*
 * mkvtoolnix - A set of programs for manipulating Matroska files
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * Cross platform compatibility definitions
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#ifndef __OS_H
#define __OS_H

#include "config.h"

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

# define LLD "%I64d"
# define LLU "%I64u"

#else  // COMP_MINGW || COMP_MSC

# define MTX_DLL_API
# define LLD "%lld"
# define LLU "%llu"
#endif // COMP_MINGW || COMP_MSC

#if defined(HAVE_STDINT_H)
#include <stdint.h>
#endif // HAVE_STDINT_H
#if defined(HAVE_INTTYPES_H)
#include <inttypes.h>
#endif // HAVE_INTTYPES_H

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
