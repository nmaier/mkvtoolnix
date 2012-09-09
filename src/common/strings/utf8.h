/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Definitions for locale handling functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_STRINGS_UTF8_H
#define MTX_COMMON_STRINGS_UTF8_H

#include "common/common_pch.h"

#include <ebml/EbmlUnicodeString.h>

std::wstring to_wide(const std::string &source);

inline std::wstring
to_wide(wchar_t const *source) {
  return source;
}

inline std::wstring
to_wide(std::wstring const &source) {
  return source;
}

inline std::wstring
to_wide(boost::wformat const &source) {
  return source.str();
}

inline std::wstring
to_wide(::libebml::UTFstring const &source) {
  return source.c_str();
}

std::string to_utf8(const std::wstring &source);

inline std::string
to_utf8(char const *source) {
  return source;
}

inline std::string
to_utf8(std::string const &source) {
  return source;
}

inline std::string
to_utf8(boost::format const &source) {
  return source.str();
}

inline std::string
to_utf8(::libebml::UTFstring const &source) {
  return source.GetUTF8();
}

size_t get_width_in_em(wchar_t c);
size_t get_width_in_em(const std::wstring &s);

#endif  // MTX_COMMON_STRINGS_UTF8_H
