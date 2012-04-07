/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   locale handling functions

   Written by Moritz Bunkus <moritz@bunkus.org>.

   Utf8ToUtf16:
     Written by Mike Matsnev <mike@po.cs.msu.su>
     Modifications by Moritz Bunkus <moritz@bunkus.org>
     Additional code by Alexander Noé <alexander.noe@s2001.tu-chemnitz.de>
*/

#include "common/common_pch.h"

#include <errno.h>
#if HAVE_NL_LANGINFO
# include <langinfo.h>
#elif HAVE_LOCALE_CHARSET
# include <libcharset.h>
#endif
#include <locale.h>
#if defined(SYS_WINDOWS)
# include <windows.h>
#endif

#include "common/memory.h"
#include "common/mm_io.h"
#include "common/strings/parsing.h"
#ifdef SYS_WINDOWS
# include "common/fs_sys_helpers.h"
# include "common/strings/formatting.h"
#endif

charset_converter_cptr g_cc_local_utf8;

std::map<std::string, charset_converter_cptr> charset_converter_c::s_converters;

charset_converter_c::charset_converter_c()
  : m_detect_byte_order_marker(false)
{
}

charset_converter_c::charset_converter_c(const std::string &charset)
  : m_charset(charset)
  , m_detect_byte_order_marker(false)
{
}

charset_converter_c::~charset_converter_c() {
}

std::string
charset_converter_c::utf8(const std::string &source) {
  return source;
}

std::string
charset_converter_c::native(const std::string &source) {
  return source;
}

charset_converter_cptr
charset_converter_c::init(const std::string &charset) {
  std::string actual_charset = charset.empty() ? get_local_charset() : charset;

  std::map<std::string, charset_converter_cptr>::iterator converter = s_converters.find(actual_charset);
  if (converter != s_converters.end())
    return (*converter).second;

#if defined(HAVE_ICONV_H) && defined(SYS_WINDOWS)
  if (iconv_charset_converter_c::is_available(actual_charset) || !windows_charset_converter_c::is_available(actual_charset))
    return charset_converter_cptr(new iconv_charset_converter_c(actual_charset));

  return charset_converter_cptr(new windows_charset_converter_c(actual_charset));

#elif defined(HAVE_ICONV_H)
  return charset_converter_cptr(new iconv_charset_converter_c(actual_charset));

#else
  return charset_converter_cptr(new windows_charset_converter_c(actual_charset));
#endif
}

bool
charset_converter_c::is_utf8_charset_name(const std::string &charset) {
  return ((charset == "UTF8") || (charset == "UTF-8"));
}

void
charset_converter_c::enable_byte_order_marker_detection(bool enable) {
  m_detect_byte_order_marker = enable;
}

bool
charset_converter_c::handle_string_with_bom(const std::string &source,
                                            std::string &recoded) {
  if (!m_detect_byte_order_marker)
    return false;

  if (!mm_text_io_c::has_byte_order_marker(source))
    return false;

  recoded.clear();
  mm_text_io_c io(new mm_mem_io_c(reinterpret_cast<const unsigned char *>(source.c_str()), source.length()));
  std::string line;
  while (io.getline2(line))
    recoded += line;

  return true;
}

// ------------------------------------------------------------
#if defined(HAVE_ICONV_H)
iconv_charset_converter_c::iconv_charset_converter_c(const std::string &charset)
  : charset_converter_c(charset)
  , m_is_utf8(false)
  , m_to_utf8_handle(reinterpret_cast<iconv_t>(-1))
  , m_from_utf8_handle(reinterpret_cast<iconv_t>(-1))
{
  if (is_utf8_charset_name(charset)) {
    m_is_utf8 = true;
    return;
  }

  m_to_utf8_handle = iconv_open("UTF-8", charset.c_str());
  if (reinterpret_cast<iconv_t>(-1) == m_to_utf8_handle)
    mxwarn(boost::format(Y("Could not initialize the iconv library for the conversion from %1% to UFT-8. "
                           "Some strings will not be converted to UTF-8 and the resulting Matroska file "
                           "might not comply with the Matroska specs (error: %2%, %3%).\n"))
           % charset % errno % strerror(errno));

  m_from_utf8_handle = iconv_open(charset.c_str(), "UTF-8");
  if (reinterpret_cast<iconv_t>(-1) == m_from_utf8_handle)
    mxwarn(boost::format(Y("Could not initialize the iconv library for the conversion from UFT-8 to %1%. "
                           "Some strings cannot be converted from UTF-8 and might be displayed incorrectly (error: %2%, %3%).\n"))
           % charset % errno % strerror(errno));
}

