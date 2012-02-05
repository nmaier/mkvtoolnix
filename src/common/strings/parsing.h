/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions for string parsing functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_STRING_PARSING_H
#define __MTX_COMMON_STRING_PARSING_H

#include "common/os.h"

#include <string>

extern std::string timecode_parser_error;
extern bool parse_timecode(const std::string &s, int64_t &timecode, bool allow_negative = false);

bool parse_int(const char *s, int64_t &value);
bool parse_int(const char *s, int &value);
inline bool
parse_int(const std::string &s,
          int64_t &value) {
  return parse_int(s.c_str(), value);
}
inline bool
parse_int(const std::string &s,
          int &value) {
  return parse_int(s.c_str(), value);
}
bool parse_uint(const char *s, uint64_t &value);
bool parse_uint(const char *s, uint32_t &value);
inline bool
parse_uint(const std::string &s,
           uint64_t &value) {
  return parse_uint(s.c_str(), value);
}
inline bool
parse_uint(const std::string &s,
           uint32_t &value) {
  return parse_uint(s.c_str(), value);
}

bool parse_double(const char *s, double &value);
inline bool
parse_double(const std::string &s,
             double &value) {
  return parse_double(s.c_str(), value);
}

bool parse_bool(std::string value);

uint64_t from_hex(const std::string &data);

#endif  // __MTX_COMMON_STRING_PARSING_H
