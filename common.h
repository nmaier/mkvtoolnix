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
    \version \$Id: common.h,v 1.32 2003/05/26 21:49:11 mosu Exp $
    \brief definitions used in all programs, helper functions
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __COMMON_H
#define __COMMON_H

#ifndef __CYGWIN__
#include <stdint.h>
#elif defined WIN32
#include <stdint.h>
#define PACKAGE "mkvtoolnix"
#define VERSION 0.4.1
#endif
#include <sys/types.h>

#include <vector>

#ifdef WIN32
#define strncasecmp _strnicmp
#define strcasecmp _stricmp
#define nice(a)
#endif

#include "EbmlUnicodeString.h"

#include "config.h"

using namespace std;

#define VERSIONINFO "mkvmerge v" VERSION

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

#define FOURCC(a, b, c, d) (uint32_t)((((unsigned char)a) << 24) + \
                           (((unsigned char)b) << 16) + \
                           (((unsigned char)c) << 8) + \
                           ((unsigned char)d))

#define TIMECODE_SCALE 1000000

#define die(fmt, args...) _die(fmt, __FILE__, __LINE__, ## args)
void _die(const char *fmt, const char *file, int line, ...);

#define trace() _trace(__func__, __FILE__, __LINE__)
void _trace(const char *func, const char *file, int line);

uint16_t get_uint16(const void *buf);
uint32_t get_uint32(const void *buf);
uint64_t get_uint64(const void *buf);

extern int cc_local_utf8;

int utf8_init(char *charset);
void utf8_done();
char *to_utf8(int handle, char *local);
char *from_utf8(int handle, char *utf8);

int is_unique_uint32(uint32_t number);
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

UTFstring cstr_to_UTFstring(const char *c);
char *UTFstring_to_cstr(const UTFstring &u);

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
    end_of_data(data+len), byte_position(data), start_of_data(data),
    bits_valid(8), out_of_data(false) {
    if (byte_position >= end_of_data)
      out_of_data = true;
  }

  bool get_bits(unsigned int n, unsigned long &r) {
    // returns true if less bits are available than asked for
    r = 0;

    while (n > 0) {
      if (byte_position >= end_of_data) {
        out_of_data = true;
        return true;
      }

      unsigned int b = 8; // number of bits to extract from the current byte
      if (b > n)
        b = n;
      if (b > bits_valid)
        b = bits_valid;

      unsigned int rshift = bits_valid-b;

      r <<= b;
      r |= ((*byte_position) >> rshift) & (0xff >> (8-b));

      bits_valid -= b;
      if (bits_valid == 0) {
        bits_valid = 8;
        byte_position += 1;
      }

      n -= b;
    }

    return false;
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
      return true;
    if (bits_valid == 8)
      return false;
    bits_valid = 0;
    byte_position += 1;
    return false;
  }

  int get_bit_position() {
    return byte_position - start_of_data + 8 - bits_valid;
  }
};

#ifdef DEBUG

class debug_c {
private:
  uint64_t entered_at, elapsed_time, number_of_calls;
  uint64_t last_elapsed_time, last_number_of_calls;

  const char *label;
public:
  debug_c(const char *nlabel);

  static void enter(const char *label);
  static void leave(const char *label);
  static void add_packetizer(void *ptzr);
  static void dump_info();
};

#define debug_enter(func) debug_c::enter(func)
#define debug_leave(func) debug_c::leave(func)

#else // DEBUG

#define debug_enter(func)
#define debug_leave(func)

#endif // DEBUG

#endif // __COMMON_H
