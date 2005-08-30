/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   definitions used in all programs, helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __COMMON_H
#define __COMMON_H

#include "os.h"

#include <stdarg.h>

#include <string>
#include <vector>

#include "config.h"

/* i18n stuff */
#if defined(HAVE_LIBINTL_H)
# include <libintl.h>
# if !defined _
#  define _(s) gettext(s)
# endif
# if !defined N_
#  define N_(s) s
# endif
# if !defined Y
#  define Y(s) gettext(s)
# endif
#else /* HAVE_LIBINTL_H */
# if !defined _
#  define _(s) (s)
# endif
# if !defined N_
#  define N_(s) s
# endif
# if !defined Y
#  define Y(s) s
# endif
#endif

namespace libebml {
  class EbmlBinary;
};

using namespace libebml;

#define VERSIONNAME "Breathe me"
#define VERSIONINFO "mkvmerge v" VERSION " ('" VERSIONNAME "')"
#define BUGMSG _("This should not have happened. Please contact the author " \
                 "Moritz Bunkus <moritz@bunkus.org> with this error/warning " \
                 "message, a description of what you were trying to do, " \
                 "the command line used and which operating system you are " \
                 "using. Thank you.")

using namespace std;

#define MXMSG_ERROR    5
#define MXMSG_WARNING 10
#define MXMSG_INFO    15
#define MXMSG_DEBUG   20

#define FOURCC(a, b, c, d) (uint32_t)((((unsigned char)a) << 24) + \
                           (((unsigned char)b) << 16) + \
                           (((unsigned char)c) << 8) + \
                           ((unsigned char)d))
#define isblanktab(c) (((c) == ' ') || ((c) == '\t'))
#define iscr(c) (((c) == '\n') || ((c) == '\r'))


#define TIMECODE_SCALE 1000000

#define FMT_TIMECODE "%02d:%02d:%02d.%03d"
#define ARG_TIMECODEINT(t) (int32_t)((t) / 60 / 60 / 1000), \
                           (int32_t)(((t) / 60 / 1000) % 60), \
                           (int32_t)(((t) / 1000) % 60), \
                           (int32_t)((t) % 1000)
#define ARG_TIMECODE(t) ARG_TIMECODEINT((int64_t)(t))
#define ARG_TIMECODE_NS(t) ARG_TIMECODE((t) / 1000000)
#define FMT_TIMECODEN "%02d:%02d:%02d.%09d"
#define ARG_TIMECODENINT(t) (int32_t)((t) / 60 / 60 / 1000000000), \
                            (int32_t)(((t) / 60 / 1000000000) % 60), \
                            (int32_t)(((t) / 1000000000) % 60), \
                            (int32_t)((t) % 1000000000)
#define ARG_TIMECODEN(t) ARG_TIMECODENINT((int64_t)(t))
extern string MTX_DLL_API timecode_parser_error;
extern bool MTX_DLL_API parse_timecode(const string &s, int64_t &timecode);

extern bool MTX_DLL_API suppress_warnings;
void MTX_DLL_API fix_format(const char *fmt, string &new_fmt);
#if defined(__GNUC__)
void MTX_DLL_API die(const char *fmt, ...)
  __attribute__ ((format (printf, 1, 2)));
void MTX_DLL_API mxprint(void *stream, const char *fmt, ...)
  __attribute__ ((format (printf, 2, 3)));
void MTX_DLL_API mxprints(char *dst, const char *fmt, ...)
  __attribute__ ((format (printf, 2, 3)));
string MTX_DLL_API mxsprintf(const char *fmt, ...)
  __attribute__ ((format (printf, 1, 2)));
void MTX_DLL_API mxwarn(const char *fmt, ...)
  __attribute__ ((format (printf, 1, 2)));
void MTX_DLL_API mxerror(const char *fmt, ...)
  __attribute__ ((format (printf, 1, 2)));
void MTX_DLL_API mxinfo(const char *fmt, ...)
  __attribute__ ((format (printf, 1, 2)));
void MTX_DLL_API mxverb(int level, const char *fmt, ...)
  __attribute__ ((format (printf, 2, 3)));
void MTX_DLL_API mxdebug(const char *fmt, ...)
  __attribute__ ((format (printf, 1, 2)));
int MTX_DLL_API mxsscanf(const string &str, const char *fmt, ...)
  __attribute__ ((format (scanf, 2, 3)));
