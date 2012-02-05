/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   string parsing helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <errno.h>
#include <locale.h>
#include <stdarg.h>

#include "common/strings/formatting.h"
#include "common/strings/parsing.h"

bool
parse_int(const char *s,
          int64_t &value) {
  if (*s == 0)
    return false;

  int sign      = 1;
  const char *p = s;
  value         = 0;

  if (*p == '-') {
    sign = -1;
    p++;
  }
  while (*p != 0) {
    if (!isdigit(*p))
      return false;
    value *= 10;
    value += *p - '0';
    p++;
  }
  value *= sign;

  return true;
}

bool
parse_int(const char *s,
          int &value) {
  int64_t tmp = 0;
  bool result = parse_int(s, tmp);
  value       = tmp;

  return result;
}

bool
parse_uint(const char *s,
           uint64_t &value) {
  if (*s == 0)
    return false;

  value = 0;
  const char *p = s;
  while (*p != 0) {
    if (!isdigit(*p))
      return false;
    value *= 10;
    value += *p - '0';
    p++;
  }

  return true;
}

bool
parse_uint(const char *s,
           uint32_t &value) {
  uint64_t tmp = 0;
  bool result  = parse_uint(s, tmp);
  value        = tmp;

  return result;
}

bool
parse_double(const char *s,
             double &value) {
  std::string old_locale = setlocale(LC_NUMERIC, "C");
  bool ok                = true;
  char *endptr           = NULL;
  value                  = strtod(s, &endptr);
  if (endptr != NULL) {
    if ((value == 0.0) && (endptr == s))
      ok = false;
    else if (*endptr != 0)
      ok = false;
  }
  if (errno == ERANGE)
    ok = false;

  setlocale(LC_NUMERIC, old_locale.c_str());

  return ok;
}

std::string timecode_parser_error;

inline bool
set_tcp_error(const std::string &error) {
  timecode_parser_error = error;
  return false;
}

inline bool
set_tcp_error(const boost::format &error) {
  timecode_parser_error = error.str();
  return false;
}

bool
parse_timecode(const std::string &src,
               int64_t &timecode,
               bool allow_negative) {
  // Recognized format:
  // 1. XXXXXXXu   with XXXXXX being a number followed
  //    by one of the units 's', 'ms', 'us' or 'ns'
  // 2. HH:MM:SS.nnnnnnnnn  with up to nine digits 'n' for ns precision;
  // HH: is optional; HH, MM and SS can be either one or two digits.
  // 2. HH:MM:SS:nnnnnnnnn  with up to nine digits 'n' for ns precision;
  // HH: is optional; HH, MM and SS can be either one or two digits.
  int h, m, s, n, values[4], num_values, num_digits, num_colons;
  size_t offset = 0, negative = 1, i;
  bool decimal_point_found;

  if (src.empty())
    return false;

  if ('-' == src[0]) {
    if (!allow_negative)
      return false;
    negative = -1;
    offset = 1;
  }

  try {
    if (src.length() < (2 + offset))
      throw false;

    std::string unit = src.substr(src.length() - 2, 2);

    int64_t multiplier = 1000000000;
    size_t unit_length = 2;
    int64_t value      = 0;

    if (unit == "ms")
      multiplier = 1000000;
    else if (unit == "us")
      multiplier = 1000;
    else if (unit == "ns")
      multiplier = 1;
    else if (unit.substr(1, 1) == "s")
      unit_length = 1;
    else
      throw false;

    if (src.length() < (unit_length + 1 + offset))
      throw false;

    if (!parse_int(src.substr(offset, src.length() - unit_length - offset), value))
      throw false;

    timecode = value * multiplier * negative;

    return true;
  } catch (...) {
  }

  num_digits = 0;
  num_colons = 0;
  num_values = 1;
  decimal_point_found = false;
  memset(&values, 0, sizeof(int) * 4);

  for (i = offset; src.length() > i; ++i) {
    if (isdigit(src[i])) {
      if (decimal_point_found && (9 == num_digits))
        return set_tcp_error(Y("Invalid format: More than nine nano-second digits"));
      values[num_values - 1] = values[num_values - 1] * 10 + src[i] - '0';
      ++num_digits;

    } else if (('.' == src[i]) ||
               ((':' == src[i]) && (2 == num_colons))) {
      if (decimal_point_found)
        return set_tcp_error(Y("Invalid format: Second decimal point after first decimal point"));

      if (0 == num_digits)
        return set_tcp_error(Y("Invalid format: No digits before decimal point"));
      ++num_values;
      num_digits = 0;
      decimal_point_found = true;

    } else if (':' == src[i]) {
      if (decimal_point_found)
        return set_tcp_error(Y("Invalid format: Colon inside nano-second part"));
      if (2 == num_colons)
        return set_tcp_error(Y("Invalid format: More than two colons"));
      if (0 == num_digits)
        return set_tcp_error(Y("Invalid format: No digits before colon"));
      ++num_colons;
      ++num_values;
      num_digits = 0;

    } else
      return set_tcp_error(boost::format(Y("Invalid format: unknown character '%1%' found")) % src[i]);
  }

  if (1 > num_colons)
    return set_tcp_error(Y("Invalid format: At least minutes and seconds have to be given, but no colon was found"));

  if ((':' == src[src.length() - 1]) || ('.' == src[src.length() - 1]))
    return set_tcp_error(Y("Invalid format: The last character is a colon or a decimal point instead of a digit"));

  // No error has been found. Now find out whoich parts have been
  // set and which haven't.

  if (4 == num_values) {
    h = values[0];
    m = values[1];
    s = values[2];
    n = values[3];
    for (; 9 > num_digits; ++num_digits)
      n *= 10;

  } else if (2 == num_values) {
    h = 0;
    m = values[0];
    s = values[1];
    n = 0;

  } else if (decimal_point_found) {
    h = 0;
    m = values[0];
    s = values[1];
    n = values[2];
    for (; 9 > num_digits; ++num_digits)
      n *= 10;

  } else {
    h = values[0];
    m = values[1];
    s = values[2];
    n = 0;

  }

  if (m > 59)
    return set_tcp_error(boost::format(Y("Invalid number of minutes: %1% > 59")) % m);
  if (s > 59)
    return set_tcp_error(boost::format(Y("Invalid number of seconds: %1% > 59")) % s);

  timecode              = (((int64_t)h * 60 * 60 + (int64_t)m * 60 + (int64_t)s) * 1000000000ll + n) * negative;
  timecode_parser_error = Y("no error");

  return true;
}

/** \brief Parse a string for a boolean value

   Interpretes the string \c orig as a boolean value. Accepted
   is "true", "yes", "1" as boolean true and "false", "no", "0"
   as boolean false.
*/
bool
parse_bool(std::string value) {
  ba::to_lower(value);

  if ((value == "yes") || (value == "true") || (value == "1"))
    return true;
  if ((value == "no") || (value == "false") || (value == "0"))
    return false;
  throw false;
}

uint64_t
from_hex(const std::string &data) {
  const char *s = data.c_str();
  if (*s == 0)
    throw std::bad_cast();

  uint64_t value = 0;

  while (*s) {
    unsigned int digit = isdigit(*s)                  ? *s - '0'
                       : (('a' <= *s) && ('f' >= *s)) ? *s - 'a' + 10
                       : (('A' <= *s) && ('F' >= *s)) ? *s - 'A' + 10
                       :                                16;
    if (16 == digit)
      throw std::bad_cast();

    value = (value << 4) + digit;
    ++s;
  }

  return value;
}
