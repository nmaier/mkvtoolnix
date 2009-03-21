/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper functions, common variables

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <cassert>
#include <ctype.h>
#include <errno.h>
#include <iconv.h>
#if HAVE_NL_LANGINFO
# include <langinfo.h>
#elif HAVE_LOCALE_CHARSET
# include <libcharset.h>
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
#if defined(SYS_WINDOWS)
#include <wchar.h>
#include <windows.h>
#endif

#include <string>
#include <vector>

#include "ebml/EbmlBinary.h"

using namespace std;
using namespace libebml;

#include "base64.h"
#include "common.h"
#include "common_memory.h"
#include "error.h"
#include "hacks.h"
#include "mm_io.h"
#include "random.h"
#include "smart_pointers.h"
#include "translation.h"

const string empty_string("");
int verbose = 1;

extern bool g_warning_issued;

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

#define ishexdigit(c) (isdigit(c) || (((c) >= 'a') && ((c) <= 'f')))
#define hextodec(c) (isdigit(c) ? ((c) - '0') : ((c) - 'a' + 10))

bitvalue_c::bitvalue_c(string s,
                       int allowed_bitlength) {
  int len, i;
  bool previous_was_space;
  string s2;

  if ((allowed_bitlength != -1) && ((allowed_bitlength % 8) != 0))
    throw error_c("wrong usage: invalid allowed_bitlength");

  len = s.size();
  previous_was_space = true;
  s = downcase(s);

  for (i = 0; i < len; i++) {
    // Space or tab?
    if (isblanktab(s[i])) {
      previous_was_space = true;
      continue;
    }
    previous_was_space = false;

    // Skip hyphens and curly braces. Makes copy & paste a bit easier.
    if ((s[i] == '-') || (s[i] == '{') || (s[i] == '}'))
      continue;

    // Space or tab followed by "0x"? Then skip it.
    if (s.substr(i, 2) == "0x") {
      i++;
      continue;
    }

    // Invalid character?
    if (!ishexdigit(s[i]))
      throw error_c(boost::format(Y("Not a hex digit at position %1%")) % i);

    // Input too long?
    if ((allowed_bitlength > 0) && ((s2.length() * 4) >= allowed_bitlength))
      throw error_c(boost::format(Y("Input too long: %1% > %2%")) % (s2.length() * 4) % allowed_bitlength);

    // Store the value.
    s2 += s[i];
  }

  // Is half a byte or more missing?
  len = s2.length();
  if (((len % 2) != 0)
      ||
      ((allowed_bitlength != -1) && ((len * 4) < allowed_bitlength)))
    throw error_c(Y("Missing one hex digit"));

  value = (unsigned char *)safemalloc(len / 2);
  bitsize = len * 4;

  for (i = 0; i < len; i += 2)
    value[i / 2] = hextodec(s2[i]) << 4 | hextodec(s2[i + 1]);
}

bitvalue_c::bitvalue_c(const EbmlBinary &elt) {
  bitsize = elt.GetSize() << 3;
  value = (unsigned char *)safememdup(elt.GetBuffer(), elt.GetSize());
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
  if (cmp.bitsize != bitsize)
    return false;
  return memcmp(value, cmp.value, bitsize / 8) == 0;
}

unsigned char
bitvalue_c::operator [](int index)
  const {
  assert((index >= 0) && (index < (bitsize / 8)));
  return value[index];
}

void
bitvalue_c::generate_random() {
  random_c::generate_bytes(value, bitsize / 8);
}

/*
   Endianess stuff
*/

uint16_t
get_uint16_le(const void *buf) {
  uint16_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[1] & 0xff;
  ret = (ret << 8) + (tmp[0] & 0xff);

  return ret;
}

uint32_t
get_uint24_le(const void *buf) {
  uint32_t ret;
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  ret = tmp[2] & 0xff;
  ret = (ret << 8) + (tmp[1] & 0xff);
  ret = (ret << 8) + (tmp[0] & 0xff);

  return ret;
}