#else
void MTX_DLL_API die(const char *fmt, ...);
void MTX_DLL_API mxprint(void *stream, const char *fmt, ...);
void MTX_DLL_API mxprints(char *dst, const char *fmt, ...);
string MTX_DLL_API mxsprintf(const char *fmt, ...);
void MTX_DLL_API mxwarn(const char *fmt, ...);
void MTX_DLL_API mxerror(const char *fmt, ...);
void MTX_DLL_API mxinfo(const char *fmt, ...);
void MTX_DLL_API mxverb(int level, const char *fmt, ...);
void MTX_DLL_API mxdebug(const char *fmt, ...);
int MTX_DLL_API mxsscanf(const string &str, const char *fmt, ...);
#endif
string MTX_DLL_API mxvsprintf(const char *fmt, va_list ap);
void MTX_DLL_API mxexit(int code = -1);

#define trace() _trace(__func__, __FILE__, __LINE__)
void MTX_DLL_API _trace(const char *func, const char *file, int line);

void MTX_DLL_API mxhexdump(int level, const unsigned char *buffer, int lenth);

class mm_io_c;

void MTX_DLL_API init_stdio();
void MTX_DLL_API redirect_stdio(mm_io_c *new_stdio);
bool MTX_DLL_API stdio_redirected();

#define get_fourcc(b) get_uint32_be(b)
uint16_t MTX_DLL_API get_uint16_le(const void *buf);
uint32_t MTX_DLL_API get_uint24_le(const void *buf);
uint32_t MTX_DLL_API get_uint32_le(const void *buf);
uint64_t MTX_DLL_API get_uint64_le(const void *buf);
uint16_t MTX_DLL_API get_uint16_be(const void *buf);
uint32_t MTX_DLL_API get_uint24_be(const void *buf);
uint32_t MTX_DLL_API get_uint32_be(const void *buf);
uint64_t MTX_DLL_API get_uint64_be(const void *buf);
void MTX_DLL_API put_uint16_le(void *buf, uint16_t value);
void MTX_DLL_API put_uint32_le(void *buf, uint32_t value);
void MTX_DLL_API put_uint64_le(void *buf, uint64_t value);
void MTX_DLL_API put_uint16_be(void *buf, uint16_t value);
void MTX_DLL_API put_uint32_be(void *buf, uint32_t value);
void MTX_DLL_API put_uint64_be(void *buf, uint64_t value);

extern int MTX_DLL_API cc_local_utf8;
void MTX_DLL_API init_cc_stdio();
void MTX_DLL_API set_cc_stdio(const string &charset);
extern string MTX_DLL_API stdio_charset;
string MTX_DLL_API get_local_charset();
int MTX_DLL_API utf8_init(const string &charset);
void MTX_DLL_API utf8_done();
string MTX_DLL_API to_utf8(int handle, const string &local);
string MTX_DLL_API from_utf8(int handle, const string &utf8);

vector<string> MTX_DLL_API command_line_utf8(int argc, char **argv);
extern string MTX_DLL_API usage_text, version_info;
void MTX_DLL_API usage(int exit_code = 0);
void MTX_DLL_API handle_common_cli_args(vector<string> &args,
                                        const string &redirect_output_short);

enum unique_id_category_e {
  UNIQUE_ALL_IDS = -1,
  UNIQUE_TRACK_IDS = 0,
  UNIQUE_CHAPTER_IDS = 1,
  UNIQUE_EDITION_IDS = 2,
  UNIQUE_ATTACHMENT_IDS = 3
};

void MTX_DLL_API clear_list_of_unique_uint32(unique_id_category_e category);
bool MTX_DLL_API is_unique_uint32(uint32_t number,
                                  unique_id_category_e category);
void MTX_DLL_API add_unique_uint32(uint32_t number,
                                   unique_id_category_e category);
bool MTX_DLL_API remove_unique_uint32(uint32_t number,
                                      unique_id_category_e category);
uint32_t MTX_DLL_API create_unique_uint32(unique_id_category_e category);

void MTX_DLL_API safefree(void *p);
#define safemalloc(s) _safemalloc(s, __FILE__, __LINE__)
void *MTX_DLL_API _safemalloc(size_t size, const char *file, int line);
#define safememdup(src, size) _safememdup(src, size, __FILE__, __LINE__)
void *MTX_DLL_API _safememdup(const void *src, size_t size, const char *file,
                              int line);
