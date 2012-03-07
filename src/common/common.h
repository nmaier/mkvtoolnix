/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions used in all programs, helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_COMMON_H
#define __MTX_COMMON_COMMON_H

#undef min
#undef max
#undef __STRICT_ANSI__

#include <algorithm>
#include <stdexcept>
#include <string>
#include <vector>

#include <cassert>
#include <cstring>

#if defined(HAVE_SYS_TYPES_H)
# include <sys/types.h>
#endif // HAVE_SYS_TYPES_H
#if defined(HAVE_STDINT_H)
# include <stdint.h>
#endif // HAVE_STDINT_H
#if defined(HAVE_INTTYPES_H)
# include <inttypes.h>
#endif // HAVE_INTTYPES_H

#include <boost/algorithm/string.hpp>
#include <boost/bind.hpp>
#include <boost/filesystem.hpp>
#include <boost/format.hpp>
#include <boost/function.hpp>
#include <boost/logic/tribool.hpp>
#include <boost/math/common_factor.hpp>
#include <boost/range.hpp>
#include <boost/range/algorithm_ext.hpp>
#include <boost/range/algorithm.hpp>
#include <boost/range/numeric.hpp>
#include <boost/rational.hpp>
#include <boost/regex.hpp>
#include <boost/system/error_code.hpp>

namespace ba  = boost::algorithm;
namespace bfs = boost::filesystem;
namespace br  = boost::range;

#include "common/os.h"

#include <ebml/c/libebml_t.h>
#undef min
#undef max

#include <ebml/EbmlElement.h>
#include <ebml/EbmlMaster.h>

/* i18n stuff */
#if defined(HAVE_LIBINTL_H)
# include <libintl.h>
# if !defined Y
#  define Y(s) gettext(s)
#  define NY(s_singular, s_plural, count) ngettext(s_singular, s_plural, count)
# endif
#else /* HAVE_LIBINTL_H */
# if !defined Y
#  define Y(s) (s)
#  define NY(s_singular, s_plural, count) (s_singular)
# endif
#endif

#include "common/smart_pointers.h"
#include "common/debugging.h"
#include "common/output.h"

namespace libebml {
  class EbmlBinary;
};

using namespace libebml;

#define BUGMSG Y("This should not have happened. Please contact the author " \
                 "Moritz Bunkus <moritz@bunkus.org> with this error/warning " \
                 "message, a description of what you were trying to do, " \
                 "the command line used and which operating system you are " \
                 "using. Thank you.")


#define MXMSG_ERROR    5
#define MXMSG_WARNING 10
#define MXMSG_INFO    15
#define MXMSG_DEBUG   20

#if !defined(FOURCC)
#define FOURCC(a, b, c, d) (uint32_t)((((unsigned char)a) << 24) + \
                                      (((unsigned char)b) << 16) + \
                                      (((unsigned char)c) <<  8) + \
                                       ((unsigned char)d))
#endif
#define isblanktab(c) (((c) == ' ')  || ((c) == '\t'))
#define iscr(c)       (((c) == '\n') || ((c) == '\r'))

#define TIMECODE_SCALE 1000000

void mxexit(int code = -1);
void set_process_priority(int priority);

extern unsigned int verbose;

#define mxforeach(it, vec)       for (it = (vec).begin(); it != (vec).end(); it++)
#define mxfind(value, cont)      std::find(cont.begin(), cont.end(), value)
#define mxfind2(it, value, cont) ((it = std::find((cont).begin(), (cont).end(), value)) != (cont).end())
#define map_has_key(m, k)        ((m).end() != (m).find(k))

void mtx_common_init();

#endif // __MTX_COMMON_COMMON_H
