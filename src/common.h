/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  common.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief definitions used in all programs, helper functions
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __COMMON_H
#define __COMMON_H

#include "os.h"

#include <stdarg.h>
#include <stdint.h>

#include <vector>

#include <ebml/EbmlUnicodeString.h>
#include "config.h"

#define VERSIONINFO "mkvmerge v" VERSION

using namespace std;
using namespace libebml;

#define DISPLAYPRIORITY_HIGH   10
#define DISPLAYPRIORITY_MEDIUM  5
#define DISPLAYPRIORITY_LOW     1

/* errors */
#define EMOREDATA    -1
#define EMALLOC      -2
#define EBADHEADER   -3
#define EBADEVENT    -4
#define EOTHER       -5

/* types */
#define TYPEUNKNOWN   0
#define TYPEOGM       1
#define TYPEAVI       2
#define TYPEWAV       3
#define TYPESRT       4
#define TYPEMP3       5
#define TYPEAC3       6
#define TYPECHAPTERS  7
#define TYPEMICRODVD  8
#define TYPEVOBSUB    9
#define TYPEMATROSKA 10
#define TYPEDTS      11
#define TYPEAAC      12
#define TYPESSA      13
#define TYPEREAL     14

#define FOURCC(a, b, c, d) (uint32_t)((((unsigned char)a) << 24) + \
                           (((unsigned char)b) << 16) + \
                           (((unsigned char)c) << 8) + \
                           ((unsigned char)d))

#define TIMECODE_SCALE 1000000

#define MXMSG_INFO      1
#define MXMSG_WARNING   2
#define MXMSG_ERROR     4
#define MXMSG_DEBUG     8

void die(const char *fmt, ...);
void mxprint(void *stream, const char *fmt, ...);
void mxprints(char *dst, const char *fmt, ...);
void fix_format(const char *fmt, string &new_fmt);
void mxwarn(const char *fmt, ...);
void mxerror(const char *fmt, ...);
void mxinfo(const char *fmt, ...);
void mxdebug(const char *fmt, ...);
void mxexit(int code = -1);

#define trace() _trace(__func__, __FILE__, __LINE__)
void _trace(const char *func, const char *file, int line);

#define get_fourcc(b) get_uint32_be(b)
uint16_t get_uint16(const void *buf);
uint32_t get_uint32(const void *buf);
uint64_t get_uint64(const void *buf);
uint16_t get_uint16_be(const void *buf);
uint32_t get_uint32_be(const void *buf);
uint64_t get_uint64_be(const void *buf);
void put_uint16(void *buf, uint16_t value);
void put_uint32(void *buf, uint32_t value);
void put_uint64(void *buf, uint64_t value);

extern int cc_local_utf8;

int utf8_init(const char *charset);
void utf8_done();
char *to_utf8(int handle, const char *local);
char *from_utf8(int handle, const char *utf8);

bool is_unique_uint32(uint32_t number);
void add_unique_uint32(uint32_t number);
uint32_t create_unique_uint32();

#define safefree(p) if ((p) != NULL) free(p);
#define safemalloc(s) _safemalloc(s, __FILE__, __LINE__)
void *_safemalloc(size_t size, const char *file, int line);
#define safestrdup(s) _safestrdup(s, __FILE__, __LINE__)
char *_safestrdup(const char *s, const char *file, int line);
unsigned char *_safestrdup(const unsigned char *s, const char *file, int line);
#define safememdup(src, size) _safememdup(src, size, __FILE__, __LINE__)
void *_safememdup(const void *src, size_t size, const char *file, int line);
#define saferealloc(mem, size) _saferealloc(mem, size, __FILE__, __LINE__)
void *_saferealloc(void *mem, size_t size, const char *file, int line);

vector<string> split(const char *src, const char *pattern = ",",
                     int max_num = -1);
void strip(string &s, bool newlines = false);
void strip(vector<string> &v, bool newlines = false);

UTFstring cstr_to_UTFstring(const char *c);
UTFstring cstrutf8_to_UTFstring(const char *c);
char *UTFstring_to_cstr(const UTFstring &u);
char *UTFstring_to_cstrutf8(const UTFstring &u);

bool parse_int(const char *s, int64_t &value);
bool parse_int(const char *s, int &value);
string to_string(int64_t i);

extern int verbose;

class bit_cursor_c {
private:
  const unsigned char *end_of_data;
  const unsigned char *byte_position;
  const unsigned char *start_of_data;
  unsigned int bits_valid;

  bool out_of_data;

public:
  bit_cursor_c(const unsigned char *data, unsigned int len):
    end_of_data(data + len), byte_position(data), start_of_data(data),
    bits_valid(8), out_of_data(false) {
    if (byte_position >= end_of_data)
      out_of_data = true;
  }

  bool get_bits(unsigned int n, unsigned long &r) {
    // returns false if less bits are available than asked for
    r = 0;

    while (n > 0) {
      if (byte_position >= end_of_data) {
        out_of_data = true;
        return false;
      }

      unsigned int b = 8; // number of bits to extract from the current byte
      if (b > n)
        b = n;
      if (b > bits_valid)
        b = bits_valid;

      unsigned int rshift = bits_valid-b;

      r <<= b;
      r |= ((*byte_position) >> rshift) & (0xff >> (8 - b));

      bits_valid -= b;
      if (bits_valid == 0) {
        bits_valid = 8;
        byte_position += 1;
      }

      n -= b;
    }

    return true;
  }

  bool get_bits(unsigned int n, int &r) {
    unsigned long t;
    bool b = get_bits(n, t);
    r = (int)t;
    return b;
  }

  bool get_bits(unsigned int n, unsigned int &r) {
    unsigned long t;
    bool b = get_bits(n, t);
    r = (unsigned int)t;
    return b;
  }

  bool get_bit(bool &r) {
    unsigned long t;
    bool b = get_bits(1, t);
    r = (bool)t;
    return b;
  }

  bool byte_align() {
    if (out_of_data)
      return false;
    if (bits_valid == 8)
      return true;
    bits_valid = 0;
    byte_position += 1;
    return true;
  }

  bool set_bit_position(unsigned int pos) {
    if (pos >= ((end_of_data - start_of_data) * 8))
      return false;

    byte_position = start_of_data + (pos / 8);
    bits_valid = 8 - (pos % 8);

    return true;
  }

  int get_bit_position() {
    return (byte_position - start_of_data) * 8 + 8 - bits_valid;
  }

  bool skip_bits(unsigned int num) {
    return set_bit_position(get_bit_position() + num);
  }
};

class byte_cursor_c {
private:
  int pos, size;
  const unsigned char *data;

public:
  byte_cursor_c(const unsigned char *ndata, int nsize);

  virtual unsigned char get_byte();
  virtual unsigned short get_word();
  virtual unsigned int get_dword();
  virtual void get_bytes(unsigned char *dst, int n);

  virtual void skip(int n);

  virtual int get_pos();
  virtual int get_len();
};

class bitvalue_c {
private:
  unsigned char *value;
  int bitsize;
public:
  bitvalue_c(int nsize);
  bitvalue_c(const bitvalue_c &src);
  bitvalue_c(const char *s, int allowed_bitlength = -1);
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

class debug_c {
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

  static void dump_elements(EbmlElement *e, int level);
};

extern debug_c debug_facility;

#define debug_enter(func) debug_facility.enter(func)
#define debug_leave(func) debug_facility.leave(func)

#else // DEBUG

#define debug_enter(func)
#define debug_leave(func)

#endif // DEBUG

#endif // __COMMON_H
