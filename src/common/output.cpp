/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper functions, common variables

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include "common/mm_io.h"
#include "common/locale.h"
#include "common/output.h"
#include "common/smart_pointers.h"

bool g_suppress_warnings          = false;
bool g_warning_issued             = false;
std::string g_stdio_charset;

static bool s_mm_stdio_redirected = false;
charset_converter_cptr g_cc_stdio;
static counted_ptr<mm_io_c> s_mm_stdio;

void
init_stdio() {
  s_mm_stdio            = counted_ptr<mm_io_c>(new mm_stdio_c());
  s_mm_stdio_redirected = false;
}

void
redirect_stdio(mm_io_c *stdio) {
  s_mm_stdio = counted_ptr<mm_io_c>(stdio);
  s_mm_stdio_redirected = true;
}

bool
stdio_redirected() {
  return s_mm_stdio_redirected;
}

void
mxmsg(int level,
      std::string message) {
  static bool s_saw_cr_after_nl = false;

  if ('\n' == message[0]) {
    message.erase(0, 1);
    s_mm_stdio->puts("\n");
    s_saw_cr_after_nl = false;
  }

  if (level == MXMSG_ERROR) {
    if (s_saw_cr_after_nl)
      s_mm_stdio->puts("\n");
    s_mm_stdio->puts(g_cc_stdio->native(Y("Error: ")));

  } else if (level == MXMSG_WARNING)
    s_mm_stdio->puts(g_cc_stdio->native(Y("Warning: ")));

  else if (level == MXMSG_DEBUG)
    s_mm_stdio->puts(g_cc_stdio->native(Y("Debug> ")));

  int idx_cr = message.rfind('\r');
  if ((0 <= idx_cr) && (message.rfind('\n') < idx_cr))
    s_saw_cr_after_nl = true;

  string output = g_cc_stdio->native(message);
  s_mm_stdio->puts(output);
  s_mm_stdio->flush();
}

void
mxinfo(const std::string &info) {
  mxmsg(MXMSG_INFO, info);
}

void
mxwarn(const std::string &warning) {
  if (g_suppress_warnings)
    return;

  mxmsg(MXMSG_WARNING, warning);

  g_warning_issued = true;
}

void
mxerror(const std::string &error) {
  mxmsg(MXMSG_ERROR, error);
  mxexit(2);
}

void
mxinfo_fn(const std::string &file_name,
          const std::string &info) {
  mxmsg(MXMSG_INFO, (boost::format(Y("'%1%': %2%")) % file_name % info).str());
}

void
mxinfo_tid(const std::string &file_name,
           int64_t track_id,
           const std::string &info) {
  mxmsg(MXMSG_INFO, (boost::format(Y("'%1%' track %2%: %3%")) % file_name % track_id % info).str());
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
mxverb_fn(int level,
          const std::string &file_name,
          const std::string &message) {
  if (verbose < level)
    return;

  mxmsg(MXMSG_INFO, (boost::format(Y("'%1%': %2%")) % file_name % message).str());
}

void
mxverb_tid(int level,
           const std::string &file_name,
           int64_t track_id,
           const std::string &message) {
  if (verbose < level)
    return;

  mxmsg(MXMSG_INFO, (boost::format(Y("'%1%' track %2%: %3%")) % file_name % track_id % message).str());
}

void
init_cc_stdio() {
  set_cc_stdio(get_local_console_charset());
}

void
set_cc_stdio(const std::string &charset) {
  g_stdio_charset = charset;
  g_cc_stdio      = charset_converter_c::init(charset);
}

void
mxhexdump(int level,
          const unsigned char *buffer,
          int length) {
  int i, j;
  char output[24];

  if (verbose < level)
    return;
  j = 0;
  for (i = 0; i < length; i++) {
    if ((i % 16) == 0) {
      if (i > 0) {
        output[j] = 0;
        mxinfo(boost::format("%1%\n") % output);
        j = 0;
      }
      mxinfo(boost::format("%|1$08x|  ") % i);

    } else if ((i % 8) == 0) {
      mxinfo(" ");
      output[j] = ' ';
      j++;

    }
    if ((buffer[i] >= 32) && (buffer[i] < 128))
      output[j] = buffer[i];
    else
      output[j] = '.';
    j++;
    mxinfo(boost::format("%|1$02x| ") % (int)buffer[i]);
  }
  while ((i % 16) != 0) {
    if ((i % 8) == 0)
      mxinfo(" ");
    mxinfo("   ");
    i++;
  }
  output[j] = 0;
  mxinfo(boost::format("%1%\n") % output);
}

