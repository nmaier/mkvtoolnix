/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  common.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief helper functions, common variables
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include "os.h"

#include <ctype.h>
#include <errno.h>
#include <iconv.h>
#if defined(COMP_MSC)
#include <libcharset.h>
#elif !defined(COMP_MINGW)
#include <langinfo.h>
#endif
#include <locale.h>
#include <malloc.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <wchar.h>

#include <string>
#include <vector>

using namespace std;

#include "common.h"
#include "pr_generic.h"

int verbose = 1;

bitvalue_c::bitvalue_c(int nbitsize) {
  assert(nbitsize > 0);
  assert((nbitsize % 8) == 0);
  bitsize = nbitsize;
  value = (unsigned char *)safemalloc(bitsize / 8);
  memset(value, 0, bitsize / 8);
}

bitvalue_c::bitvalue_c(const bitvalue_c &src) {
  value = NULL;
  *this = src;
}

#define ishexdigit(c) (isdigit(c) || (((c) >= 'a') && ((c) <= 'f')) || \
                       (((c) >= 'A') && ((c) <= 'F')))
#define upperchar(c) (((c) >= 'a') ? ((c) - 'a' + 'A') : (c))
#define hextodec(c) (isdigit(c) ? ((c) - '0') : ((c) - 'A' + 10))

bitvalue_c::bitvalue_c(const char *s, int allowed_bitlength) {
  int len, i;
  string s2;

  len = strlen(s);
  if (len < 2)
    throw exception();

  if ((s[0] == '0') && (s[1] == 'x')) {
    i = 0;
    while (i < len) {
      if ((len - i) < 4)
        throw exception();
      if ((s[i] != '0') || (s[i + 1] != 'x'))
        throw exception();
      s2 += s[i + 2];
      s2 += s[i + 3];
      i += 4;
      if (i < len) {
        if (s[i] != ' ')
          throw exception();
        i++;
      }
    }
    s = s2.c_str();
    len = strlen(s);
  }

  if ((len % 2) == 1)
    throw exception();

  if ((allowed_bitlength != -1) && ((len * 4) != allowed_bitlength))
    throw exception();

  for (i = 0; i < len; i++)
    if (!ishexdigit(s[i]))
      throw exception();

  value = (unsigned char *)safemalloc(len / 2);
  bitsize = len * 4;

  for (i = 0; i < len; i += 2)
    value[i / 2] = hextodec(upperchar(s[i])) * 16 +
      hextodec(upperchar(s[i + 1]));
}

bitvalue_c &bitvalue_c::operator =(const bitvalue_c &src) {
  safefree(value);
  bitsize = src.bitsize;
  value = (unsigned char *)safememdup(src.value, bitsize / 8);

  return *this;
}

bitvalue_c::~bitvalue_c() {
  safefree(value);
}

bool bitvalue_c::operator ==(const bitvalue_c &cmp) const {
  int i;

  if (cmp.bitsize != bitsize)
    return false;
  for (i = 0; i < bitsize / 8; i++)
    if (value[i] != cmp.value[i])
      return false;
  return true;
}

unsigned char bitvalue_c::operator [](int index) const {
  assert((index >= 0) && (index < (bitsize / 8)));
  return value[index];
}

int bitvalue_c::size() const {
  return bitsize;
}

unsigned char *bitvalue_c::data() const {
  return value;
}

void bitvalue_c::generate_random() {
  int i;

  for (i = 0; i < bitsize / 8; i++)
    value[i] = (unsigned char)(255.0 * rand() / RAND_MAX);
}

// ---------------------------------------------------------------------

byte_cursor_c::byte_cursor_c(const unsigned char *ndata, int nsize) :
  size(nsize),
  data(ndata) {
  pos = 0;
  if (nsize < 0)
    throw exception();
}

unsigned char byte_cursor_c::get_byte() {
  if ((pos + 1) > size)
    throw exception();

  pos++;

  return data[pos - 1];
}

unsigned short byte_cursor_c::get_word() {
  unsigned short v;

  if ((pos + 2) > size)
    throw exception();

  v = data[pos];
  v = (v << 8) | (data[pos + 1] & 0xff);
  pos += 2;

  return v;
}

unsigned int byte_cursor_c::get_dword() {
  unsigned int v;

  if ((pos + 4) > size)
    throw exception();

  v = data[pos];
  v = (v << 8) | (data[pos + 1] & 0xff);
  v = (v << 8) | (data[pos + 2] & 0xff);
  v = (v << 8) | (data[pos + 3] & 0xff);
  pos += 4;

  return v;
}