#define safestrdup(s) _safestrdup(s, __FILE__, __LINE__)
inline char *
_safestrdup(const string &s,
            const char *file,
            int line) {
  return (char *)_safememdup(s.c_str(), s.length() + 1, file, line);
}
inline unsigned char *
_safestrdup(const unsigned char *s,
            const char *file,
            int line) {
  return (unsigned char *)_safememdup(s, strlen((const char *)s) + 1, file,
                                      line);
}
inline char *
_safestrdup(const char *s,
            const char *file,
            int line) {
  return (char *)_safememdup(s, strlen(s) + 1, file, line);
}
#define saferealloc(mem, size) _saferealloc(mem, size, __FILE__, __LINE__)
void *MTX_DLL_API _saferealloc(void *mem, size_t size, const char *file,
                               int line);
void MTX_DLL_API dump_malloc_report();

vector<string> MTX_DLL_API split(const char *src, const char *pattern = ",",
                              int max_num = -1);
inline vector<string>
split(const string &src,
      const string &pattern = string(","),
      int max_num = -1) {
  return split(src.c_str(), pattern.c_str(), max_num);
}
string MTX_DLL_API join(const char *pattern, vector<string> &strings);
void MTX_DLL_API strip(string &s, bool newlines = false);
void MTX_DLL_API strip(vector<string> &v, bool newlines = false);
string MTX_DLL_API escape(const string &src);
string MTX_DLL_API unescape(const string &src);
string MTX_DLL_API escape_xml(const string &src, bool escape_quotes = false);
string MTX_DLL_API create_xml_node_name(const char *name, const char **atts);
bool MTX_DLL_API starts_with(const string &s, const char *start,
                             int maxlen = -1);
inline bool
starts_with(const string &s,
            const string &start,
            int maxlen = -1) {
  return starts_with(s, start.c_str(), maxlen);
}
bool MTX_DLL_API starts_with_case(const string &s, const char *start,
                                  int maxlen = -1);
inline bool
starts_with_case(const string &s,
                 const string &start,
                 int maxlen = -1) {
  return starts_with_case(s, start.c_str(), maxlen);
}
string MTX_DLL_API upcase(const string &s);
string MTX_DLL_API downcase(const string &s);

#define irnd(a) ((int64_t)((double)(a) + 0.5))
#define iabs(a) ((a) < 0 ? (a) * -1 : (a))

uint32_t MTX_DLL_API round_to_nearest_pow2(uint32_t value);
bool MTX_DLL_API parse_int(const char *s, int64_t &value);
bool MTX_DLL_API parse_int(const char *s, int &value);
inline bool
parse_int(const string &s,
          int64_t &value) {
  return parse_int(s.c_str(), value);
}
inline bool
parse_int(const string &s,
          int &value) {
  return parse_int(s.c_str(), value);
}
string MTX_DLL_API to_string(int64_t i);
bool MTX_DLL_API parse_double(const char *s, double &value);

int MTX_DLL_API get_arg_len(const char *fmt, ...);
int MTX_DLL_API get_varg_len(const char *fmt, va_list ap);

extern int MTX_DLL_API verbose;

#define foreach(it, vec) for (it = (vec).begin(); it != (vec).end(); it++)
#define mxfind(value, cont) std::find(cont.begin(), cont.end(), value)
#define mxfind2(it, value, cont) \
  ((id = std::find((cont).begin(), (cont).end(), value)) != (cont).end())
#define map_has_key(m, k) ((m).end() != (m).find(k))

class MTX_DLL_API bitvalue_c {
private:
  unsigned char *value;
  int bitsize;
public:
  bitvalue_c(int nsize);
  bitvalue_c(const bitvalue_c &src);
  bitvalue_c(string s, int allowed_bitlength = -1);
  bitvalue_c(const EbmlBinary &elt);
  virtual ~bitvalue_c();

  bitvalue_c &operator =(const bitvalue_c &src);
  bool operator ==(const bitvalue_c &cmp) const;
  unsigned char operator [](int index) const;

  int size() const;
  void generate_random();
  unsigned char *data() const;
};

#ifdef DEBUG

class generic_packetizer_c;

typedef struct {
  uint64_t entered_at, elapsed_time, number_of_calls;
  uint64_t last_elapsed_time, last_number_of_calls;

  const char *label;
} debug_data_t;

class MTX_DLL_API debug_c {
private:
  vector<generic_packetizer_c *> packetizers;
  vector<debug_data_t *> entries;

public:
  debug_c();
  ~debug_c();

  void enter(const char *label);
  void leave(const char *label);
  void add_packetizer(void *ptzr);
  void dump_info();
};

extern debug_c MTX_DLL_API debug_facility;

#define debug_enter(func) debug_facility.enter(func)
#define debug_leave(func) debug_facility.leave(func)

#else // DEBUG

#define debug_enter(func)
#define debug_leave(func)

#endif // DEBUG

#endif // __COMMON_H
