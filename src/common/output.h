/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions used in all programs, helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_OUTPUT_H
#define __MTX_COMMON_OUTPUT_H

#include "common/os.h"

#include <boost/format.hpp>
#include <string>

#include <ebml/EbmlElement.h>

#include "common/locale.h"

using namespace libebml;
class mm_io_c;

extern bool MTX_DLL_API g_suppress_info, g_suppress_warnings;
extern std::string MTX_DLL_API g_stdio_charset;
extern charset_converter_cptr MTX_DLL_API g_cc_stdio;
extern counted_ptr<mm_io_c> MTX_DLL_API g_mm_stdio;

void MTX_DLL_API redirect_stdio(mm_io_c *new_stdio);
bool MTX_DLL_API stdio_redirected();

void MTX_DLL_API init_cc_stdio();
void MTX_DLL_API set_cc_stdio(const std::string &charset);

void MTX_DLL_API mxmsg(unsigned int level, std::string message);
inline void
mxmsg(unsigned int level,
      const boost::format &message) {
  mxmsg(level, message.str());
}

void MTX_DLL_API mxinfo(const std::string &info);
inline void mxinfo(const boost::format &info) {
  mxinfo(info.str());
}
void MTX_DLL_API mxinfo(const std::wstring &info);
void MTX_DLL_API mxinfo(const boost::wformat &info);

void MTX_DLL_API mxwarn(const std::string &warning);
inline void
mxwarn(const boost::format &warning) {
  mxwarn(warning.str());
}

void MTX_DLL_API mxerror(const std::string &error);
inline void
mxerror(const boost::format &error) {
  mxerror(error.str());
}

#define mxverb(level, message)        \
  if (verbose >= level)               \
    mxmsg(MXMSG_INFO, message);

void MTX_DLL_API mxinfo_fn(const std::string &file_name, const std::string &info);
inline void
mxinfo_fn(const std::string &file_name,
          const boost::format &info) {
  mxinfo_fn(file_name, info.str());
}

void MTX_DLL_API mxinfo_tid(const std::string &file_name, int64_t track_id, const std::string &info);
inline void
mxinfo_tid(const std::string &file_name,
           int64_t track_id,
           const boost::format &info) {
  mxinfo_tid(file_name, track_id, info.str());
}

void MTX_DLL_API mxwarn_fn(const std::string &file_name, const std::string &info);
inline void
mxwarn_fn(const std::string &file_name,
          const boost::format &info) {
  mxwarn_fn(file_name, info.str());
}

void MTX_DLL_API mxwarn_tid(const std::string &file_name, int64_t track_id, const std::string &warning);
inline void
mxwarn_tid(const std::string &file_name,
           int64_t track_id,
           const boost::format &warning) {
  mxwarn_tid(file_name, track_id, warning.str());
}

void MTX_DLL_API mxerror_fn(const std::string &file_name, const std::string &error);
inline void
mxerror_fn(const std::string &file_name,
           const boost::format &error) {
  mxerror_fn(file_name, error.str());
}

void MTX_DLL_API mxerror_tid(const std::string &file_name, int64_t track_id, const std::string &error);
inline void
mxerror_tid(const std::string &file_name,
            int64_t track_id,
            const boost::format &error) {
  mxerror_tid(file_name, track_id, error.str());
}

void MTX_DLL_API mxverb_fn(unsigned int level, const std::string &file_name, const std::string &message);
inline void
mxverb_fn(unsigned int level,
          const std::string &file_name,
          const boost::format &message) {
  mxverb_fn(level, file_name, message.str());
}

void MTX_DLL_API mxverb_tid(unsigned int level, const std::string &file_name, int64_t track_id, const std::string &message);
inline void
mxverb_tid(unsigned int level,
           const std::string &file_name,
           int64_t track_id,
           const boost::format &message) {
  mxverb_tid(level, file_name, track_id, message.str());
}

void MTX_DLL_API mxhexdump(unsigned int level, const void *buffer_to_dump, size_t lenth);

void MTX_DLL_API dump_ebml_elements(EbmlElement *element, bool with_values = false, unsigned int level = 0);

#endif  // __MTX_COMMON_OUTPUT_H