uint32_t
get_uint32_le(const void *buf) {
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
get_uint64_le(const void *buf) {
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
put_uint16_le(void *buf,
              uint16_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[0] = value & 0xff;
  tmp[1] = (value >>= 8) & 0xff;
}

void
put_uint32_le(void *buf,
              uint32_t value) {
  unsigned char *tmp;

  tmp = (unsigned char *) buf;

  tmp[0] = value & 0xff;
  tmp[1] = (value >>= 8) & 0xff;
  tmp[2] = (value >>= 8) & 0xff;
  tmp[3] = (value >>= 8) & 0xff;
}

void
put_uint64_le(void *buf,
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
   Character map conversion stuff
*/

struct kax_conv_t {
  iconv_t ict_from_utf8, ict_to_utf8;
  string charset;

  kax_conv_t(iconv_t n_ict_from_utf8, iconv_t n_ict_to_utf8,
             const string &n_charset):
    ict_from_utf8(n_ict_from_utf8), ict_to_utf8(n_ict_to_utf8),
    charset(n_charset) { }
};

static vector<kax_conv_t> kax_convs;
int cc_local_utf8 = -1;

int
add_kax_conv(const string &charset,
             iconv_t ict_from,
             iconv_t ict_to) {
  int i;

  for (i = 0; i < kax_convs.size(); i++)
    if (kax_convs[i].charset == charset)
      return i;

  kax_convs.push_back(kax_conv_t(ict_from, ict_to, charset));

  return kax_convs.size() - 1;
}

string
get_local_charset() {
  string lc_charset;

  setlocale(LC_CTYPE, "");
#if defined(COMP_MINGW) || defined(COMP_MSC)
  lc_charset = "CP" + to_string(GetOEMCP());
#elif defined(SYS_SOLARIS)
  int i;

  lc_charset = nl_langinfo(CODESET);
  if (parse_int(lc_charset, i))
    lc_charset = string("ISO") + lc_charset + string("-US");
#elif HAVE_NL_LANGINFO
  lc_charset = nl_langinfo(CODESET);
#elif HAVE_LOCALE_CHARSET
  lc_charset = locale_charset();
#endif

  return lc_charset;
}

int
utf8_init(const string &charset) {
  string lc_charset;
  iconv_t ict_from_utf8, ict_to_utf8;
  int i;

  if (charset == "")
    lc_charset = get_local_charset();
  else
    lc_charset = charset;

  if ((lc_charset == "UTF8") || (lc_charset == "UTF-8"))
    return -1;

  for (i = 0; i < kax_convs.size(); i++)
    if (kax_convs[i].charset == lc_charset)
      return i;

  ict_to_utf8 = iconv_open("UTF-8", lc_charset.c_str());
  if (ict_to_utf8 == (iconv_t)(-1))
    mxwarn(boost::format(Y("Could not initialize the iconv library for the conversion from %1% to UFT-8. "
                           "Some strings will not be converted to UTF-8 and the resulting Matroska file "
                           "might not comply with the Matroska specs (error: %2%, %3%).\n"))
           % lc_charset % errno % strerror(errno));

  ict_from_utf8 = iconv_open(lc_charset.c_str(), "UTF-8");
  if (ict_from_utf8 == (iconv_t)(-1))
    mxwarn(boost::format(Y("Could not initialize the iconv library for the conversion from UFT-8 to %1%. "
                           "Some strings cannot be converted from UTF-8 and might be displayed incorrectly (error: %2%, %3%).\n"))
           % lc_charset % errno % strerror(errno));

  return add_kax_conv(lc_charset.c_str(), ict_from_utf8, ict_to_utf8);
}

void
utf8_done() {
  int i;

  for (i = 0; i < kax_convs.size(); i++) {
    if (kax_convs[i].ict_from_utf8 != (iconv_t)(-1))
      iconv_close(kax_convs[i].ict_from_utf8);
    if (kax_convs[i].ict_to_utf8 != (iconv_t)(-1))
      iconv_close(kax_convs[i].ict_to_utf8);
  }
  kax_convs.clear();
}

static string
convert_charset(iconv_t ict,
                const string &src) {
  char *dst, *psrc, *pdst, *srccopy;
  size_t lsrc, ldst;
  int len;
  string result;

  if (ict == (iconv_t)(-1))
    return src;

  len = src.length() * 4;
  dst = (char *)safemalloc(len + 1);
  memset(dst, 0, len + 1);

  iconv(ict, NULL, 0, NULL, 0);      // Reset the iconv state.
  lsrc = len / 4;
  ldst = len;
  srccopy = safestrdup(src);
  psrc = srccopy;
  pdst = dst;
  iconv(ict, (ICONV_CONST char **)&psrc, &lsrc, &pdst, &ldst);
  iconv(ict, NULL, NULL, &pdst, &ldst);
  safefree(srccopy);
  result = dst;
  safefree(dst);

  return result;
}

string
to_utf8(int handle,
        const string &local) {
  string s;

  if (handle == -1)
    return local;

  if (handle >= kax_convs.size())
    mxerror(boost::format(Y("common.cpp/to_utf8(): Invalid conversion handle %1% (num: %2%).\n")) % handle % kax_convs.size());

  return convert_charset(kax_convs[handle].ict_to_utf8, local);
}

string
from_utf8(int handle,
          const string &utf8) {
  string s;

  if (handle == -1)
    return utf8;

  if (handle >= kax_convs.size())
    mxerror(boost::format(Y("common.cpp/from_utf8(): Invalid conversion handle %1% (num: %2%).\n")) % handle % kax_convs.size());

  return convert_charset(kax_convs[handle].ict_from_utf8, utf8);
}

/*
   Random unique uint32_t numbers
*/

static vector<uint32_t> ru_numbers[4];

void
clear_list_of_unique_uint32(unique_id_category_e category) {
  int i;

  assert((category >= UNIQUE_ALL_IDS) && (category <= UNIQUE_ATTACHMENT_IDS));
  if (category == UNIQUE_ALL_IDS)
    for (i = 0; i < 4; i++)
      clear_list_of_unique_uint32((unique_id_category_e)i);
  else
    ru_numbers[category].clear();
}

bool
is_unique_uint32(uint32_t number,
                 unique_id_category_e category) {
  int i;

  assert((category >= UNIQUE_TRACK_IDS) &&
         (category <= UNIQUE_ATTACHMENT_IDS));

  if (hack_engaged(ENGAGE_NO_VARIABLE_DATA))
    return true;
  for (i = 0; i < ru_numbers[category].size(); i++)
    if (ru_numbers[category][i] == number)
      return false;

  return true;
}

void
add_unique_uint32(uint32_t number,
                  unique_id_category_e category) {
  assert((category >= UNIQUE_TRACK_IDS) &&
         (category <= UNIQUE_ATTACHMENT_IDS));

  if (hack_engaged(ENGAGE_NO_VARIABLE_DATA))
    ru_numbers[category].push_back(ru_numbers[category].size() + 1);
  else
    ru_numbers[category].push_back(number);
}

bool
remove_unique_uint32(uint32_t number,
                     unique_id_category_e category) {
  vector<uint32_t>::iterator dit;

  assert((category >= UNIQUE_TRACK_IDS) &&
         (category <= UNIQUE_ATTACHMENT_IDS));

  if (hack_engaged(ENGAGE_NO_VARIABLE_DATA))
    return true;
  mxforeach(dit, ru_numbers[category])
    if (*dit == number) {
      ru_numbers[category].erase(dit);
      return true;
    }

  return false;
}

uint32_t
create_unique_uint32(unique_id_category_e category) {
  uint32_t rnumber;

  assert((category >= UNIQUE_TRACK_IDS) &&
         (category <= UNIQUE_ATTACHMENT_IDS));

  if (hack_engaged(ENGAGE_NO_VARIABLE_DATA)) {
    ru_numbers[category].push_back(ru_numbers[category].size() + 1);
    return ru_numbers[category].size();
  }

  do {
    rnumber = random_c::generate_32bits();
  } while ((rnumber == 0) || !is_unique_uint32(rnumber, category));
  add_unique_uint32(rnumber, category);

  return rnumber;
}

/*
   Miscellaneous stuff
*/

static uint64_t _safedupped = 0;
static uint64_t _safemalloced = 0;

void *
_safememdup(const void *s,
            size_t size,
            const char *file,
            int line) {
  void *copy;

  if (s == NULL)
    return NULL;

  _safedupped += size;

  copy = malloc(size);
  if (copy == NULL)
    mxerror(boost::format(Y("common.cpp/safememdup() called from file %1%, line %2%: malloc() returned NULL for a size of %3% bytes.\n")) % file % line % size);
  memcpy(copy, s, size);

  return copy;
}

void *
_safemalloc(size_t size,
            const char *file,
            int line) {
  void *mem;

  _safemalloced += size;

  mem = malloc(size);
  if (mem == NULL)
    mxerror(boost::format(Y("common.cpp/safemalloc() called from file %1%, line %2%: malloc() returned NULL for a size of %3% bytes.\n")) % file % line % size);

  return mem;
}

void *
_saferealloc(void *mem,
             size_t size,
             const char *file,
             int line) {
  if (0 == size)
    // Do this so realloc() may not return NULL on success.
    size = 1;
  mem = realloc(mem, size);
  if (mem == NULL)
    mxerror(boost::format(Y("common.cpp/saferealloc() called from file %1%, line %2%: realloc() returned NULL for a size of %3% bytes.\n")) % file % line % size);

  return mem;
}

void
safefree(void *p) {
  if (p != NULL)
    free(p);
}

void
dump_malloc_report() {
  mxinfo(boost::format("%1% bytes malloced, %2% bytes duplicated\n") % (_safemalloced - _safedupped) % _safedupped);
}

/*
   standard string processing
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
     const vector<string> &strings) {
  string dst;
  int i;

  if (strings.empty())
    return "";
  dst = strings[0];
  for (i = 1; i < strings.size(); i++) {
    dst += pattern;
    dst += strings[i];
  }

  return dst;
}

void
strip_back(string &s,
           bool newlines) {
  const char *c = s.c_str();
  int len       = s.length();
  int i         = 0;

  if (newlines)
    while ((i < len) && (isblanktab(c[len - i - 1]) || iscr(c[len - i - 1])))
      ++i;
  else
    while ((i < len) && isblanktab(c[len - i - 1]))
      ++i;

  if (i > 0)
    s.erase(len - i, i);
}

void
strip(string &s,
      bool newlines) {
  const char *c = s.c_str();
  int i         = 0;

  if (newlines)
    while ((c[i] != 0) && (isblanktab(c[i]) || iscr(c[i])))
      i++;
  else
    while ((c[i] != 0) && isblanktab(c[i]))
      i++;

  if (i > 0)
    s.erase(0, i);

  strip_back(s, newlines);
}

void
strip(vector<string> &v,
      bool newlines) {
  int i;

  for (i = 0; i < v.size(); i++)
    strip(v[i], newlines);
}

string &
shrink_whitespace(string &s) {
  int i;
  bool previous_was_whitespace;

  i = 0;
  previous_was_whitespace = false;
  while (s.length() > i) {
    if (!isblanktab(s[i])) {
      previous_was_whitespace = false;
      ++i;
      continue;
    }
    if (previous_was_whitespace)
      s.erase(i, 1);
    else {
      previous_was_whitespace = true;
      ++i;
    }
  }

  return s;
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
    else if (*src == ':')
      dst += "\\c";
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
        else if (*next_char == 'c')
          dst += ':';
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
    else if (*src == '"')
      dst += "&quot;";
    else
      dst += *src;
    src++;
  }

  return dst;
}

string
create_xml_node_name(const char *name,
                     const char **atts) {
  int i;
  string node_name;

  node_name = string("<") + name;
  for (i = 0; (NULL != atts[i]) && (NULL != atts[i + 1]); i += 2)
    node_name += string(" ") + atts[i] + "=\"" +
      escape_xml(atts[i + 1]) + "\"";
  node_name += ">";

  return node_name;
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

string
get_displayable_string(const char *src,
                       int max_len) {
  string result;
  int    len = (-1 == max_len) ? strlen(src) : max_len;

  for (int i = 0; i < len; ++i)
    result += (' ' > src[i]) ? '?' : src[i];

  return result;
}

/*
   Integer parsing
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

int
int_log2(uint32_t value) {
  uint32_t highest, bit, mask;

  highest = 0;
  for (mask = 1, bit = 0; 31 >= bit; mask <<= 1, ++bit)
    if (value & mask)
      highest = bit;

  return highest;
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

  tmp = 0;
  result = parse_int(s, tmp);
  value = tmp;

  return result;
}

bool
parse_uint(const char *s,
           uint64_t &value) {
  if (*s == 0)
    return false;

  value = 0;
  const char *p = s;
  while (*p != 0) {
    if (!isdigit(*p))
      return false;
    value *= 10;
    value += *p - '0';
    p++;
  }

  return true;
}

bool
parse_uint(const char *s,
           uint32_t &value) {
  uint64_t tmp;
  bool result;

  tmp = 0;
  result = parse_uint(s, tmp);
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
  return (boost::format("%1%") % i).str();
}

/*
   Other related news
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

std::string
format_timecode(int64_t timecode,
                unsigned int precision) {
  std::string result = (boost::format("%|1$02d|:%|2$02d|:%|3$02d|")
                        % (int)( timecode / 60 / 60 / 1000000000)
                        % (int)((timecode      / 60 / 1000000000) % 60)
                        % (int)((timecode           / 1000000000) % 60)).str();

  if (9 < precision)
    precision = 9;

  if (precision) {
    std::string decimals = (boost::format(".%|1$09d|") % (int)(timecode % 1000000000)).str();

    if (decimals.length() > (precision + 1))
      decimals.erase(precision + 1);

    result += decimals;
  }

  return result;
}

void
mxexit(int code) {
  if (code != -1)
    exit(code);

  if (g_warning_issued)
    exit(1);

  exit(0);
}

/** \brief Platform independant version of sscanf

   This is a platform independant version of sscanf. It first fixes the format
   string (\see fix_format) and then calls sscanf.

   \param str The string to parse
   \param fmt The format string
   \returns The number of elements assigned
*/
int
mxsscanf(const string &str,
         const char *fmt,
         ...) {
  va_list ap;
  string new_fmt;
  int result;

  mxverb(5, boost::format("mxsscanf: str: %1% /// fmt: %2%\n") % str % fmt);
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

string timecode_parser_error;

inline bool
set_tcp_error(const std::string &error) {
  timecode_parser_error = error;
  return false;
}

inline bool
set_tcp_error(const boost::format &error) {
  timecode_parser_error = error.str();
  return false;
}

bool
parse_timecode(const string &src,
               int64_t &timecode,
               bool allow_negative) {
  // Recognized format:
  // 1. XXXXXXXu   with XXXXXX being a number followed
  //    by one of the units 's', 'ms', 'us' or 'ns'
  // 2. HH:MM:SS.nnnnnnnnn  with up to nine digits 'n' for ns precision;
  // HH: is optional; HH, MM and SS can be either one or two digits.
  // 2. HH:MM:SS:nnnnnnnnn  with up to nine digits 'n' for ns precision;
  // HH: is optional; HH, MM and SS can be either one or two digits.
  int h, m, s, n, i, values[4], num_values, num_digits, num_colons;
  int offset = 0, negative = 1;
  bool decimal_point_found;

  if (src.empty())
    return false;

  if ('-' == src[0]) {
    if (!allow_negative)
      return false;
    negative = -1;
    offset = 1;
  }

  try {
    if (src.length() < (2 + offset))
      throw false;

    string unit = src.substr(src.length() - 2, 2);

    int64_t multiplier = 1000000000;
    int unit_length = 2;
    int64_t value = 0;

    if (unit == "ms")
      multiplier = 1000000;
    else if (unit == "us")
      multiplier = 1000;
    else if (unit == "ns")
      multiplier = 1;
    else if (unit.substr(1, 1) == "s")
      unit_length = 1;
    else
      throw false;

    if (src.length() < (unit_length + 1 + offset))
      throw false;

    if (!parse_int(src.substr(offset, src.length() - unit_length - offset), value))
      throw false;

    timecode = value * multiplier * negative;

    return true;
  } catch (...) {
  }

  num_digits = 0;
  num_colons = 0;
  num_values = 1;
  decimal_point_found = false;
  memset(&values, 0, sizeof(int) * 4);

  for (i = offset; src.length() > i; ++i) {
    if (isdigit(src[i])) {
      if (decimal_point_found && (9 == num_digits))
        return set_tcp_error(Y("Invalid format: More than nine nano-second digits"));
      values[num_values - 1] = values[num_values - 1] * 10 + src[i] - '0';
      ++num_digits;

    } else if (('.' == src[i]) ||
               ((':' == src[i]) && (2 == num_colons))) {
      if (decimal_point_found)
        return set_tcp_error(Y("Invalid format: Second decimal point after first decimal point"));

      if (0 == num_digits)
        return set_tcp_error(Y("Invalid format: No digits before decimal point"));
      ++num_values;
      num_digits = 0;
      decimal_point_found = true;

    } else if (':' == src[i]) {
      if (decimal_point_found)
        return set_tcp_error(Y("Invalid format: Colon inside nano-second part"));
      if (2 == num_colons)
        return set_tcp_error(Y("Invalid format: More than two colons"));
      if (0 == num_digits)
        return set_tcp_error(Y("Invalid format: No digits before colon"));
      ++num_colons;
      ++num_values;
      num_digits = 0;

    } else
      return set_tcp_error(boost::format(Y("Invalid format: unknown character '%1%' found")) % src[i]);
  }

  if (1 > num_colons)
    return set_tcp_error(Y("Invalid format: At least minutes and seconds have to be given, but no colon was found"));

  if ((':' == src[src.length() - 1]) || ('.' == src[src.length() - 1]))
    return set_tcp_error(Y("Invalid format: The last character is a colon or a decimal point instead of a digit"));

  // No error has been found. Now find out whoich parts have been
  // set and which haven't.

  if (4 == num_values) {
    h = values[0];
    m = values[1];
    s = values[2];
    n = values[3];
    for (; 9 > num_digits; ++num_digits)
      n *= 10;

  } else if (2 == num_values) {
    h = 0;
    m = values[0];
    s = values[1];
    n = 0;

  } else if (decimal_point_found) {
    h = 0;
    m = values[0];
    s = values[1];
    n = values[2];
    for (; 9 > num_digits; ++num_digits)
      n *= 10;

  } else {
    h = values[0];
    m = values[1];
    s = values[2];
    n = 0;

  }

  if (m > 59)
    return set_tcp_error(boost::format(Y("Invalid number of minutes: %1% > 59")) % m);
  if (s > 59)
    return set_tcp_error(boost::format(Y("Invalid number of seconds: %1% > 59")) % s);

  timecode = ((int64_t)h * 60 * 60 + (int64_t)m * 60 + (int64_t)s) *
    1000000000ll + n;

  timecode *= negative;

  timecode_parser_error = Y("no error");
  return true;
}

/** \brief Reads command line arguments from a file

   Each line contains exactly one command line argument or a
   comment. Arguments are converted to UTF-8 and appended to the array
   \c args.
*/
static void
read_args_from_file(vector<string> &args,
                    const string &filename) {
  mm_text_io_c *mm_io;
  string buffer;
  bool skip_next;

  mm_io = NULL;
  try {
    mm_io = new mm_text_io_c(new mm_file_io_c(filename));
  } catch (...) {
    mxerror(boost::format(Y("The file '%1%' could not be opened for reading command line arguments.\n")) % filename);
  }

  skip_next = false;
  while (!mm_io->eof() && mm_io->getline2(buffer)) {
    if (skip_next) {
      skip_next = false;
      continue;
    }
    strip(buffer);

    if (buffer == "#EMPTY#") {
      args.push_back("");
      continue;
    }

    if ((buffer[0] == '#') || (buffer[0] == 0))
      continue;

    if (buffer == "--command-line-charset") {
      skip_next = true;
      continue;
    }
    args.push_back(buffer);
  }

  delete mm_io;
}

/** \brief Expand the command line parameters

   Takes each command line paramter, converts it to UTF-8, and reads more
   commands from command files if the argument starts with '@'. Puts all
   arguments into a new array.
   On Windows it uses the \c GetCommandLineW() function. That way it can
   also handle multi-byte input like Japanese file names.

   \param argc The number of arguments. This is the same argument that
     \c main normally receives.
   \param argv The arguments themselves. This is the same argument that
     \c main normally receives.
   \return An array of strings converted to UTF-8 containing all the
     command line arguments and any arguments read from option files.
*/
#if !defined(SYS_WINDOWS)
vector<string>
command_line_utf8(int argc,
                  char **argv) {
  int i, cc_command_line;
  vector<string> args;

  cc_command_line = cc_local_utf8;

  for (i = 1; i < argc; i++)
    if (argv[i][0] == '@')
      read_args_from_file(args, &argv[i][1]);
    else {
      if (!strcmp(argv[i], "--command-line-charset")) {
        if ((i + 1) == argc)
          mxerror(Y("'--command-line-charset' is missing its argument.\n"));
        cc_command_line = utf8_init(argv[i + 1] == NULL ? "" : argv[i + 1]);
        i++;
      } else
        args.push_back(to_utf8(cc_command_line, argv[i]));
    }

  return args;
}

#else  // !defined(SYS_WINDOWS)

vector<string>
command_line_utf8(int,
                  char **) {
  vector<string> args;
  wchar_t *p;
  string utf8;
  bool quoted, ignore_me, skip_first;

  p = GetCommandLineW();
  quoted = false;
  ignore_me = true;
  skip_first = true;
  while (*p != 0) {
    if (*p == L'"')
      quoted = !quoted;
    else if (*p == L' ') {
      if (quoted)
        utf8 += ' ';
      else if (!ignore_me) {
        if (!skip_first) {
          if (utf8[0] == '@')
            read_args_from_file(args, utf8.substr(1).c_str());
          else
            args.push_back(utf8);
        }
        skip_first = false;
        ignore_me = true;
        utf8.clear();
      }
    } else {
      ignore_me = false;
      if (*p < 0x80)
        utf8 += (char)*p;
      else if (*p < 0x800) {
        utf8 += (char)(0xc0 | (*p >> 6));
        utf8 += (char)(0x80 | (*p & 0x3f));
      } else {
        utf8 += (char)(0xe0 | (*p >> 12));
        utf8 += (char)(0x80 | ((*p >> 6) & 0x3f));
        utf8 += (char)(0x80 | (*p & 0x3f));
      }
    }

    ++p;
  }
  if (!ignore_me && !skip_first) {
    if (utf8[0] == '@')
      read_args_from_file(args, utf8.substr(1));
    else
      args.push_back(utf8);
  }

  return args;
}
#endif // !defined(SYS_WINDOWS)

string usage_text, version_info;

/** Handle command line arguments common to all programs

   Iterates over the list of command line arguments and handles the ones
   that are common to all programs. These include --output-charset,
   --redirect-output, --help, --version and --verbose along with their
   short counterparts.

   \param args A vector of strings containing the command line arguments.
     The ones that have been handled are removed from the vector.
   \param redirect_output_short The name of the short option that is
     recognized for --redirect-output. If left empty then no short
     version is accepted.
   \returns \c true if the locale has changed and the function should be
     called again and \c false otherwise.
*/
bool
handle_common_cli_args(vector<string> &args,
                       const string &redirect_output_short) {
  int i;

  // First see if there's an output charset given.
  i = 0;
  while (args.size() > i) {
    if (args[i] == "--output-charset") {
      if ((i + 1) == args.size())
        mxerror(Y("Missing argument for '--output-charset'.\n"));
      set_cc_stdio(args[i + 1]);
      args.erase(args.begin() + i, args.begin() + i + 2);
    } else
      ++i;
  }

  // Now let's see if the user wants the output redirected.
  i = 0;
  while (args.size() > i) {
    if ((args[i] == "--redirect-output") || (args[i] == "-r") ||
        ((redirect_output_short != "") &&
         (args[i] == redirect_output_short))) {
      if ((i + 1) == args.size())
        mxerror(boost::format(Y("'%1%' is missing the file name.\n")) % args[i]);
      try {
        if (!stdio_redirected()) {
          mm_file_io_c *file = new mm_file_io_c(args[i + 1], MODE_CREATE);
          file->write_bom(g_stdio_charset);
          redirect_stdio(file);
        }
        args.erase(args.begin() + i, args.begin() + i + 2);
      } catch(mm_io_open_error_c &e) {
        mxerror(boost::format(Y("Could not open the file '%1%' for directing the output.\n")) % args[i + 1]);
      }
    } else
      ++i;
  }

  // Check for the translations to use (if any).
  i = 0;
  while (args.size() > i) {
    if (args[i] == "--ui-language") {
      if ((i + 1) == args.size())
        mxerror(Y("Missing argument for '--ui-language'.\n"));

      if (args[i + 1] == "list") {
        mxinfo(Y("Available translations:\n"));
        std::vector<translation_c>::iterator translation = translation_c::ms_available_translations.begin();
        while (translation != translation_c::ms_available_translations.end()) {
          mxinfo(boost::format("  %1% (%2%)\n") % translation->get_locale() % translation->m_english_name);
          ++translation;
        }
        mxexit(0);
      }

      if (-1 == translation_c::look_up_translation(args[i + 1]))
        mxerror(boost::format(Y("There is no translation available for '%1%'.\n")) % args[i + 1]);

      init_locales(args[i + 1]);

      args.erase(args.begin() + i, args.begin() + i + 2);

      return true;
    } else
      ++i;
  }

  // Last find the --help and --version arguments.
  i = 0;
  while (args.size() > i) {
    if ((args[i] == "-V") || (args[i] == "--version")) {
      mxinfo(boost::format(Y("%1% built on %2% %3%\n")) % version_info % __DATE__ % __TIME__);
      mxexit(0);

    } else if ((args[i] == "-v") || (args[i] == "--verbose")) {
      ++verbose;
      args.erase(args.begin() + i, args.begin() + i + 1);

    } else if ((args[i] == "-h") || (args[i] == "-?") ||
             (args[i] == "--help"))
      usage();

    else
      ++i;
  }

  return false;
}

void
usage(int exit_code) {
  mxinfo(boost::format("%1%\n") % usage_text);
  mxexit(exit_code);
}

buffer_t::buffer_t():
  m_buffer(NULL), m_size(0) {
}

buffer_t::buffer_t(unsigned char *buffer,
                   int size):
  m_buffer(buffer), m_size(size) {
}

buffer_t::~buffer_t() {
  safefree(m_buffer);
}
