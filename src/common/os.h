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
# define PACKAGE "mkvtoolnix"
# define VERSION "0.8.0"

# define strncasecmp _strnicmp
# define strcasecmp _stricmp
# define nice(a)
# define vsnprintf _vsnprintf
# define vfprintf _vfprintf
#endif // COMP_MSC

#if defined(COMP_MINGW) || defined(COMP_MSC)
# define LLD "%I64d"
# define LLU "%I64u"
#else
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