void byte_cursor_c::skip(int n) {
  if ((pos + n) > size)
    throw exception();

  pos += n;
}

void byte_cursor_c::get_bytes(unsigned char *dst, int n) {
  if ((pos + n) > size)
    throw exception();

  memcpy(dst, &data[pos], n);
  pos += n;
}

int byte_cursor_c::get_pos() {
  return pos;
}

int byte_cursor_c::get_len() {
  return size - pos;
}

/*
 * Control functions
 */

void die(const char *fmt, ...) {
  va_list ap;

  mxprint(stdout, "'die' called: ");
  va_start(ap, fmt);
  vfprintf(stdout, fmt, ap);
  va_end(ap);
  mxprint(stdout, "\n");
#ifdef DEBUG
  debug_facility.dump_info();
#endif
  exit(2);
}

void _trace(const char *func, const char *file, int line) {
  mxdebug("trace: %s:%s (%d)\n", file, func, line);
}

/*
 * Endianess stuff
 */

uint16_t get_uint16(const void *buf) {
  uint16_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[1] & 0xff;
  ret = (ret << 8) + (tmp[0] & 0xff);

  return ret;
}

uint32_t get_uint24(const void *buf) {
  uint32_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[2] & 0xff;
  ret = (ret << 8) + (tmp[1] & 0xff);
  ret = (ret << 8) + (tmp[0] & 0xff);

  return ret;
}

uint32_t get_uint32(const void *buf) {
  uint32_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[3] & 0xff;
  ret = (ret << 8) + (tmp[2] & 0xff);
  ret = (ret << 8) + (tmp[1] & 0xff);
  ret = (ret << 8) + (tmp[0] & 0xff);

  return ret;
}

uint64_t get_uint64(const void *buf) {
  uint64_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[7] & 0xff;
  ret = (ret << 8) + (tmp[6] & 0xff);
  ret = (ret << 8) + (tmp[5] & 0xff);
  ret = (ret << 8) + (tmp[4] & 0xff);
  ret = (ret << 8) + (tmp[3] & 0xff);
  ret = (ret << 8) + (tmp[2] & 0xff);
  ret = (ret << 8) + (tmp[1] & 0xff);
  ret = (ret << 8) + (tmp[0] & 0xff);

  return ret;
}

uint16_t get_uint16_be(const void *buf) {
  uint16_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[0] & 0xff;
  ret = (ret << 8) + (tmp[1] & 0xff);

  return ret;
}

uint32_t get_uint24_be(const void *buf) {
  uint32_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[0] & 0xff;
  ret = (ret << 8) + (tmp[1] & 0xff);
  ret = (ret << 8) + (tmp[2] & 0xff);

  return ret;
}

uint32_t get_uint32_be(const void *buf) {
  uint32_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[0] & 0xff;
  ret = (ret << 8) + (tmp[1] & 0xff);
  ret = (ret << 8) + (tmp[2] & 0xff);
  ret = (ret << 8) + (tmp[3] & 0xff);

  return ret;
}

uint64_t get_uint64_be(const void *buf) {
  uint64_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[0] & 0xff;
  ret = (ret << 8) + (tmp[1] & 0xff);
  ret = (ret << 8) + (tmp[2] & 0xff);
  ret = (ret << 8) + (tmp[3] & 0xff);
  ret = (ret << 8) + (tmp[4] & 0xff);
  ret = (ret << 8) + (tmp[5] & 0xff);
  ret = (ret << 8) + (tmp[6] & 0xff);
  ret = (ret << 8) + (tmp[7] & 0xff);

  return ret;
}

void put_uint16(void *buf, uint16_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[0] = value & 0xff;
  tmp[1] = (value >>= 8) & 0xff;
}

void put_uint32(void *buf, uint32_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[0] = value & 0xff;
  tmp[1] = (value >>= 8) & 0xff;
  tmp[2] = (value >>= 8) & 0xff;
  tmp[3] = (value >>= 8) & 0xff;
}

void put_uint64(void *buf, uint64_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[0] = value & 0xff;
  tmp[1] = (value >>= 8) & 0xff;
  tmp[2] = (value >>= 8) & 0xff;
  tmp[3] = (value >>= 8) & 0xff;
  tmp[4] = (value >>= 8) & 0xff;
  tmp[5] = (value >>= 8) & 0xff;
  tmp[6] = (value >>= 8) & 0xff;
  tmp[7] = (value >>= 8) & 0xff;
}

/*
 * Character map conversion stuff
 */

typedef struct {
  iconv_t ict_from_utf8, ict_to_utf8;
  char *charset;
} kax_conv_t;

