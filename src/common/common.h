/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions used in all programs, helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_H
#define __MTX_COMMON_H

#include "common/os.h"

// Compilation with mingw gcc fails with Boost's format library if
// "min" is defined. The libebml headers define "min", so make sure
// that it isn't before including boost/format.hpp.

#include <ebml/EbmlElement.h>
#include <ebml/EbmlMaster.h>
#undef min

#include <boost/foreach.hpp>
#include <boost/format.hpp>
#include <string>

/* i18n stuff */
#if defined(HAVE_LIBINTL_H)
# include <libintl.h>
# if !defined Y
#  define Y(s) gettext(s)
# endif
#else /* HAVE_LIBINTL_H */
# if !defined Y
#  define Y(s) (s)
# endif
#endif

#include "common/debugging.h"
#include "common/output.h"
#include "common/smart_pointers.h"

namespace libebml {
  class EbmlBinary;
};

using namespace libebml;

#define VERSIONNAME "C'est le bon"
#define VERSIONINFO "mkvmerge v" VERSION " ('" VERSIONNAME "')"
#define BUGMSG Y("This should not have happened. Please contact the author " \
                 "Moritz Bunkus <moritz@bunkus.org> with this error/warning " \
                 "message, a description of what you were trying to do, " \
                 "the command line used and which operating system you are " \
                 "using. Thank you.")


#define MXMSG_ERROR    5
#define MXMSG_WARNING 10
#define MXMSG_INFO    15
#define MXMSG_DEBUG   20

#define FOURCC(a, b, c, d) (uint32_t)((((unsigned char)a) << 24) + \
                                      (((unsigned char)b) << 16) + \
                                      (((unsigned char)c) <<  8) + \
                                       ((unsigned char)d))
#define isblanktab(c) (((c) == ' ')  || ((c) == '\t'))
#define iscr(c)       (((c) == '\n') || ((c) == '\r'))

#define TIMECODE_SCALE 1000000

void MTX_DLL_API mxexit(int code = -1);
void MTX_DLL_API set_process_priority(int priority);

extern int MTX_DLL_API verbose;

#define foreach                  BOOST_FOREACH
#define reverse_foreach          BOOST_REVERSE_FOREACH
#define mxforeach(it, vec)       for (it = (vec).begin(); it != (vec).end(); it++)
#define mxfind(value, cont)      std::find(cont.begin(), cont.end(), value)
#define mxfind2(it, value, cont) ((it = std::find((cont).begin(), (cont).end(), value)) != (cont).end())
#define map_has_key(m, k)        ((m).end() != (m).find(k))

void MTX_DLL_API mtx_common_init();

#endif // __MTX_COMMON_H
