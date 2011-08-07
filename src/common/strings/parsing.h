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

extern std::string MTX_DLL_API timecode_parser_error;
extern bool MTX_DLL_API parse_timecode(const std::string &s, int64_t &timecode, bool allow_negative = false);

bool MTX_DLL_API parse_int(const char *s, int64_t &value);
bool MTX_DLL_API parse_int(const char *s, int &value);
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
bool MTX_DLL_API parse_uint(const char *s, uint64_t &value);
bool MTX_DLL_API parse_uint(const char *s, uint32_t &value);
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

bool MTX_DLL_API parse_double(const char *s, double &value);
inline bool
parse_double(const std::string &s,
             double &value) {
  return parse_double(s.c_str(), value);
}

bool MTX_DLL_API parse_bool(std::string value);

#endif  // __MTX_COMMON_STRING_PARSING_H