static kax_conv_t *kax_convs = NULL;
static int num_kax_convs = 0;
int cc_local_utf8 = -1;

int add_kax_conv(const char *charset, iconv_t ict_from, iconv_t ict_to) {
  kax_conv_t *c;
  int i;

  for (i = 0; i < num_kax_convs; i++)
    if (!strcmp(kax_convs[i].charset, charset))
      return i;

  kax_convs = (kax_conv_t *)saferealloc(kax_convs, (num_kax_convs + 1) *
                                        sizeof(kax_conv_t));
  c = &kax_convs[num_kax_convs];
  c->charset = safestrdup(charset);
  c->ict_from_utf8 = ict_from;
  c->ict_to_utf8 = ict_to;
  num_kax_convs++;

  return num_kax_convs - 1;
}

int utf8_init(const char *charset) {
  string lc_charset;
  iconv_t ict_from_utf8, ict_to_utf8;
  int i;

  if ((charset == NULL) || (*charset == 0)) {
    setlocale(LC_CTYPE, "");
#if defined(COMP_MINGW)
    lc_charset = "CP" + to_string(GetACP());
#elif defined(SYS_UNIX)
    lc_charset = nl_langinfo(CODESET);
#else
    lc_charset = locale_charset();
#endif
    if ((lc_charset == "UTF8") || (lc_charset == "UTF-8"))
      return -1;
  } else
    lc_charset = charset;

  for (i = 0; i < num_kax_convs; i++)
    if (kax_convs[i].charset == lc_charset)
      return i;

  ict_to_utf8 = iconv_open("UTF-8", lc_charset.c_str());
  if (ict_to_utf8 == (iconv_t)(-1))
    mxwarn("Could not initialize the iconv library for "
           "the conversion from %s to UFT-8. "
           "Some strings will not be converted to UTF-8 and the resulting "
           "Matroska file might not comply with the Matroska specs ("
           "error: %d, %s).\n", lc_charset.c_str(), errno, strerror(errno));

  ict_from_utf8 = iconv_open(lc_charset.c_str(), "UTF-8");
  if (ict_from_utf8 == (iconv_t)(-1))
    mxwarn("Could not initialize the iconv library for "
           "the conversion from UFT-8 to %s. "
           "Some strings cannot be converted from UTF-8 and might be "
           "displayed incorrectly (error: %d, %s).\n", lc_charset.c_str(),
           errno, strerror(errno));

  return add_kax_conv(lc_charset.c_str(), ict_from_utf8, ict_to_utf8);
}

void utf8_done() {
  int i;

  for (i = 0; i < num_kax_convs; i++) {
    if (kax_convs[i].ict_from_utf8 != (iconv_t)(-1))
      iconv_close(kax_convs[i].ict_from_utf8);
    if (kax_convs[i].ict_to_utf8 != (iconv_t)(-1))
      iconv_close(kax_convs[i].ict_to_utf8);
    safefree(kax_convs[i].charset);
  }
  if (kax_convs != NULL)
    safefree(kax_convs);
}

static char *convert_charset(iconv_t ict, const char *src) {
  char *dst, *psrc, *pdst, *srccopy;
  size_t lsrc, ldst;
  int len;

  if (ict == (iconv_t)(-1))
    return safestrdup(src);

  len = strlen(src) * 4;
  dst = (char *)safemalloc(len + 1);
  memset(dst, 0, len + 1);

  iconv(ict, NULL, 0, NULL, 0);      // Reset the iconv state.
  lsrc = len / 4;
  ldst = len;
  srccopy = safestrdup(src);
  psrc = srccopy;
  pdst = dst;
#if defined(COMP_CYGWIN) || defined(COMP_MINGW)
  iconv(ict, (const char **)&psrc, &lsrc, &pdst, &ldst);
#else
  iconv(ict, &psrc, &lsrc, &pdst, &ldst);
#endif
  safefree(srccopy);

  return dst;
}

char *to_utf8(int handle, const char *local) {
  char *copy;

  if (handle == -1) {
    copy = safestrdup(local);
    return copy;
  }

  if (handle >= num_kax_convs)
    die("common.cpp/to_utf8(): Invalid conversion handle %d (num: %d).",
        handle, num_kax_convs);

  return convert_charset(kax_convs[handle].ict_to_utf8, local);
}

string &to_utf8(int handle, string &local) {
  char *cutf8;

  cutf8 = to_utf8(handle, local.c_str());
  local = cutf8;
  safefree(cutf8);

  return local;
}

