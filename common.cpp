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
    \version \$Id: common.cpp,v 1.26 2003/05/25 15:35:39 mosu Exp $
    \brief helper functions, common variables
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <errno.h>
#include <iconv.h>
#ifdef WIN32
#include <libcharset.h>
#else
#include <langinfo.h>
#endif
#include <locale.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <time.h>
#include <wchar.h>

#include "common.h"
#include "pr_generic.h"

int verbose = 1;

void _die(const char *s, const char *file, int line) {
  fprintf(stderr, "die @ %s/%d : %s\n", file, line, s);
  exit(1);
}

void _trace(const char *func, const char *file, int line) {
  fprintf(stdout, "trace: %s:%s (%d)\n", file, func, line);
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

int utf8_init(char *charset) {
  char *lc_charset;
  iconv_t ict_from_utf8, ict_to_utf8;
  int i;

  if ((charset == NULL) || (*charset == 0)) {
    setlocale(LC_CTYPE, "");
#ifdef WIN32
    lc_charset = (char *)locale_charset();
#else
    lc_charset = nl_langinfo(CODESET);
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
    fprintf(stdout, "Warning: Could not initialize the iconv library for "
            "the conversion from %s to UFT-8. "
            "Some strings will not be converted to UTF-8 and the resulting "
            "Matroska file might not comply with the Matroska specs ("
            "error: %d, %s).\n", lc_charset, errno, strerror(errno));

  ict_from_utf8 = iconv_open(lc_charset, "UTF-8");
  if (ict_from_utf8 == (iconv_t)(-1))
    fprintf(stdout, "Warning: Could not initialize the iconv library for "
            "the conversion from UFT-8 to %s. "
            "Some strings cannot be converted from UTF-8 and might be "
            "displayed incorrectly (error: %d, %s).\n", lc_charset, errno,
            strerror(errno));

  return add_mkv_conv(lc_charset, ict_from_utf8, ict_to_utf8);
}

void utf8_done() {
  int i;

  for (i = 0; i < num_mkv_convs; i++)
    safefree(mkv_convs[i].charset);
  if (mkv_convs != NULL)
    safefree(mkv_convs);
}

static char *convert_charset(iconv_t ict, char *src) {
  char *dst, *psrc, *pdst;
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
  psrc = src;
  pdst = dst;
#ifdef __CYGWIN__
  iconv(ict, (const char **)&psrc, &lsrc, &pdst, &ldst);
#else
  iconv(ict, &psrc, &lsrc, &pdst, &ldst);
#endif

  return dst;
}

char *to_utf8(int handle, char *local) {
  char *copy;

  if (handle == -1) {
    copy = safestrdup(local);
    return copy;
  }

  if (handle >= num_mkv_convs)
    die("Invalid conversion handle.");

  return convert_charset(mkv_convs[handle].ict_to_utf8, local);
}

char *from_utf8(int handle, char *utf8) {
  char *copy;

  if (handle == -1) {
    copy = safestrdup(utf8);
    return copy;
  }

  if (handle >= num_mkv_convs)
    die("Invalid conversion handle.");

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
  if (copy == NULL) {
    fprintf(stderr, "die @ %s/%d : in safestrdup: strdup == NULL\n", file,
            line);
    exit(1);
  }

  return copy;
}

unsigned char *_safestrdup(const unsigned char *s, const char *file,
                           int line) {
  char *copy;

  if (s == NULL)
    return NULL;

  copy = strdup((const char *)s);
  if (copy == NULL) {
    fprintf(stderr, "die @ %s/%d : in safestrdup: strdup == NULL\n", file,
            line);
    exit(1);
  }

  return (unsigned char *)copy;
}

void *_safememdup(const void *s, size_t size, const char *file, int line) {
  void *copy;

  if (s == NULL)
    return NULL;

  copy = malloc(size);
  if (copy == NULL) {
    fprintf(stderr, "die @ %s/%d : in safememdup: malloc == NULL\n", file,
            line);
    exit(1);
  }
  memcpy(copy, s, size);

  return copy;
}

void *_safemalloc(size_t size, const char *file, int line) {
  void *mem;

  mem = malloc(size);
  if (mem == NULL) {
    fprintf(stderr, "die @ %s/%d : in safemalloc: malloc == NULL\n", file,
            line);
    exit(1);
  }

  return mem;
}

void *_saferealloc(void *mem, size_t size, const char *file, int line) {
  mem = realloc(mem, size);
  if (mem == NULL) {
    fprintf(stderr, "die @ %s/%d : in safemalloc: realloc == NULL\n", file,
            line);
    exit(1);
  }

  return mem;
}

/*
 * UTFstring <-> C string conversion
 */

UTFstring cstr_to_UTFstring(const char *c) {
#ifdef NO_WSTRING
  return UTFstring(c);
#else
  wchar_t *new_string;
  const char *sptr;
  UTFstring u;
  int len;

  len = strlen(c);
  new_string = (wchar_t *)safemalloc((len + 1) * sizeof(wchar_t));
  memset(new_string, 0, (len + 1) * sizeof(wchar_t));
  new_string[len] = L'\0';
  sptr = c;
  mbsrtowcs(new_string, &sptr, len, NULL);
  u = UTFstring(new_string);
  safefree(new_string);

  return u;
#endif
}

char *UTFstring_to_cstr(const UTFstring &u) {
#ifdef NO_WSTRING
  return safestrdup(u.c_str());
#else
  const wchar_t *sptr;
  char *new_string;
  int len;

  len = u.size();
  new_string = (char *)safemalloc(len * 4 + 1);
  memset(new_string, 0, len * 4 + 1);
  sptr = u.c_str();
  wcsrtombs(new_string, &sptr, len, NULL);

  return new_string;
#endif
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
  struct timeval tv;

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

  gettimeofday(&tv, NULL);
  entry->entered_at = (uint64_t)tv.tv_sec * (uint64_t)1000000 +
    tv.tv_usec;
}

void debug_c::leave(const char *label) {
  int i;
  debug_c *entry;
  struct timeval tv;

  gettimeofday(&tv, NULL);

  entry = NULL;
  for (i = 0; i < dbg_entries.size(); i++)
    if (!strcmp(dbg_entries[i]->label, label)) {
      entry = dbg_entries[i];
      break;
    }

  if ((entry == NULL) || (entry->entered_at == 0)) {
    string s("leave without enter: ");
    s += label;
    die(s.c_str());
  }

  entry->number_of_calls++;
  entry->elapsed_time += (uint64_t)tv.tv_sec * (uint64_t)1000000 +
    tv.tv_usec - entry->entered_at;
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

  fprintf(stderr, "\nDBG> dumping time info:\n");
  for (i = 0; i < dbg_entries.size(); i++) {
    entry = dbg_entries[i];
    fprintf(stderr, "DBG> function: %s, # calls: %llu, elapsed time: %.3fs, "
            "time/call: %.3fms", entry->label, entry->number_of_calls,
            entry->elapsed_time / 1000000.0,
            entry->elapsed_time / (float)entry->number_of_calls / 1000.0);
    diff_calls = entry->number_of_calls - entry->last_number_of_calls;
    diff_time = entry->elapsed_time - entry->last_elapsed_time;
    if ((entry->last_elapsed_time != 0) &&
        (entry->last_number_of_calls != 0) &&
        (diff_calls > 0)) {
      fprintf(stderr, ", since the last call: # calls: %llu, elapsed time: "
              "%.3fs, time/call: %.3fms", diff_calls, diff_time / 1000000.0,
              diff_time / (float)diff_calls / 1000.0);
    }
    fprintf(stderr, "\n");
    entry->last_elapsed_time = entry->elapsed_time;
    entry->last_number_of_calls = entry->number_of_calls;
  }

  fprintf(stderr, "DBG> dumping packetzer info:\n");
  for (i = 0; i < dbg_packetizers.size(); i++)
    dbg_packetizers[i]->dump_debug_info();
}

#endif // DEBUG
