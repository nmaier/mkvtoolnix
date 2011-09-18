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

#include "common/ebml.h"
#include "common/endian.h"
#include "common/mm_io.h"
#include "common/locale.h"
#include "common/output.h"
#include "common/smart_pointers.h"
#include "common/strings/formatting.h"
#include "common/strings/utf8.h"

#include <ebml/EbmlDate.h>
#include <ebml/EbmlDummy.h>
#include <ebml/EbmlMaster.h>
#include <ebml/EbmlSInteger.h>
#include <ebml/EbmlString.h>
#include <ebml/EbmlUInteger.h>
#include <ebml/EbmlUnicodeString.h>
#include <ebml/EbmlVoid.h>

using namespace libebml;

bool g_suppress_info              = false;
bool g_suppress_warnings          = false;
bool g_warning_issued             = false;
std::string g_stdio_charset;
static bool s_mm_stdio_redirected = false;

charset_converter_cptr g_cc_stdio = charset_converter_cptr(new charset_converter_c);
counted_ptr<mm_io_c> g_mm_stdio   = counted_ptr<mm_io_c>(new mm_stdio_c);

void
redirect_stdio(mm_io_c *stdio) {
  g_mm_stdio            = counted_ptr<mm_io_c>(stdio);
  s_mm_stdio_redirected = true;
}

bool
stdio_redirected() {
  return s_mm_stdio_redirected;
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
    if (!ba::starts_with(message, Y("Error:")))
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

void
mxinfo(const std::string &info) {
  mxmsg(MXMSG_INFO, info);
}

void
mxinfo(const std::wstring &info) {
  mxmsg(MXMSG_INFO, to_utf8(info));
}

void
mxinfo(const boost::wformat &info) {
  mxmsg(MXMSG_INFO, to_utf8(info.str()));
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
mxverb_fn(unsigned int level,
          const std::string &file_name,
          const std::string &message) {
  if (verbose < level)
    return;

  mxmsg(MXMSG_INFO, (boost::format(Y("'%1%': %2%")) % file_name % message).str());
}

void
mxverb_tid(unsigned int level,
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
mxhexdump(unsigned int level,
          const void *buffer_to_dump,
          size_t length) {
  if (verbose < level)
    return;

  const unsigned char *buffer = static_cast<const unsigned char *>(buffer_to_dump);
  unsigned int output_idx = 0;
  unsigned int buffer_idx = 0;
  char output[24];

  while (buffer_idx < length) {
    if ((buffer_idx % 16) == 0) {
      if (0 < buffer_idx) {
        output[output_idx] = 0;
        mxinfo(boost::format("%1%\n") % output);
        output_idx = 0;
      }
      mxinfo(boost::format("%|1$08x|  ") % buffer_idx);

    } else if ((buffer_idx % 8) == 0) {
      mxinfo(" ");
      output[output_idx] = ' ';
      ++output_idx;
    }

    output[output_idx] = ((32 <= buffer[buffer_idx]) && (128 > buffer[buffer_idx])) ? buffer[buffer_idx] : '.';
    ++output_idx;

    mxinfo(boost::format("%|1$02x| ") % static_cast<unsigned int>(buffer[buffer_idx]));

    ++buffer_idx;
  }

  while ((buffer_idx % 16) != 0) {
    if ((buffer_idx % 8) == 0)
      mxinfo(" ");
    mxinfo("   ");
    ++buffer_idx;
  }
  output[output_idx] = 0;

  mxinfo(boost::format("%1%\n") % output);
}

void
dump_ebml_elements(EbmlElement *element,
                   bool with_values,
                   unsigned int level) {
  std::string indent_str, value_str;
  size_t i;

  for (i = 1; i <= level; ++i)
    indent_str += " ";

  if (with_values) {
    if (NULL != dynamic_cast<EbmlUInteger *>(element))
      value_str = to_string(uint64(*static_cast<EbmlUInteger *>(element)));

    else if (NULL != dynamic_cast<EbmlSInteger *>(element))
      value_str = to_string(int64(*static_cast<EbmlSInteger *>(element)));

    else if (NULL != dynamic_cast<EbmlFloat *>(element))
      value_str = to_string(double(*static_cast<EbmlFloat *>(element)), 9);

    else if (NULL != dynamic_cast<EbmlUnicodeString *>(element))
      value_str = UTFstring_to_cstrutf8(UTFstring(*static_cast<EbmlUnicodeString *>(element)));

    else if (NULL != dynamic_cast<EbmlString *>(element))
      value_str = std::string(*static_cast<EbmlString *>(element));

    else if (NULL != dynamic_cast<EbmlDate *>(element))
      value_str = to_string(static_cast<EbmlDate *>(element)->GetEpochDate());

    else
      value_str = (boost::format("(type: %1%)") %
                   (  NULL != dynamic_cast<EbmlBinary *>(element) ? "binary"
                    : NULL != dynamic_cast<EbmlMaster *>(element) ? "master"
                    : NULL != dynamic_cast<EbmlVoid *>(element)   ? "void"
                    :                                               "unknown")).str();

    value_str = " " + value_str;
  }

  mxinfo(boost::format("%1%%2%%3%\n") % indent_str % EBML_NAME(element) % value_str);

  EbmlMaster *master = dynamic_cast<EbmlMaster *>(element);
  if (NULL == master)
    return;

  for (i = 0; master->ListSize() > i; ++i)
    dump_ebml_elements((*master)[i], with_values, level + 1);
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