char *from_utf8(int handle, const char *utf8) {
  char *copy;

  if (handle == -1) {
    copy = safestrdup(utf8);
    return copy;
  }

  if (handle >= num_kax_convs)
    die("common.cpp/from_utf8(): Invalid conversion handle %d (num: %d).",
        handle, num_kax_convs);

  return convert_charset(kax_convs[handle].ict_from_utf8, utf8);
}

string &from_utf8(int handle, string &utf8) {
  char *clocal;

  clocal = from_utf8(handle, utf8.c_str());
  utf8 = clocal;
  safefree(clocal);

  return utf8;
}

/*
 * Random unique uint32_t numbers
 */

static vector<uint32_t> ru_numbers;

void clear_list_of_unique_uint32() {
  ru_numbers.clear();
}

bool is_unique_uint32(uint32_t number) {
  int i;

  for (i = 0; i < ru_numbers.size(); i++)
    if (ru_numbers[i] == number)
      return false;

  return true;
}

void add_unique_uint32(uint32_t number) {
  ru_numbers.push_back(number);
}

uint32_t create_unique_uint32() {
  uint32_t rnumber, half;

  do {
    half = (uint32_t)(65535.0 * rand() / RAND_MAX);
    rnumber = half;
    half = (uint32_t)(65535.0 * rand() / RAND_MAX);
    rnumber |= (half << 16);
  } while ((rnumber == 0) || !is_unique_uint32(rnumber));
  add_unique_uint32(rnumber);

  return rnumber;
}

/*
 * Miscellaneous stuff
 */

char *_safestrdup(const char *s, const char *file, int line) {
  char *copy;

  if (s == NULL)
    return NULL;

  copy = strdup(s);
  if (copy == NULL)
    die("common.cpp/safestrdup() called from file %s, line %d: strdup() "
        "returned NULL for '%s'.", file, line, s);

  return copy;
}

unsigned char *_safestrdup(const unsigned char *s, const char *file,
                           int line) {
  char *copy;

  if (s == NULL)
    return NULL;

  copy = strdup((const char *)s);
  if (copy == NULL)
    die("common.cpp/safestrdup() called from file %s, line %d: strdup() "
        "returned NULL for '%s'.", file, line, s);

  return (unsigned char *)copy;
}

void *_safememdup(const void *s, size_t size, const char *file, int line) {
  void *copy;

  if (s == NULL)
    return NULL;

  copy = malloc(size);
  if (copy == NULL)
    die("common.cpp/safememdup() called from file %s, line %d: malloc() "
        "returned NULL for a size of %d bytes.", file, line, size);
  memcpy(copy, s, size);

  return copy;
}

void *_safemalloc(size_t size, const char *file, int line) {
  void *mem;

  mem = malloc(size);
  if (mem == NULL)
    die("common.cpp/safemalloc() called from file %s, line %d: malloc() "
        "returned NULL for a size of %d bytes.", file, line, size);

  return mem;
}

void *_saferealloc(void *mem, size_t size, const char *file, int line) {
  mem = realloc(mem, size);
  if (mem == NULL)
    die("common.cpp/saferealloc() called from file %s, line %d: realloc() "
        "returned NULL for a size of %d bytes.", file, line, size);

  return mem;
}

/*
 * UTFstring <-> C string conversion
 */

UTFstring cstr_to_UTFstring(const char *c) {
#if defined(COMP_MSC) || defined(COMP_MINGW)
  wchar_t *new_string;
  int len;
  UTFstring u;

  len = strlen(c);
  new_string = (wchar_t *)safemalloc((len + 1) * sizeof(wchar_t));
  MultiByteToWideChar(CP_ACP, 0, c, -1, new_string, len + 1);

  u = new_string;
  safefree(new_string);

  return u;
#else
  wchar_t *new_string;
  char *old_locale;
  UTFstring u;
  int len;

  len = strlen(c);
  new_string = (wchar_t *)safemalloc((len + 1) * sizeof(wchar_t));
  memset(new_string, 0, (len + 1) * sizeof(wchar_t));
  new_string[len] = L'\0';
  old_locale = safestrdup(setlocale(LC_CTYPE, NULL));
  setlocale(LC_CTYPE, "");
  mbstowcs(new_string, c, len);
  setlocale(LC_CTYPE, old_locale);
  safefree(old_locale);
  u = UTFstring(new_string);
  safefree(new_string);

  return u;
#endif
}

