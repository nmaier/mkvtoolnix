/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions used in all programs, helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_OUTPUT_H
#define MTX_COMMON_OUTPUT_H

#include "common/os.h"

#include <functional>

#include <ebml/EbmlElement.h>

#include "common/locale.h"
#include "common/mm_io.h"

using namespace libebml;

typedef std::function<void(unsigned int level, std::string const &)> mxmsg_handler_t;
void set_mxmsg_handler(unsigned int level, mxmsg_handler_t const &handler);

extern bool g_suppress_info, g_suppress_warnings;
extern std::string g_stdio_charset;
extern charset_converter_cptr g_cc_stdio;
extern std::shared_ptr<mm_io_c> g_mm_stdio;

void redirect_stdio(const mm_io_cptr &new_stdio);
bool stdio_redirected();

void init_common_output(bool no_charset_detection);
void set_cc_stdio(const std::string &charset);

void mxmsg(unsigned int level, std::string message);
inline void
mxmsg(unsigned int level,
      const boost::format &message) {
  mxmsg(level, message.str());
}

void mxinfo(const std::string &info);
inline void mxinfo(const boost::format &info) {
  mxinfo(info.str());
}
inline void mxinfo(char const *info) {
  mxinfo(std::string{info});
}
void mxinfo(const std::wstring &info);
void mxinfo(const boost::wformat &info);

void mxwarn(const std::string &warning);
inline void
mxwarn(const boost::format &warning) {
  mxwarn(warning.str());
}

void mxerror(const std::string &error);
inline void
mxerror(const boost::format &error) {
  mxerror(error.str());
}

#define mxverb(level, message)        \
  if (verbose >= level)               \
    mxinfo(message);

void mxinfo_fn(const std::string &file_name, const std::string &info);
inline void
mxinfo_fn(const std::string &file_name,
          const boost::format &info) {
  mxinfo_fn(file_name, info.str());
}

void mxinfo_tid(const std::string &file_name, int64_t track_id, const std::string &info);
inline void
mxinfo_tid(const std::string &file_name,
           int64_t track_id,
           const boost::format &info) {
  mxinfo_tid(file_name, track_id, info.str());
}

void mxwarn_fn(const std::string &file_name, const std::string &info);
inline void
mxwarn_fn(const std::string &file_name,
          const boost::format &info) {
  mxwarn_fn(file_name, info.str());
}

void mxwarn_tid(const std::string &file_name, int64_t track_id, const std::string &warning);
inline void
mxwarn_tid(const std::string &file_name,
           int64_t track_id,
           const boost::format &warning) {
  mxwarn_tid(file_name, track_id, warning.str());
}

void mxerror_fn(const std::string &file_name, const std::string &error);
inline void
mxerror_fn(const std::string &file_name,
           const boost::format &error) {
  mxerror_fn(file_name, error.str());
}

void mxerror_tid(const std::string &file_name, int64_t track_id, const std::string &error);
inline void
mxerror_tid(const std::string &file_name,
            int64_t track_id,
            const boost::format &error) {
  mxerror_tid(file_name, track_id, error.str());
}

void mxverb_fn(unsigned int level, const std::string &file_name, const std::string &message);
inline void
mxverb_fn(unsigned int level,
          const std::string &file_name,
          const boost::format &message) {
  mxverb_fn(level, file_name, message.str());
}

void mxverb_tid(unsigned int level, const std::string &file_name, int64_t track_id, const std::string &message);
inline void
mxverb_tid(unsigned int level,
           const std::string &file_name,
           int64_t track_id,
           const boost::format &message) {
  mxverb_tid(level, file_name, track_id, message.str());
}

extern const std::string empty_string;

std::string fourcc_to_string(uint32_t fourcc);

#endif  // MTX_COMMON_OUTPUT_H