iconv_charset_converter_c::~iconv_charset_converter_c() {
  if (reinterpret_cast<iconv_t>(-1) != m_to_utf8_handle)
    iconv_close(m_to_utf8_handle);

  if (reinterpret_cast<iconv_t>(-1) != m_from_utf8_handle)
    iconv_close(m_from_utf8_handle);
}

std::string
iconv_charset_converter_c::utf8(const std::string &source) {
  std::string recoded;
  if (handle_string_with_bom(source, recoded))
    return recoded;

  return m_is_utf8 ? source : iconv_charset_converter_c::convert(m_to_utf8_handle, source);
}

std::string
iconv_charset_converter_c::native(const std::string &source) {
  return m_is_utf8 ? source : iconv_charset_converter_c::convert(m_from_utf8_handle, source);
}

std::string
iconv_charset_converter_c::convert(iconv_t handle,
                                   const std::string &source) {
  if (reinterpret_cast<iconv_t>(-1) == handle)
    return source;

  int length        = source.length() * 4;
  char *destination = (char *)safemalloc(length + 1);
  memset(destination, 0, length + 1);

  iconv(handle, nullptr, 0, nullptr, 0); // Reset the iconv state.

  size_t length_source      = length / 4;
  size_t length_destination = length;
  char *source_copy         = safestrdup(source.c_str());
  char *ptr_source          = source_copy;
  char *ptr_destination     = destination;
  iconv(handle, (ICONV_CONST char **)&ptr_source, &length_source, &ptr_destination, &length_destination);
  iconv(handle, nullptr, nullptr, &ptr_destination, &length_destination);

  safefree(source_copy);
  std::string result = destination;
  safefree(destination);

  return result;
}

bool
iconv_charset_converter_c::is_available(const std::string &charset) {
  if (is_utf8_charset_name(charset))
    return true;

  iconv_t handle = iconv_open("UTF-8", charset.c_str());
  if (reinterpret_cast<iconv_t>(-1) == handle)
    return false;

  iconv_close(handle);

  return true;
}
#endif //HAVE_ICONV_H

// ------------------------------------------------------------

#if defined(SYS_WINDOWS)

windows_charset_converter_c::windows_charset_converter_c(const std::string &charset)
  : charset_converter_c(charset)
  , m_is_utf8(is_utf8_charset_name(charset))
  , m_code_page(extract_code_page(charset))
{
}

windows_charset_converter_c::~windows_charset_converter_c() {
}

std::string
windows_charset_converter_c::utf8(const std::string &source) {
  std::string recoded;
  if (handle_string_with_bom(source, recoded))
    return recoded;

  return m_is_utf8 ? source : windows_charset_converter_c::convert(m_code_page, CP_UTF8, source);
}

std::string
windows_charset_converter_c::native(const std::string &source) {
  return m_is_utf8 ? source : windows_charset_converter_c::convert(CP_UTF8, m_code_page, source);
}

std::string
windows_charset_converter_c::convert(unsigned int source_code_page,
                                     unsigned int destination_code_page,
                                     const std::string &source) {
  if (source_code_page == destination_code_page)
    return source;

  int num_wide_chars = MultiByteToWideChar(source_code_page, 0, source.c_str(), -1, nullptr, 0);
  wchar_t *wbuffer   = new wchar_t[num_wide_chars];
  MultiByteToWideChar(source_code_page, 0, source.c_str(), -1, wbuffer, num_wide_chars);

  int num_bytes = WideCharToMultiByte(destination_code_page, 0, wbuffer, -1, nullptr, 0, nullptr, nullptr);
  char *buffer  = new char[num_bytes];
  WideCharToMultiByte(destination_code_page, 0, wbuffer, -1, buffer, num_bytes, nullptr, nullptr);

  std::string result = buffer;

  delete []wbuffer;
  delete []buffer;

  return result;
}

bool
windows_charset_converter_c::is_available(const std::string &charset) {
  unsigned int code_page = extract_code_page(charset);
  if (0 == code_page)
    return false;

  return IsValidCodePage(code_page);
}

unsigned int
windows_charset_converter_c::extract_code_page(const std::string &charset) {
  if (charset.substr(0, 2) != "CP")
    return 0;

  std::string number_as_str = charset.substr(2, charset.length() - 2);
  uint64_t number           = 0;
  if (!parse_number(number_as_str.c_str(), number))
    return 0;

  return number;
}

#endif  // defined(SYS_WINDOWS)

// ------------------------------------------------------------