static int utf8_byte_length(unsigned char c) {
  if (c < 0x80)                 // 0xxxxxxx
    return 1;
  else if (c < 0xc0)            // 10xxxxxx
    die("cstrutf8_to_UTFstring: Invalid UTF-8 sequence encountered. Please "
        "contact moritz@bunkus.org and request that he implements a better "
        "UTF-8 parser.");
  else if (c < 0xe0)            // 110xxxxx
    return 2;
  else if (c < 0xf0)            // 1110xxxx
    return 3;
  else if (c < 0xf8)            // 11110xxx
    return 4;
  else if (c < 0xfc)            // 111110xx
    return 5;
  else if (c < 0xfe)            // 1111110x
    return 6;
  else
    die("cstrutf8_to_UTFstring: Invalid UTF-8 sequence encountered. Please "
        "contact moritz@bunkus.org and request that he implements a better "
        "UTF-8 parser.");

  return 0;
}

static int wchar_to_utf8_byte_length(uint32_t w) {
  if (w < 0x00000080)
    return 1;
  else if (w < 0x00000800)
    return 2;
  else if (w < 0x00010000)
    return 3;
  else if (w < 0x00200000)
    return 4;
  else if (w < 0x04000000)
    return 5;
  else if (w < 0x80000000)
    return 6;
  else
    die("UTFstring_to_cstrutf8: Invalid wide character. Please contact "
        "moritz@bunkus.org if you think that this is not true.");

  return 0;
}

UTFstring cstrutf8_to_UTFstring(const char *c) {
  wchar_t *new_string;
  int slen, dlen, src, dst, clen;
  UTFstring u;

  slen = strlen(c);
  dlen = 0;
  for (src = 0; src < slen; dlen++)
    src += utf8_byte_length(c[src]);

  new_string = (wchar_t *)safemalloc((dlen + 1) * sizeof(wchar_t));
  for (src = 0, dst = 0; src < slen; dst++) {
    clen = utf8_byte_length(c[src]);
    if ((src + clen) > slen)
      die("cstrutf8_to_UTFstring: Invalid UTF-8 sequence encountered. Please "
          "contact moritz@bunkus.org and request that he implements a better "
          "UTF-8 parser.");

    if (clen == 1)
      new_string[dst] = c[src];

    else if (clen == 2)
      new_string[dst] =
        ((((uint32_t)c[src]) & 0x1f) << 6) |
        (((uint32_t)c[src + 1]) & 0x3f);
    else if (clen == 3)
      new_string[dst] =
        ((((uint32_t)c[src]) & 0x0f) << 12) |
        ((((uint32_t)c[src + 1]) & 0x3f) << 6) |
        (((uint32_t)c[src + 2]) & 0x3f);
    else if (clen == 4)
      new_string[dst] =
        ((((uint32_t)c[src]) & 0x07) << 18) |
        ((((uint32_t)c[src + 1]) & 0x3f) << 12) |
        ((((uint32_t)c[src + 2]) & 0x3f) << 6) |
        (((uint32_t)c[src + 3]) & 0x3f);
    else if (clen == 5)
      new_string[dst] =
        ((((uint32_t)c[src]) & 0x07) << 24) |
        ((((uint32_t)c[src + 1]) & 0x3f) << 18) |
        ((((uint32_t)c[src + 2]) & 0x3f) << 12) |
        ((((uint32_t)c[src + 3]) & 0x3f) << 6) |
        (((uint32_t)c[src + 4]) & 0x3f);
    else if (clen == 6)
      new_string[dst] =
        ((((uint32_t)c[src]) & 0x07) << 30) |
        ((((uint32_t)c[src + 1]) & 0x3f) << 24) |
        ((((uint32_t)c[src + 2]) & 0x3f) << 18) |
        ((((uint32_t)c[src + 3]) & 0x3f) << 12) |
        ((((uint32_t)c[src + 4]) & 0x3f) << 6) |
        (((uint32_t)c[src + 5]) & 0x3f);

    src += clen;
  }
  new_string[dst] = 0;

  u = UTFstring(new_string);
  safefree(new_string);

  return u;
}

char *UTFstring_to_cstr(const UTFstring &u) {
#if defined(COMP_MSC) || defined(COMP_MINGW)
  char *new_string;
  int len;
  BOOL dummy;

  len = u.length();
  new_string = (char *)safemalloc(len + 1);
  WideCharToMultiByte(CP_ACP, 0, u.c_str(), -1, new_string, len + 1, " ",
                      &dummy);

  return new_string;
#else
  const wchar_t *sptr;
  char *new_string, *old_locale;
  int len;

  len = u.length();
  new_string = (char *)safemalloc(len * 4 + 1);
  memset(new_string, 0, len * 4 + 1);
  sptr = u.c_str();
  old_locale = safestrdup(setlocale(LC_CTYPE, NULL));
  setlocale(LC_CTYPE, "");
  wcsrtombs(new_string, &sptr, len, NULL);
  setlocale(LC_CTYPE, old_locale);
  safefree(old_locale);

  return new_string;
#endif
}

