/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions for string parsing functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_STRING_PARSING_H
#define MTX_COMMON_STRING_PARSING_H

#include "common/common_pch.h"

#include <locale.h>

#include <boost/lexical_cast.hpp>

#include "common/at_scope_exit.h"

namespace mtx { namespace conversion {

template <bool is_unsigned>
struct unsigned_checker {
  template<typename StrT>
  static inline void do_check(StrT const &) { }
};

template <>
struct unsigned_checker<true> {
  template<typename StrT>
  static inline void do_check(StrT const &str) {
    if (str[0] == '-')
      boost::throw_exception( boost::bad_lexical_cast{} );
  }
};

}}

template<typename StrT, typename ValueT>
bool
parse_number(StrT const &string,
             ValueT &value) {
  try {
    mtx::conversion::unsigned_checker< std::is_unsigned<ValueT>::value >::do_check(string);
    value = boost::lexical_cast<ValueT>(string);
    return true;
  } catch (boost::bad_lexical_cast &) {
    return false;
  }
}

template<typename StrT>
bool
parse_number(StrT const &string,
             double &value) {
  std::string old_locale = setlocale(LC_NUMERIC, "C");
  at_scope_exit_c restore_locale{ [&]() { setlocale(LC_NUMERIC, old_locale.c_str()); } };
  return parse_number<StrT, double>(string, value);
}

template<typename StrT>
bool
parse_number(StrT const &string,
             float &value) {
  std::string old_locale = setlocale(LC_NUMERIC, "C");
  at_scope_exit_c restore_locale{ [&]() { setlocale(LC_NUMERIC, old_locale.c_str()); } };
  return parse_number<StrT, float>(string, value);
}

extern std::string timecode_parser_error;
extern bool parse_timecode(const std::string &s, int64_t &timecode, bool allow_negative = false);

bool parse_bool(std::string value);

uint64_t from_hex(const std::string &data);

#endif  // MTX_COMMON_STRING_PARSING_H