std::string
get_local_charset() {
  std::string lc_charset;

  setlocale(LC_CTYPE, "");
#if defined(COMP_MINGW) || defined(COMP_MSC)
  lc_charset = "CP" + to_string(GetACP());
#elif defined(SYS_SOLARIS)
  int i;

  lc_charset = nl_langinfo(CODESET);
  if (parse_number(lc_charset, i))
    lc_charset = std::string("ISO") + lc_charset + std::string("-US");
#elif HAVE_NL_LANGINFO
  lc_charset = nl_langinfo(CODESET);
#elif HAVE_LOCALE_CHARSET
  lc_charset = locale_charset();
#endif

  return lc_charset;
}

std::string
get_local_console_charset() {
#if defined(SYS_WINDOWS)
  if (get_windows_version() >= WINDOWS_VERSION_VISTA)
    return std::string("CP") + to_string(GetACP());
  return std::string("CP") + to_string(GetOEMCP());
#else
  return get_local_charset();
#endif
}

#if defined(SYS_WINDOWS)
unsigned
Utf8ToUtf16(const char *utf8,
            int utf8len,
            wchar_t *utf16,
            unsigned utf16len) {
  const unsigned char *u = (const unsigned char *)utf8;
  const unsigned char *t = u + (utf8len < 0 ? strlen(utf8)+1 : utf8len);
  wchar_t *d = utf16, *w = utf16 + utf16len, c0, c1;
  unsigned ch;

  if (utf16len == 0) {
    d = utf16 = nullptr;
    w = d - 1;
  }

  while (u<t && d<w) {
    if (!(*u & 0x80))
      ch = *u++;
    else if ((*u & 0xe0) == 0xc0) {
      ch = (unsigned)(*u++ & 0x1f) << 6;
      if (u<t && (*u & 0xc0) == 0x80)
        ch |= *u++ & 0x3f;
    } else if ((*u & 0xf0) == 0xe0) {
      ch = (unsigned)(*u++ & 0x0f) << 12;
      if (u<t && (*u & 0xc0) == 0x80) {
        ch |= (unsigned)(*u++ & 0x3f) << 6;
        if (u<t && (*u & 0xc0) == 0x80)
          ch |= *u++ & 0x3f;
      }
    } else if ((*u & 0xf8) == 0xf0) {
      ch = (unsigned)(*u++ & 0x07) << 18;
      if (u<t && (*u & 0xc0) == 0x80) {
        ch |= (unsigned)(*u++ & 0x3f) << 12;
        if (u<t && (*u & 0xc0) == 0x80) {
          ch |= (unsigned)(*u++ & 0x3f) << 6;
          if (u<t && (*u & 0xc0) == 0x80)
            ch |= *u++ & 0x3f;
        }
      }
    } else
      continue;

    c0 = c1 = 0x0000;

    if (ch < 0xd800)
      c0 = (wchar_t)ch;
    else if (ch < 0xe000) // invalid
      c0 = 0x0020;
    else if (ch < 0xffff)
      c0 = (wchar_t)ch;
    else if (ch < 0x110000) {
      c0 = 0xd800 | (ch>>10);
      c1 = 0xdc00 | (ch & 0x03ff);
    } else
      c0 = 0x0020;

    if (utf16) {
      if (c1) {
        if (d+1<w) {
          *d++ = c0;
          *d++ = c1;
        } else
          break;
      } else
        *d++ = c0;
    } else {
      d++;
      if (c1)
        d++;
    }
  }

  if (utf16 && d<w)
    *d = 0x0000;

  if (utf8len < 0 && utf16len > 0)
    utf16[utf16len - 1] = L'\0';

  return (unsigned)(d - utf16);
}

char *
win32_wide_to_multi(const wchar_t *wbuffer) {
  int reqbuf = WideCharToMultiByte(CP_ACP, 0, wbuffer, -1, nullptr, 0, nullptr,
                                   nullptr);
  char *buffer = new char[reqbuf];
  WideCharToMultiByte(CP_ACP, 0, wbuffer, -1, buffer, reqbuf, nullptr, nullptr);

  return buffer;
}

std::string
win32_wide_to_multi_utf8(const wchar_t *wbuffer) {
  int reqbuf   = WideCharToMultiByte(CP_UTF8, 0, wbuffer, -1, nullptr, 0, nullptr, nullptr);
  char *buffer = new char[reqbuf];
  WideCharToMultiByte(CP_UTF8, 0, wbuffer, -1, buffer, reqbuf, nullptr, nullptr);

  std::string retval = buffer;

  delete []buffer;

  return retval;
}

wchar_t *
win32_utf8_to_utf16(const char *s) {
  int wreqbuf = Utf8ToUtf16(s, -1, nullptr, 0);
  wchar_t *wbuffer = new wchar_t[wreqbuf];

  Utf8ToUtf16(s, -1, wbuffer, wreqbuf);

  return wbuffer;
}

#endif  // SYS_WINDOWS