char *UTFstring_to_cstrutf8(const UTFstring &u) {
  int src, dst, dlen, slen, clen;
  unsigned char *new_string;
  uint32_t uc;

  dlen = 0;
  slen = u.length();

  for (src = 0, dlen = 0; src < slen; src++)
    dlen += wchar_to_utf8_byte_length((uint32_t)u[src]);

  new_string = (unsigned char *)malloc(dlen + 1);

  for (src = 0, dst = 0; src < slen; src++) {
    uc = (uint32_t)u[src];
    clen = wchar_to_utf8_byte_length(uc);

    if (clen == 1)
      new_string[dst] = (unsigned char)uc;

    else if (clen == 2) {
      new_string[dst]     = 0xc0 | ((uc >> 6) & 0x0000001f);
      new_string[dst + 1] = 0x80 | (uc & 0x0000003f);

    } else if (clen == 3) {
      new_string[dst]     = 0xe0 | ((uc >> 12) & 0x0000000f);
      new_string[dst + 1] = 0x80 | ((uc >> 6) & 0x0000003f);
      new_string[dst + 2] = 0x80 | (uc & 0x0000003f);

    } else if (clen == 4) {
      new_string[dst]     = 0xf0 | ((uc >> 18) & 0x00000007);
      new_string[dst + 1] = 0x80 | ((uc >> 12) & 0x0000003f);
      new_string[dst + 2] = 0x80 | ((uc >> 6) & 0x0000003f);
      new_string[dst + 3] = 0x80 | (uc & 0x0000003f);

    } else if (clen == 5) {
      new_string[dst]     = 0xf8 | ((uc >> 24) & 0x00000003);
      new_string[dst + 1] = 0x80 | ((uc >> 18) & 0x0000003f);
      new_string[dst + 2] = 0x80 | ((uc >> 12) & 0x0000003f);
      new_string[dst + 3] = 0x80 | ((uc >> 6) & 0x0000003f);
      new_string[dst + 4] = 0x80 | (uc & 0x0000003f);

    } else {
      new_string[dst]     = 0xfc | ((uc >> 30) & 0x00000001);
      new_string[dst + 1] = 0x80 | ((uc >> 24) & 0x0000003f);
      new_string[dst + 2] = 0x80 | ((uc >> 18) & 0x0000003f);
      new_string[dst + 3] = 0x80 | ((uc >> 12) & 0x0000003f);
      new_string[dst + 4] = 0x80 | ((uc >> 6) & 0x0000003f);
      new_string[dst + 5] = 0x80 | (uc & 0x0000003f);

    }

    dst += clen;
  }

  new_string[dst] = 0;

  return (char *)new_string;
}

vector<string> split(const char *src, const char *pattern, int max_num) {
  int num, i, plen;
  char *copy, *p1, *p2;
  vector<string> v;

  plen = strlen(pattern);
  copy = safestrdup(src);
  p2 = copy;
  p1 = strstr(p2, pattern);
  num = 1;
  while ((p1 != NULL) && ((max_num == -1) || (num < max_num))) {
    for (i = 0; i < plen; i++)
      p1[i] = 0;
    v.push_back(string(p2));
    p2 = &p1[plen];
    p1 = strstr(p2, pattern);
    num++;
  }
  if (*p2 != 0)
    v.push_back(string(p2));
  safefree(copy);

  return v;
}

void strip(string &s, bool newlines) {
  int i, len;
  const char *c;

  c = s.c_str();
  i = 0;
  if (newlines)
    while ((c[i] != 0) && (isblanktab(c[i]) || iscr(c[i])))
      i++;
  else
    while ((c[i] != 0) && isblanktab(c[i]))
      i++;

  if (i > 0)
    s.erase(0, i);

  c = s.c_str();
  len = s.length();
  i = 0;

  if (newlines)
    while ((i < len) && (isblanktab(c[len - i - 1]) || iscr(c[len - i - 1])))
      i++;
  else
    while ((i < len) && isblanktab(c[len - i - 1]))
      i++;

  if (i > 0)
    s.erase(len - i, i);
}

void strip(vector<string> &v, bool newlines) {
  int i;

  for (i = 0; i < v.size(); i++)
    strip(v[i], newlines);
}

