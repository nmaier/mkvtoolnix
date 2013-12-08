/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper functions, common variables

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <sstream>

#include "common/ebml.h"
#include "common/endian.h"
#include "common/locale.h"
#include "common/logger.h"
#include "common/mm_io.h"
#include "common/strings/utf8.h"

bool g_suppress_info              = false;
bool g_suppress_warnings          = false;
bool g_warning_issued             = false;
std::string g_stdio_charset;
static bool s_mm_stdio_redirected = false;

charset_converter_cptr g_cc_stdio = charset_converter_cptr(new charset_converter_c);
std::shared_ptr<mm_io_c> g_mm_stdio   = std::shared_ptr<mm_io_c>(new mm_stdio_c);

static mxmsg_handler_t s_mxmsg_info_handler, s_mxmsg_warning_handler, s_mxmsg_error_handler;

void
redirect_stdio(const mm_io_cptr &stdio) {
  g_mm_stdio            = stdio;
  s_mm_stdio_redirected = true;
}

bool
stdio_redirected() {
  return s_mm_stdio_redirected;
}

void
set_mxmsg_handler(unsigned int level,
                  mxmsg_handler_t const &handler) {
  if (MXMSG_INFO == level)
    s_mxmsg_info_handler = handler;
  else if (MXMSG_WARNING == level)
    s_mxmsg_warning_handler = handler;
  else if (MXMSG_ERROR == level)
    s_mxmsg_error_handler = handler;
  else
    assert(false);
}

void
mxmsg(unsigned int level,
      std::string message) {
  static bool s_saw_cr_after_nl = false;

  if (g_suppress_info && (MXMSG_INFO == level))
    return;

  if ('\n' == message[0]) {
    message.erase(0, 1);
    g_mm_stdio->puts("\n");
    s_saw_cr_after_nl = false;
  }

  if (level == MXMSG_ERROR) {
    if (s_saw_cr_after_nl)
      g_mm_stdio->puts("\n");
    if (!balg::starts_with(message, Y("Error:")))
      g_mm_stdio->puts(g_cc_stdio->native(Y("Error: ")));

  } else if (level == MXMSG_WARNING)
    g_mm_stdio->puts(g_cc_stdio->native(Y("Warning: ")));

  else if (level == MXMSG_DEBUG)
    g_mm_stdio->puts(g_cc_stdio->native(Y("Debug> ")));

  size_t idx_cr = message.rfind('\r');
  if (std::string::npos != idx_cr) {
    size_t idx_nl = message.rfind('\n');
    if ((std::string::npos != idx_nl) && (idx_nl < idx_cr))
      s_saw_cr_after_nl = true;
  }

  std::string output = g_cc_stdio->native(message);
  g_mm_stdio->puts(output);
  g_mm_stdio->flush();
}

static void
default_mxinfo(unsigned int,
               std::string const &info) {
  mxmsg(MXMSG_INFO, info);
}

void
mxinfo(std::string const &info) {
  s_mxmsg_info_handler(MXMSG_INFO, info);
}

void
mxinfo(const std::wstring &info) {
  mxinfo(to_utf8(info));
}

void
mxinfo(const boost::wformat &info) {
  mxinfo(to_utf8(info.str()));
}

static void
default_mxwarn(unsigned int,
               std::string const &warning) {
  if (g_suppress_warnings)
    return;

  mxmsg(MXMSG_WARNING, warning);

  g_warning_issued = true;
}

void
mxwarn(std::string const &warning) {
  s_mxmsg_warning_handler(MXMSG_WARNING, warning);
}

static void
default_mxerror(unsigned int,
                std::string const &error) {
  mxmsg(MXMSG_ERROR, error);
  mxexit(2);
}

void
mxerror(std::string const &error) {
  s_mxmsg_error_handler(MXMSG_ERROR, error);
}

void
mxinfo_fn(const std::string &file_name,
          const std::string &info) {
  mxinfo((boost::format(Y("'%1%': %2%")) % file_name % info).str());
}

void
mxinfo_tid(const std::string &file_name,
           int64_t track_id,
           const std::string &info) {
  mxinfo((boost::format(Y("'%1%' track %2%: %3%")) % file_name % track_id % info).str());
}

void
mxwarn_fn(const std::string &file_name,
          const std::string &warning) {
  mxwarn(boost::format(Y("'%1%': %2%")) % file_name % warning);
}

void
mxwarn_tid(const std::string &file_name,
           int64_t track_id,
           const std::string &warning) {
  mxwarn(boost::format(Y("'%1%' track %2%: %3%")) % file_name % track_id % warning);
}

void
mxerror_fn(const std::string &file_name,
           const std::string &error) {
  mxerror(boost::format(Y("'%1%': %2%")) % file_name % error);
}

void
mxerror_tid(const std::string &file_name,
            int64_t track_id,
            const std::string &error) {
  mxerror(boost::format(Y("'%1%' track %2%: %3%")) % file_name % track_id % error);
}

void
mxverb_fn(unsigned int level,
          const std::string &file_name,
          const std::string &message) {
  if (verbose < level)
    return;

  mxinfo((boost::format(Y("'%1%': %2%")) % file_name % message).str());
}

void
mxverb_tid(unsigned int level,
           const std::string &file_name,
           int64_t track_id,
           const std::string &message) {
  if (verbose < level)
    return;

  mxinfo((boost::format(Y("'%1%' track %2%: %3%")) % file_name % track_id % message).str());
}

void
init_common_output(bool no_charset_detection) {
  if (no_charset_detection)
    set_cc_stdio("UTF-8");
  else
    set_cc_stdio(get_local_console_charset());
  set_mxmsg_handler(MXMSG_INFO,    default_mxinfo);
  set_mxmsg_handler(MXMSG_WARNING, default_mxwarn);
  set_mxmsg_handler(MXMSG_ERROR,   default_mxerror);
}

void
set_cc_stdio(const std::string &charset) {
  g_stdio_charset = charset;
  g_cc_stdio      = charset_converter_c::init(charset);
}

std::string
fourcc_to_string(uint32_t fourcc) {
  unsigned char buffer[4], idx;

  put_uint32_be(buffer, fourcc);
  for (idx = 0; 4 > idx; ++idx)
    if (buffer[idx] < ' ')
      buffer[idx] = ' ';

  return std::string(reinterpret_cast<char *>(buffer), 4);
}
