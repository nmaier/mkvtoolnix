/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   string helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_STRINGS_H
#define __MTX_COMMON_STRINGS_H

#include "common/os.h"

#include <boost/algorithm/string.hpp>
#include <string>
#include <vector>

#include <stdarg.h>

namespace ba = boost::algorithm;

std::vector<std::string> MTX_DLL_API split(const char *src, const char *pattern = ",", int max_num = -1);
inline std::vector<std::string>
split(const std::string &src,
      const std::string &pattern = std::string(","),
      int max_num = -1) {
  return split(src.c_str(), pattern.c_str(), max_num);
}

std::string MTX_DLL_API join(const char *pattern, const std::vector<std::string> &strings);

void MTX_DLL_API strip(std::string &s, bool newlines = false);
void MTX_DLL_API strip(std::vector<std::string> &v, bool newlines = false);
void MTX_DLL_API strip_back(std::string &s, bool newlines = false);

std::string &MTX_DLL_API shrink_whitespace(std::string &s);

std::string MTX_DLL_API escape(const std::string &src);
std::string MTX_DLL_API unescape(const std::string &src);

std::string MTX_DLL_API get_displayable_string(const char *src, int max_len = -1);

bool MTX_DLL_API starts_with(const std::string &s, const char *start, int maxlen = -1);
inline bool
starts_with(const std::string &s,
            const std::string &start,
            int maxlen = -1) {
  return starts_with(s, start.c_str(), maxlen);
}

bool MTX_DLL_API starts_with_case(const std::string &s, const char *start, int maxlen = -1);
inline bool
starts_with_case(const std::string &s,
                 const std::string &start,
                 int maxlen = -1) {
  return starts_with_case(s, start.c_str(), maxlen);
}

extern const std::string MTX_DLL_API empty_string;

int MTX_DLL_API get_arg_len(const char *fmt, ...);
int MTX_DLL_API get_varg_len(const char *fmt, va_list ap);

size_t MTX_DLL_API utf8_strlen(const std::string &s);

#endif  // __MTX_COMMON_STRINGS_H