/*
 * Integer parsing
 */

bool parse_int(const char *s, int64_t &value) {
  const char *p;
  int sign;

  if (*s == 0)
    return false;

  sign = 1;
  value = 0;
  p = s;
  if (*p == '-') {
    sign = -1;
    p++;
  }
  while (*p != 0) {
    if (!isdigit(*p))
      return false;
    value *= 10;
    value += *p - '0';
    p++;
  }
  value *= sign;

  return true;
}

bool parse_int(const char *s, int &value) {
  int64_t tmp;
  bool result;

  result = parse_int(s, tmp);
  value = tmp;

  return result;
}

string to_string(int64_t i) {
  char buf[100];

  sprintf(buf, "%lld", i);

  return string(buf);
}

#ifdef DEBUG
/*
 * debugging stuff
 */

debug_c debug_facility;

debug_c::debug_c() {
}

debug_c::~debug_c() {
  while (entries.size() > 0) {
    safefree(entries[entries.size() - 1]);
    entries.pop_back();
  }
}

void debug_c::enter(const char *label) {
  int i;
  debug_data_t *entry;
#if defined(SYS_UNIX) || defined(COMP_CYGWIN)
  struct timeval tv;
#endif

  entry = NULL;
  for (i = 0; i < entries.size(); i++)
    if (!strcmp(entries[i]->label, label)) {
      entry = entries[i];
      break;
    }

  if (entry == NULL) {
    entry = (debug_data_t *)safemalloc(sizeof(debug_data_t));
    entry->label = label;
    entries.push_back(entry);
  }


#if defined(SYS_UNIX) || defined(COMP_CYGWIN)
  gettimeofday(&tv, NULL);
  entry->entered_at = (uint64_t)tv.tv_sec * (uint64_t)1000000 +
    tv.tv_usec;
#else
  entry->entered_at = (uint64_t)time(NULL) * (uint64_t)1000000;
#endif
}

void debug_c::leave(const char *label) {
  int i;
  debug_data_t *entry;
#if defined(SYS_UNIX) || defined(COMP_CYGWIN)
  struct timeval tv;

  gettimeofday(&tv, NULL);
#else
  time_t now;

  now = time(NULL);
#endif

  entry = NULL;
  for (i = 0; i < entries.size(); i++)
    if (!strcmp(entries[i]->label, label)) {
      entry = entries[i];
      break;
    }

  if ((entry == NULL) || (entry->entered_at == 0))
    die("common.cpp/debug_c::leave() leave without enter: %s", label);

  entry->number_of_calls++;
#if defined(SYS_UNIX) || defined(COMP_CYGWIN)
  entry->elapsed_time += (uint64_t)tv.tv_sec * (uint64_t)1000000 +
    tv.tv_usec - entry->entered_at;
#else
  entry->elapsed_time += (uint64_t)now * (uint64_t)1000000;
#endif
  entry->entered_at = 0;
}

void debug_c::add_packetizer(void *ptzr) {
  int i;

  for (i = 0; i < packetizers.size(); i++)
    if (packetizers[i] == ptzr)
      return;

  packetizers.push_back((generic_packetizer_c *)ptzr);
}

void debug_c::dump_info() {
  int i;
#if defined SYS_UNIX
  debug_data_t *entry;
  uint64_t diff_calls, diff_time;

  mxprint(stdout, "\nDBG> dumping time info:\n");
  for (i = 0; i < entries.size(); i++) {
    entry = entries[i];
    mxprint(stdout, "DBG> function: %s, # calls: %llu, elapsed time: %.3fs, "
            "time/call: %.3fms", entry->label, entry->number_of_calls,
            entry->elapsed_time / 1000000.0,
            entry->elapsed_time / (float)entry->number_of_calls / 1000.0);
    diff_calls = entry->number_of_calls - entry->last_number_of_calls;
    diff_time = entry->elapsed_time - entry->last_elapsed_time;
    if ((entry->last_elapsed_time != 0) &&
        (entry->last_number_of_calls != 0) &&
        (diff_calls > 0)) {
      mxprint(stdout, ", since the last call: # calls: %llu, elapsed time: "
              "%.3fs, time/call: %.3fms", diff_calls, diff_time / 1000000.0,
              diff_time / (float)diff_calls / 1000.0);
    }
    mxprint(stdout, "\n");
    entry->last_elapsed_time = entry->elapsed_time;
    entry->last_number_of_calls = entry->number_of_calls;
  }
#endif // defined SYS_UNIX

  mxprint(stdout, "DBG> dumping packetzer info:\n");
  for (i = 0; i < packetizers.size(); i++)
    packetizers[i]->dump_debug_info();
}

