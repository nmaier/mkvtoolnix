/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Definitions for locale handling functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_LOCALE_H
#define __MTX_COMMON_LOCALE_H

#include "common/common_pch.h"

#if defined(HAVE_ICONV_H)
# include <iconv.h>
#endif

#if defined(SYS_WINDOWS)
# include <windows.h>
#endif

#if !defined(HAVE_ICONV_H) && !defined(SYS_WINDOWS)
# error Build requires either <iconv.h> or Windows APIs.
#endif

class charset_converter_c;
typedef std::shared_ptr<charset_converter_c> charset_converter_cptr;

class charset_converter_c {
protected:
  std::string m_charset;
  bool m_detect_byte_order_marker;

public:
  charset_converter_c();
  charset_converter_c(const std::string &charset);
  virtual ~charset_converter_c();

  virtual std::string utf8(const std::string &source);
  virtual std::string native(const std::string &source);
  virtual void enable_byte_order_marker_detection(bool enable);

protected:
  bool handle_string_with_bom(const std::string &source, std::string &recoded);

public:                         // Static members
  static charset_converter_cptr init(const std::string &charset);
  static bool is_utf8_charset_name(const std::string &charset);

private:
  static std::map<std::string, charset_converter_cptr> s_converters;
};

#if defined(HAVE_ICONV_H)
class iconv_charset_converter_c: public charset_converter_c {
private:
  bool m_is_utf8;
  iconv_t m_to_utf8_handle, m_from_utf8_handle;

public:
  iconv_charset_converter_c(const std::string &charset);
  virtual ~iconv_charset_converter_c();

  virtual std::string utf8(const std::string &source);
  virtual std::string native(const std::string &source);

public:                         // Static functions
  static bool is_available(const std::string &charset);

private:                        // Static functions
  static std::string convert(iconv_t handle, const std::string &source);
};
# endif  // HAVE_ICONV_H

#if defined(SYS_WINDOWS)
class windows_charset_converter_c: public charset_converter_c {
private:
  bool m_is_utf8;
  UINT m_code_page;

public:
  windows_charset_converter_c(const std::string &charset);
  virtual ~windows_charset_converter_c();

  virtual std::string utf8(const std::string &source);
  virtual std::string native(const std::string &source);

public:                         // Static functions
  static bool is_available(const std::string &charset);

private:                        // Static functions
  static std::string convert(UINT source_code_page, UINT destination_code_page, const std::string &source);
  static UINT extract_code_page(const std::string &charset);
};

unsigned Utf8ToUtf16(const char *utf8, int utf8len, wchar_t *utf16, unsigned utf16len);
char * win32_wide_to_multi(const wchar_t *wbuffer);
wchar_t * win32_utf8_to_utf16(const char *s);
std::string win32_wide_to_multi_utf8(const wchar_t *wbuffer);
#endif

extern charset_converter_cptr g_cc_local_utf8;

std::string get_local_charset();
std::string get_local_console_charset();

std::vector<std::string> command_line_utf8(int argc, char **argv);

#endif  // __MTX_COMMON_LOCALE_H
