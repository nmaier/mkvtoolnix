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

#include "../os.h"

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

/*
 * Control functions
 */

void die(const char *fmt, ...) {
  va_list ap;

  mxprint(stderr, "'die' called: ");
  va_start(ap, fmt);
  vfprintf(stderr, fmt, ap);
  va_end(ap);
  mxprint(stderr, "\n");
#ifdef DEBUG
  debug_c::dump_info();
#endif
  exit(1);
}

void _trace(const char *func, const char *file, int line) {
  mxprint(stdout, "trace: %s:%s (%d)\n", file, func, line);
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
} mkv_conv_t;

static mkv_conv_t *mkv_convs = NULL;
static int num_mkv_convs = 0;
int cc_local_utf8 = -1;

int add_mkv_conv(const char *charset, iconv_t ict_from, iconv_t ict_to) {
  mkv_conv_t *c;
  int i;

  for (i = 0; i < num_mkv_convs; i++)
    if (!strcmp(mkv_convs[i].charset, charset))
      return i;

  mkv_convs = (mkv_conv_t *)saferealloc(mkv_convs, (num_mkv_convs + 1) *
                                        sizeof(mkv_conv_t));
  c = &mkv_convs[num_mkv_convs];
  c->charset = safestrdup(charset);
  c->ict_from_utf8 = ict_from;
  c->ict_to_utf8 = ict_to;
  num_mkv_convs++;

  return num_mkv_convs - 1;
}

int utf8_init(const char *charset) {
  const char *lc_charset;
  iconv_t ict_from_utf8, ict_to_utf8;
  int i;

  if ((charset == NULL) || (*charset == 0)) {
    setlocale(LC_CTYPE, "");
#if defined(COMP_MINGW)
    lc_charset = "US-ASCII";
#elif defined(SYS_UNIX)
    lc_charset = nl_langinfo(CODESET);
#else
    lc_charset = (char *)locale_charset();
#endif
    if (!strcmp(lc_charset, "UTF8") || !strcmp(lc_charset, "UTF-8"))
      return -1;
  } else
    lc_charset = charset;

  for (i = 0; i < num_mkv_convs; i++)
    if (!strcmp(mkv_convs[i].charset, lc_charset))
      return i;

  ict_to_utf8 = iconv_open("UTF-8", lc_charset);
  if (ict_to_utf8 == (iconv_t)(-1))
    mxprint(stdout, "Warning: Could not initialize the iconv library for "
            "the conversion from %s to UFT-8. "
            "Some strings will not be converted to UTF-8 and the resulting "
            "Matroska file might not comply with the Matroska specs ("
            "error: %d, %s).\n", lc_charset, errno, strerror(errno));

  ict_from_utf8 = iconv_open(lc_charset, "UTF-8");
  if (ict_from_utf8 == (iconv_t)(-1))
    mxprint(stdout, "Warning: Could not initialize the iconv library for "
            "the conversion from UFT-8 to %s. "
            "Some strings cannot be converted from UTF-8 and might be "
            "displayed incorrectly (error: %d, %s).\n", lc_charset, errno,
            strerror(errno));

  return add_mkv_conv(lc_charset, ict_from_utf8, ict_to_utf8);
}

void utf8_done() {
  int i;

  for (i = 0; i < num_mkv_convs; i++) {
    if (mkv_convs[i].ict_from_utf8 != (iconv_t)(-1))
      iconv_close(mkv_convs[i].ict_from_utf8);
    if (mkv_convs[i].ict_to_utf8 != (iconv_t)(-1))
      iconv_close(mkv_convs[i].ict_to_utf8);
    safefree(mkv_convs[i].charset);
  }
  if (mkv_convs != NULL)
    safefree(mkv_convs);
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

  if (handle >= num_mkv_convs)
    die("common.cpp/to_utf8(): Invalid conversion handle %d (num: %d).",
        handle, num_mkv_convs);

  return convert_charset(mkv_convs[handle].ict_to_utf8, local);
}

char *from_utf8(int handle, const char *utf8) {
  char *copy;

  if (handle == -1) {
    copy = safestrdup(utf8);
    return copy;
  }

  if (handle >= num_mkv_convs)
    die("common.cpp/from_utf8(): Invalid conversion handle %d (num: %d).",
        handle, num_mkv_convs);

  return convert_charset(mkv_convs[handle].ict_from_utf8, utf8);
}

/*
 * Random unique uint32_t numbers
 */

static uint32_t *ru_numbers = NULL;
static int num_ru_numbers = 0;