void debug_c::dump_elements(EbmlElement *e, int level) {
  int i;
  EbmlMaster *m;

  for (i = 0; i < level; i++)
    mxprint(stdout, " ");
  mxprint(stdout, "%s", e->Generic().DebugName);

  if ((m = dynamic_cast<EbmlMaster *>(e)) != NULL) {
    mxprint(stdout, " (size: %u)\n", m->ListSize());
    for (i = 0; i < m->ListSize(); i++)
      dump_elements((*m)[i], level + 1);
  } else
    mxprint(stdout, "\n");
}

#endif // DEBUG

/*
 * Other related news
 */

void fix_format(const char *fmt, string &new_fmt) {
#if defined(COMP_MINGW) || defined(COMP_MSC)
  int i, len;
  bool state;

  new_fmt = "";
  len = strlen(fmt);
  state = false;
  for (i = 0; i < len; i++) {
    if (fmt[i] == '%') {
      state = !state;
      new_fmt += '%';

    } else if (!state)
      new_fmt += fmt[i];

    else {
      if (((i + 3) <= len) && (fmt[i] == 'l') && (fmt[i + 1] == 'l') &&
          ((fmt[i + 2] == 'u') || (fmt[i + 2] == 'd'))) {
        new_fmt += "I64";
        new_fmt += fmt[i + 2];
        i += 2;
        state = false;

      } else {
        new_fmt += fmt[i];
        if (isalpha(fmt[i]))
          state = false;
      }
    }
  }

#else

  new_fmt = fmt;

#endif
}

void mxprint(void *stream, const char *fmt, ...) {
  va_list ap;
  string new_fmt;

  fix_format(fmt, new_fmt);
  va_start(ap, fmt);
  vfprintf((FILE *)stream, new_fmt.c_str(), ap);
  fflush((FILE *)stream);
  va_end(ap);
}

void mxprints(char *dst, const char *fmt, ...) {
  va_list ap;
  string new_fmt;

  fix_format(fmt, new_fmt);
  va_start(ap, fmt);
  vsprintf(dst, new_fmt.c_str(), ap);
  va_end(ap);
}

static void mxmsg(int level, const char *fmt, va_list &ap) {
  string new_fmt;
  bool nl;
  FILE *stream;
  char *prefix;

  fix_format(fmt, new_fmt);

  if (new_fmt[0] == '\n') {
    nl = true;
    new_fmt.erase(0, 1);
  } else
    nl = false;

  stream = stdout;
  prefix = NULL;

  if (level == MXMSG_ERROR)
    prefix = "Error: ";
  else if (level == MXMSG_WARNING)
    prefix = "Warning: ";
  else if (level == MXMSG_DEBUG)
    prefix = "DBG> ";

  if (nl)
    fprintf(stream, "\n");

  if (prefix != NULL)
    fprintf(stream, prefix);

  vfprintf(stream, new_fmt.c_str(), ap);
  fflush(stream);
}

void mxverb(int level, const char *fmt, ...) {
  va_list ap;

  if (verbose < level)
    return;

  va_start(ap, fmt);
  mxmsg(MXMSG_INFO, fmt, ap);
  va_end(ap);
}

void mxinfo(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  mxmsg(MXMSG_INFO, fmt, ap);
  va_end(ap);
}

static bool warning_issued = false;

void mxwarn(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  mxmsg(MXMSG_WARNING, fmt, ap);
  va_end(ap);

  warning_issued = true;
}

void mxerror(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  mxmsg(MXMSG_ERROR, fmt, ap);
  va_end(ap);

  exit(2);
}

void mxdebug(const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  mxmsg(MXMSG_DEBUG, fmt, ap);
  va_end(ap);
}

void mxexit(int code) {
  if (code != -1)
    exit(code);

  if (warning_issued)
    exit(1);

  exit(0);
}

int get_arg_len(const char *fmt, ...) {
  int size;
  va_list ap;

  va_start(ap, fmt);
  size = get_varg_len(fmt, ap);
  va_end(ap);

  return size;
}

int get_varg_len(const char *fmt, va_list ap) {
  int size, result;
  char *dst;

  size = 1024;
  dst = (char *)safemalloc(size);
  while (1) {
    result = vsnprintf(dst, size - 1, fmt, ap);
    if (result >= 0) {
      safefree(dst);
      return result;
    }
    size += 1024;
    dst = (char *)saferealloc(dst, size);
  }
  safefree(dst);

  return -1;
}
