/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   string helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_STRINGS_EDITING_H
#define MTX_COMMON_STRINGS_EDITING_H

#include "common/common_pch.h"

#include <stdarg.h>

std::vector<std::string> split(std::string const &text, boost::regex const &pattern, size_t max = 0, boost::match_flag_type match_flags = boost::match_default);

inline std::vector<std::string>
split(std::string const &text,
      std::string const &pattern = ",",
      size_t max = 0) {
  return split(text, boost::regex(std::string("\\Q") + pattern, boost::regex::perl), max);
}

std::string join(const char *pattern, const std::vector<std::string> &strings);

void strip(std::string &s, bool newlines = false);
std::string strip_copy(std::string const &s, bool newlines = false);
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

#endif  // MTX_COMMON_STRINGS_EDITING_H