int is_unique_uint32(uint32_t number) {
  int i;

  for (i = 0; i < num_ru_numbers; i++)
    if (ru_numbers[i] == number)
      return 0;

  return 1;
}

void add_unique_uint32(uint32_t number) {
  ru_numbers = (uint32_t *)saferealloc(ru_numbers, (num_ru_numbers + 1) *
                                       sizeof(uint32_t));

  ru_numbers[num_ru_numbers] = number;
  num_ru_numbers++;
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

char *UTFstring_to_cstr(const UTFstring &u) {
#if defined(COMP_MSC) || defined(COMP_MINGW)
  char *new_string;
  int len;
  BOOL dummy;

  len = u.length();
  new_string = (char *)safemalloc(len + 1);
  WideCharToMultiByte(CP_ACP, 0, u.c_str(), -1, new_string, len + 1, " ", &dummy);

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

void strip(string &s) {
  int i, len;
  const char *c;

  c = s.c_str();
  i = 0;
  while ((c[i] != 0) && isspace(c[i]))
    i++;

  if (i > 0)
    s.erase(0, i);

  c = s.c_str();
  len = s.length();
  i = 0;

  while ((i < len) && isspace(c[len - i - 1]))
    i++;

  if (i > 0)
    s.erase(len - i, i);
}

void strip(vector<string> &v) {
  int i;

  for (i = 0; i < v.size(); i++)
    strip(v[i]);
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

static vector<debug_c *> dbg_entries;
static vector<generic_packetizer_c *>dbg_packetizers;

debug_c::debug_c(const char *nlabel) {
  elapsed_time = 0;
  number_of_calls = 0;
  last_elapsed_time = 0;
  last_number_of_calls = 0;
  label = nlabel;
}

void debug_c::enter(const char *label) {
  int i;
  debug_c *entry;
#if defined(SYS_UNIX) || defined(COMP_CYGWIN)
  struct timeval tv;
#endif

  entry = NULL;
  for (i = 0; i < dbg_entries.size(); i++)
    if (!strcmp(dbg_entries[i]->label, label)) {
      entry = dbg_entries[i];
      break;
    }

  if (entry == NULL) {
    entry = new debug_c(label);
    dbg_entries.push_back(entry);
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
  debug_c *entry;
#if defined(SYS_UNIX) || defined(COMP_CYGWIN)
  struct timeval tv;

  gettimeofday(&tv, NULL);
#else
  time_t now;

  now = time(NULL);
#endif

  entry = NULL;
  for (i = 0; i < dbg_entries.size(); i++)
    if (!strcmp(dbg_entries[i]->label, label)) {
      entry = dbg_entries[i];
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

  for (i = 0; i < dbg_packetizers.size(); i++)
    if (dbg_packetizers[i] == ptzr)
      return;

  dbg_packetizers.push_back((generic_packetizer_c *)ptzr);
}

void debug_c::dump_info() {
  int i;
  debug_c *entry;
  uint64_t diff_calls, diff_time;

  mxprint(stderr, "\nDBG> dumping time info:\n");
  for (i = 0; i < dbg_entries.size(); i++) {
    entry = dbg_entries[i];
    mxprint(stderr, "DBG> function: %s, # calls: %llu, elapsed time: %.3fs, "
            "time/call: %.3fms", entry->label, entry->number_of_calls,
            entry->elapsed_time / 1000000.0,
            entry->elapsed_time / (float)entry->number_of_calls / 1000.0);
    diff_calls = entry->number_of_calls - entry->last_number_of_calls;
    diff_time = entry->elapsed_time - entry->last_elapsed_time;
    if ((entry->last_elapsed_time != 0) &&
        (entry->last_number_of_calls != 0) &&
        (diff_calls > 0)) {
      mxprint(stderr, ", since the last call: # calls: %llu, elapsed time: "
              "%.3fs, time/call: %.3fms", diff_calls, diff_time / 1000000.0,
              diff_time / (float)diff_calls / 1000.0);
    }
    mxprint(stderr, "\n");
    entry->last_elapsed_time = entry->elapsed_time;
    entry->last_number_of_calls = entry->number_of_calls;
  }

  mxprint(stderr, "DBG> dumping packetzer info:\n");
  for (i = 0; i < dbg_packetizers.size(); i++)
    dbg_packetizers[i]->dump_debug_info();
}

#endif // DEBUG

/*
 * Other related news
 */
void mxprint(void *stream, const char *fmt, ...) {
  va_list ap;

  va_start(ap, fmt);
  vfprintf((FILE *)stream, fmt, ap);
  fflush((FILE *)stream);
  va_end(ap);
}
