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

#include "common/common_pch.h"

#include <stdarg.h>

std::vector<std::string> split(const char *src, const char *pattern = ",", int max_num = -1);
inline std::vector<std::string>
split(const std::string &src,
      const std::string &pattern = std::string(","),
      int max_num = -1) {
  return split(src.c_str(), pattern.c_str(), max_num);
}

std::string join(const char *pattern, const std::vector<std::string> &strings);

void strip(std::string &s, bool newlines = false);
void strip(std::vector<std::string> &v, bool newlines = false);
void strip_back(std::string &s, bool newlines = false);

std::string &shrink_whitespace(std::string &s);

std::string escape(const std::string &src);
std::string unescape(const std::string &src);

std::string get_displayable_string(const char *src, int max_len = -1);

extern const std::string empty_string;

int get_arg_len(const char *fmt, ...);
int get_varg_len(const char *fmt, va_list ap);

size_t utf8_strlen(const std::string &s);

#endif  // __MTX_COMMON_STRINGS_H
