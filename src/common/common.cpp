
/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * helper functions, common variables
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include "os.h"

#include <assert.h>
#include <ctype.h>
#include <errno.h>
#include <iconv.h>
#if defined(COMP_MSC)
#include <libcharset.h>
#elif !defined(COMP_MINGW)
#include <langinfo.h>
#endif
#include <locale.h>
#include <stdarg.h>
#if defined(COMP_GCC) && (__GNUC__ < 3)
# define __USE_ISOC99
# include <stdio.h>
# undef __USE_ISOC99
#else
# include <stdio.h>
#endif // COMP_GCC && __GNUC__ < 3
#include <stdlib.h>
#include <string.h>
#if !defined(COMP_MSC)
#include <sys/time.h>
#endif
#include <time.h>
#include <wchar.h>
#if defined(SYS_WINDOWS)
#include <windows.h>
#endif

#include <string>
#include <vector>

using namespace std;

#include "base64.h"
#include "common.h"
#include "hacks.h"

int verbose = 1;
bool suppress_warnings = false;

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

bitvalue_c::bitvalue_c(string s,
                       int allowed_bitlength) {
  int len, i;
  string s2;

  len = s.size();
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
    s = s2;
    len = s.size();
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

bitvalue_c &
bitvalue_c::operator =(const bitvalue_c &src) {
  safefree(value);
  bitsize = src.bitsize;
  value = (unsigned char *)safememdup(src.value, bitsize / 8);

  return *this;
}

bitvalue_c::~bitvalue_c() {
  safefree(value);
}

bool
bitvalue_c::operator ==(const bitvalue_c &cmp)
  const {
  int i;

  if (cmp.bitsize != bitsize)
    return false;
  for (i = 0; i < bitsize / 8; i++)
    if (value[i] != cmp.value[i])
      return false;
  return true;
}

unsigned char
bitvalue_c::operator [](int index)
  const {
  assert((index >= 0) && (index < (bitsize / 8)));
  return value[index];
}

int
bitvalue_c::size()
  const {
  return bitsize;
}

unsigned char *
bitvalue_c::data()
  const {
  return value;
}

void
bitvalue_c::generate_random() {
  int i;

  for (i = 0; i < bitsize / 8; i++)
    value[i] = (unsigned char)(255.0 * rand() / RAND_MAX);
}

// ---------------------------------------------------------------------

byte_cursor_c::byte_cursor_c(const unsigned char *ndata,
                             int nsize):
  size(nsize),
  data(ndata) {
  pos = 0;
  if (nsize < 0)
    throw exception();
}

unsigned char
byte_cursor_c::get_uint8() {
  if ((pos + 1) > size)
    throw exception();

  pos++;

  return data[pos - 1];
}

unsigned short
byte_cursor_c::get_uint16_be() {
  unsigned short v;

  if ((pos + 2) > size)
    throw exception();

  v = ::get_uint16_be(&data[pos]);
  pos += 2;

  return v;
}

unsigned int
byte_cursor_c::get_uint32_be() {
  unsigned int v;

  if ((pos + 4) > size)
    throw exception();

  v = ::get_uint32_be(&data[pos]);
  pos += 4;

  return v;
}

unsigned short
byte_cursor_c::get_uint16() {
  unsigned short v;

  if ((pos + 2) > size)
    throw exception();

  v = ::get_uint16(&data[pos]);
  pos += 2;

  return v;
}

unsigned int
byte_cursor_c::get_uint32() {
  unsigned int v;

  if ((pos + 4) > size)
    throw exception();

  v = ::get_uint32(&data[pos]);
  pos += 4;

  return v;
}

void
byte_cursor_c::skip(int n) {
  if ((pos + n) > size)
    throw exception();

  pos += n;
}

void
byte_cursor_c::get_bytes(unsigned char *dst,
                         int n) {
  if ((pos + n) > size)
    throw exception();

  memcpy(dst, &data[pos], n);
  pos += n;
}

int
byte_cursor_c::get_pos() {
  return pos;
}

int
byte_cursor_c::get_len() {
  return size - pos;
}

/*
 * Control functions
 */

void
die(const char *fmt,
    ...) {
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

void
_trace(const char *func,
       const char *file,
       int line) {
  mxdebug("trace: %s:%s (%d)\n", file, func, line);
}

/*
 * Endianess stuff
 */

uint16_t
get_uint16(const void *buf) {
  uint16_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[1] & 0xff;
  ret = (ret << 8) + (tmp[0] & 0xff);

  return ret;
}

uint32_t
get_uint24(const void *buf) {
  uint32_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[2] & 0xff;
  ret = (ret << 8) + (tmp[1] & 0xff);
  ret = (ret << 8) + (tmp[0] & 0xff);

  return ret;
}

uint32_t
get_uint32(const void *buf) {
  uint32_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[3] & 0xff;
  ret = (ret << 8) + (tmp[2] & 0xff);
  ret = (ret << 8) + (tmp[1] & 0xff);
  ret = (ret << 8) + (tmp[0] & 0xff);

  return ret;
}

uint64_t
get_uint64(const void *buf) {
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

uint16_t
get_uint16_be(const void *buf) {
  uint16_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[0] & 0xff;
  ret = (ret << 8) + (tmp[1] & 0xff);

  return ret;
}

uint32_t
get_uint24_be(const void *buf) {
  uint32_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[0] & 0xff;
  ret = (ret << 8) + (tmp[1] & 0xff);
  ret = (ret << 8) + (tmp[2] & 0xff);

  return ret;
}

uint32_t
get_uint32_be(const void *buf) {
  uint32_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[0] & 0xff;
  ret = (ret << 8) + (tmp[1] & 0xff);
  ret = (ret << 8) + (tmp[2] & 0xff);
  ret = (ret << 8) + (tmp[3] & 0xff);

  return ret;
}

uint64_t
get_uint64_be(const void *buf) {
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

void
put_uint16(void *buf,
           uint16_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[0] = value & 0xff;
  tmp[1] = (value >>= 8) & 0xff;
}

void
put_uint32(void *buf,
           uint32_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[0] = value & 0xff;
  tmp[1] = (value >>= 8) & 0xff;
  tmp[2] = (value >>= 8) & 0xff;
  tmp[3] = (value >>= 8) & 0xff;
}

void
put_uint64(void *buf,
           uint64_t value) {
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

void
put_uint16_be(void *buf,
              uint16_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[1] = value & 0xff;
  tmp[0] = (value >>= 8) & 0xff;
}

void
put_uint32_be(void *buf,
              uint32_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[3] = value & 0xff;
  tmp[2] = (value >>= 8) & 0xff;
  tmp[1] = (value >>= 8) & 0xff;
  tmp[0] = (value >>= 8) & 0xff;
}

void
put_uint64_be(void *buf,
              uint64_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[7] = value & 0xff;
  tmp[6] = (value >>= 8) & 0xff;
  tmp[5] = (value >>= 8) & 0xff;
  tmp[4] = (value >>= 8) & 0xff;
  tmp[3] = (value >>= 8) & 0xff;
  tmp[2] = (value >>= 8) & 0xff;
  tmp[1] = (value >>= 8) & 0xff;
  tmp[0] = (value >>= 8) & 0xff;
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

int
add_kax_conv(const char *charset,
             iconv_t ict_from,
             iconv_t ict_to) {
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

int
utf8_init(const string &charset) {
  string lc_charset;
  iconv_t ict_from_utf8, ict_to_utf8;
  int i;

  if (charset == "") {
    setlocale(LC_CTYPE, "");
#if defined(COMP_MINGW) || defined(COMP_MSC)
    lc_charset = "CP" + to_string(GetACP());
#elif defined(SYS_SOLARIS)
    lc_charset = nl_langinfo(CODESET);
    if (parse_int(lc_charset, i))
      lc_charset = string("ISO") + lc_charset + string("-US");
#else
    lc_charset = nl_langinfo(CODESET);
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

void
utf8_done() {
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

static char *
convert_charset(iconv_t ict,
                const char *src) {
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
  iconv(ict, (ICONV_CONST char **)&psrc, &lsrc, &pdst, &ldst);
  safefree(srccopy);

  return dst;
}

char *
to_utf8_c(int handle,
          const char *local) {
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

string
to_utf8(int handle,
        const string &local) {
  string s;
  char *cutf8;

  cutf8 = to_utf8_c(handle, local.c_str());
  s = cutf8;
  safefree(cutf8);

  return s;
}

char *
from_utf8_c(int handle,
            const char *utf8) {
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

string
from_utf8(int handle,
          const string &utf8) {
  string s;
  char *clocal;

  clocal = from_utf8_c(handle, utf8.c_str());
  s = clocal;
  safefree(clocal);

  return s;
}

/*
 * Random unique uint32_t numbers
 */

static vector<uint32_t> ru_numbers[4];
static bool random_seeded = false;

void
clear_list_of_unique_uint32(int category) {
  int i;

  assert((category >= -1) && (category <= 3));
  if (category == UNIQUE_ALL_IDS)
    for (i = 0; i < 4; i++)
      clear_list_of_unique_uint32(i);
  else
    ru_numbers[category].clear();
}

bool
is_unique_uint32(uint32_t number,
                 int category) {
  int i;

  assert((category >= 0) && (category <= 3));

  if (hack_engaged(ENGAGE_NO_VARIABLE_DATA))
    return true;
  for (i = 0; i < ru_numbers[category].size(); i++)
    if (ru_numbers[category][i] == number)
      return false;

  return true;
}

void
add_unique_uint32(uint32_t number,
                  int category) {
  assert((category >= 0) && (category <= 3));

  if (hack_engaged(ENGAGE_NO_VARIABLE_DATA))
    ru_numbers[category].push_back(ru_numbers[category].size() + 1);
  else
    ru_numbers[category].push_back(number);
}

bool
remove_unique_uint32(uint32_t number,
                     int category) {
  vector<uint32_t>::iterator dit;

  assert((category >= 0) && (category <= 3));

  if (hack_engaged(ENGAGE_NO_VARIABLE_DATA))
    return true;
  foreach(dit, ru_numbers[category])
    if (*dit == number) {
      ru_numbers[category].erase(dit);
      return true;
    }

  return false;
}

uint32_t
create_unique_uint32(int category) {
  uint32_t rnumber, half;

  assert((category >= 0) && (category <= 3));

  if (hack_engaged(ENGAGE_NO_VARIABLE_DATA)) {
    ru_numbers[category].push_back(ru_numbers[category].size() + 1);
    return ru_numbers[category].size();
  }

  if (!random_seeded) {
    srand(time(NULL));
    random_seeded = true;
  }

  do {
    half = (uint32_t)(65535.0 * rand() / RAND_MAX);
    rnumber = half;
    half = (uint32_t)(65535.0 * rand() / RAND_MAX);
    rnumber |= (half << 16);
  } while ((rnumber == 0) || !is_unique_uint32(rnumber, category));
  add_unique_uint32(rnumber, category);

  return rnumber;
}

/*
 * Miscellaneous stuff
 */

char *
_safestrdup(const char *s,
            const char *file,
            int line) {
  char *copy;

  if (s == NULL)
    return NULL;

  copy = strdup(s);
  if (copy == NULL)
    die("common.cpp/safestrdup() called from file %s, line %d: strdup() "
        "returned NULL for '%s'.", file, line, s);

  return copy;
}

unsigned char *
_safestrdup(const unsigned char *s,
            const char *file,
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

void *
_safememdup(const void *s,
            size_t size,
            const char *file,
            int line) {
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

void *
_safemalloc(size_t size,
            const char *file,
            int line) {
  void *mem;

  mem = malloc(size);
  if (mem == NULL)
    die("common.cpp/safemalloc() called from file %s, line %d: malloc() "
        "returned NULL for a size of %d bytes.", file, line, size);

  return mem;
}

void *
_saferealloc(void *mem,
             size_t size,
             const char *file,
             int line) {
  mem = realloc(mem, size);
  if (mem == NULL)
    die("common.cpp/saferealloc() called from file %s, line %d: realloc() "
        "returned NULL for a size of %d bytes.", file, line, size);

  return mem;
}

/*
 * standard string processing
 */

vector<string>
split(const char *src,
      const char *pattern,
      int max_num) {
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

string
join(const char *pattern,
     vector<string> &strings) {
  string dst;
  uint32_t i;

  if (strings.size() == 0)
    return "";
  dst = strings[0];
  for (i = 1; i < strings.size(); i++) {
    dst += pattern;
    dst += strings[i];
  }

  return dst;
}

void
strip(string &s,
      bool newlines) {
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

void
strip(vector<string> &v,
      bool newlines) {
  int i;

  for (i = 0; i < v.size(); i++)
    strip(v[i], newlines);
}

string
escape(const string &source) {
  string dst;
  string::const_iterator src;

  for (src = source.begin(); src < source.end(); src++) {
    if (*src == '\\')
      dst += "\\\\";
    else if (*src == '"')
      dst += "\\2";               // Yes, this IS a trick ;)
    else if (*src == ' ')
      dst += "\\s";
    else
      dst += *src;
  }

  return dst;
}

string
unescape(const string &source) {
  string dst;
  string::const_iterator src, next_char;

  src = source.begin();
  next_char = src + 1;
  while (src != source.end()) {
    if (*src == '\\') {
      if (next_char == source.end()) // This is an error...
        dst += '\\';
      else {
        if (*next_char == '2')
          dst += '"';
        else if (*next_char == 's')
          dst += ' ';
        else
          dst += *next_char;
        src++;
      }
    } else
      dst += *src;
    src++;
    next_char = src + 1;
  }

  return dst;
}

string
escape_xml(const string &source) {
  string dst;
  string::const_iterator src;

  src = source.begin();
  while (src != source.end()) {
    if (*src == '&')
      dst += "&amp;";
    else if (*src == '>')
      dst += "&gt;";
    else if (*src == '<')
      dst += "&lt;";
    else
      dst += *src;
    src++;
  }

  return dst;
}

bool
starts_with(const string &s,
            const char *start,
            int maxlen) {
  int len, slen;

  slen = strlen(start);
  if (maxlen == -1)
    len = slen;
  else
    len = maxlen < slen ? maxlen : slen;

  return strncmp(s.c_str(), start, slen) == 0;
}

bool
starts_with_case(const string &s,
                 const char *start,
                 int maxlen) {
  int len, slen;

  slen = strlen(start);
  if (maxlen == -1)
    len = slen;
  else
    len = maxlen < slen ? maxlen : slen;

  return strncasecmp(s.c_str(), start, slen) == 0;
}

string
upcase(const string &s) {
  string dst;
  int i;

  dst.reserve(s.size());
  for (i = 0; i < s.size(); i++)
    dst += toupper(s[i]);

  return dst;
}

string
downcase(const string &s) {
  string dst;
  int i;

  dst.reserve(s.size());
  for (i = 0; i < s.size(); i++)
    dst += tolower(s[i]);

  return dst;
}

/*
 * Integer parsing
 */

uint32_t
round_to_nearest_pow2(uint32_t value) {
  int64_t best_value, test_value;

  best_value = 0;
  test_value = 1;
  while (test_value <= 0x80000000ul) {
    if (iabs(value - test_value) < iabs(value - best_value))
      best_value = test_value;
    test_value <<= 1;
  }

  return best_value;
}

bool
parse_int(const char *s,
          int64_t &value) {
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

bool
parse_int(const char *s,
          int &value) {
  int64_t tmp;
  bool result;

  result = parse_int(s, tmp);
  value = tmp;

  return result;
}

bool
parse_double(const char *s,
             double &value) {
  char *endptr;
  string old_locale;
  bool ok;

  old_locale = setlocale(LC_NUMERIC, "C");

  ok = true;
  value = strtod(s, &endptr);
  if (endptr != NULL) {
    if ((value == 0.0) && (endptr == s))
      ok = false;
    else if (*endptr != 0)
      ok = false;
  }
  if (errno == ERANGE)
    ok = false;

  setlocale(LC_NUMERIC, old_locale.c_str());

  return ok;
}

string
to_string(int64_t i) {
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

void
debug_c::enter(const char *label) {
  int i;
  debug_data_t *entry;
#if defined(SYS_UNIX) || defined(COMP_CYGWIN) || defined(SYS_APPLE)
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
    memset(entry, 0, sizeof(debug_data_t));
    entry->label = label;
    entries.push_back(entry);
  }


#if defined(SYS_UNIX) || defined(COMP_CYGWIN) || defined(SYS_APPLE)
  gettimeofday(&tv, NULL);
  entry->entered_at = (uint64_t)tv.tv_sec * (uint64_t)1000000 +
    tv.tv_usec;
#else
  entry->entered_at = (uint64_t)time(NULL) * (uint64_t)1000000;
#endif
}

void
debug_c::leave(const char *label) {
  int i;
  debug_data_t *entry;
#if defined(SYS_UNIX) || defined(COMP_CYGWIN) || defined(SYS_APPLE)
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
#if defined(SYS_UNIX) || defined(COMP_CYGWIN) || defined(SYS_APPLE)
  entry->elapsed_time += (uint64_t)tv.tv_sec * (uint64_t)1000000 +
    tv.tv_usec - entry->entered_at;
#else
  entry->elapsed_time += (uint64_t)now * (uint64_t)1000000;
#endif
  entry->entered_at = 0;
}

void
debug_c::add_packetizer(void *ptzr) {
  int i;

  for (i = 0; i < packetizers.size(); i++)
    if (packetizers[i] == ptzr)
      return;

  packetizers.push_back((generic_packetizer_c *)ptzr);
}

void
debug_c::dump_info() {
  int i;
#if defined(SYS_UNIX) || defined(SYS_APPLE)
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
}

#endif // DEBUG

/*
 * Other related news
 */

void
fix_format(const char *fmt,
           string &new_fmt) {
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

void
mxprint(void *stream,
        const char *fmt,
        ...) {
  va_list ap;
  string new_fmt;

  fix_format(fmt, new_fmt);
  va_start(ap, fmt);
  vfprintf((FILE *)stream, new_fmt.c_str(), ap);
  fflush((FILE *)stream);
  va_end(ap);
}

void
mxprints(char *dst,
         const char *fmt,
         ...) {
  va_list ap;
  string new_fmt;

  fix_format(fmt, new_fmt);
  va_start(ap, fmt);
  vsprintf(dst, new_fmt.c_str(), ap);
  va_end(ap);
}

static void
mxmsg(int level,
      const char *fmt,
      va_list &ap) {
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

void
mxverb(int level,
       const char *fmt,
       ...) {
  va_list ap;

  if (verbose < level)
    return;

  va_start(ap, fmt);
  mxmsg(MXMSG_INFO, fmt, ap);
  va_end(ap);
}

void
mxinfo(const char *fmt,
       ...) {
  va_list ap;

  va_start(ap, fmt);
  mxmsg(MXMSG_INFO, fmt, ap);
  va_end(ap);
}

static bool warning_issued = false;

void
mxwarn(const char *fmt,
       ...) {
  va_list ap;

  if (suppress_warnings)
    return;
  va_start(ap, fmt);
  mxmsg(MXMSG_WARNING, fmt, ap);
  va_end(ap);

  warning_issued = true;
}

void
mxerror(const char *fmt,
        ...) {
  va_list ap;

  va_start(ap, fmt);
  mxinfo("\n");
  mxmsg(MXMSG_ERROR, fmt, ap);
  va_end(ap);

  exit(2);
}

void
mxdebug(const char *fmt,
        ...) {
  va_list ap;

  va_start(ap, fmt);
  mxmsg(MXMSG_DEBUG, fmt, ap);
  va_end(ap);
}

void
mxexit(int code) {
  if (code != -1)
    exit(code);

  if (warning_issued)
    exit(1);

  exit(0);
}

int
get_arg_len(const char *fmt,
            ...) {
  int size;
  va_list ap;

  va_start(ap, fmt);
  size = get_varg_len(fmt, ap);
  va_end(ap);

  return size;
}

int
get_varg_len(const char *fmt,
             va_list ap) {
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

string
mxsprintf(const char *fmt,
          ...) {
  va_list ap;
  string new_fmt, dst;
  char *new_string;
  int len;

  fix_format(fmt, new_fmt);
  va_start(ap, fmt);
  len = get_varg_len(new_fmt.c_str(), ap);
  new_string = (char *)safemalloc(len + 1);
  vsprintf(new_string, new_fmt.c_str(), ap);
  va_end(ap);
  dst = new_string;
  safefree(new_string);

  return dst;
}

/** \brief Platform independant version of sscanf
 *
 * This is a platform independant version of sscanf. It first fixes the format
 * string (\see fix_format) and then calls sscanf.
 *
 * \param str The string to parse
 * \param fmt The format string
 * \returns The number of elements assigned
 */
int
mxsscanf(const string &str,
         const char *fmt,
         ...) {
  va_list ap;
  string new_fmt;
  int result;

  fix_format(fmt, new_fmt);
  va_start(ap, fmt);
  result = vsscanf(str.c_str(), new_fmt.c_str(), ap);
  va_end(ap);

  return result;
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
        mxinfo("%s\n", output);
        j = 0;
      }
      mxinfo("%08x  ", i);

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
    mxinfo("%02x ", buffer[i]);
  }
  while ((i % 16) != 0) {
    if ((i % 8) == 0)
      mxinfo(" ");
    mxinfo("   ");
    i++;
  }
  output[j] = 0;
  mxinfo("%s\n", output);
}

const char *timecode_parser_error = NULL;

static bool
set_tcp_error(const char *error) {
  timecode_parser_error = error;
  return false;
}

bool
parse_timecode(const char *src,
               int64_t *timecode) {
  // Recognized format:
  // HH:MM:SS and HH:MM:SS.nnn with up to nine 'n' for ns precision
  //              012345678901...
  int h, m, s, n, i;
  char format[10];

  if ((strlen(src) < 8) || (strlen(src) > 18) || (src[2] != ':') ||
      (src[5] != ':'))
    return set_tcp_error("Invalid format");
  if (sscanf(src, "%02d:%02d:%02d", &h, &m, &s) != 3)
    return set_tcp_error("Invalid format (non-numbers encountered)");
  if (h > 23)
    return set_tcp_error("Invalid hour");
  if (m > 59)
    return set_tcp_error("Invalid minute");
  if (s > 59)
    return set_tcp_error("Invalid second");
  if (strlen(src) > 9) {
    if (src[8] != '.')
      return set_tcp_error("Invalid format (expected a dot '.' after the "
                           "seconds)");
    sprintf(format, "%%0%dd", strlen(src) - 8);
    if (sscanf(&src[9], format, &n) != 1)
      return set_tcp_error("Invalid format (non-numbers encountered)");
    for (i = strlen(src); i < 18; i++)
      n *= 10;
  } else
    n = 0;
  if (timecode != NULL)
    *timecode = ((int64_t)h * 60 * 60 + (int64_t)m * 60 + (int64_t)s) *
      1000000000 + n;
  timecode_parser_error = "no error";
  return true;
}
