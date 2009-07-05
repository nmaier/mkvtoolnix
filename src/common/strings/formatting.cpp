/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions used in all programs, helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include "common/common.h"
#include "common/strings/formatting.h"

std::string
format_timecode(int64_t timecode,
                unsigned int precision) {
  static boost::format s_bf_format("%|1$02d|:%|2$02d|:%|3$02d|");
  static boost::format s_bf_decimals(".%|1$09d|");

  std::string result = (s_bf_format
                        % (int)( timecode / 60 / 60 / 1000000000)
                        % (int)((timecode      / 60 / 1000000000) % 60)
                        % (int)((timecode           / 1000000000) % 60)).str();

  if (9 < precision)
    precision = 9;

  if (precision) {
    std::string decimals = (s_bf_decimals % (int)(timecode % 1000000000)).str();

    if (decimals.length() > (precision + 1))
      decimals.erase(precision + 1);

    result += decimals;
  }

  return result;
}

static boost::format s_bf_single_value("%1%");

std::string
to_string(int64_t value) {
  return (s_bf_single_value % value).str();
}

std::string
to_string(int value) {
  return (s_bf_single_value % value).str();
}

std::string
to_string(uint64_t value) {
  return (s_bf_single_value % value).str();
}

std::string
to_string(unsigned int value) {
  return (s_bf_single_value % value).str();
}

std::string
to_string(double value,
          unsigned int precision) {
  int64_t scale = 1;
  for (int i = 0; i < precision; ++i)
    scale *= 10;

  return to_string((int64_t)(value * scale), scale, precision);
}

std::string
to_string(int64_t numerator,
          int64_t denominator,
          unsigned int precision) {
  std::string output      = (s_bf_single_value % (numerator / denominator)).str();
  int64_t fractional_part = numerator % denominator;

  if (0 != fractional_part) {
    static boost::format s_bf_precision_format_format(".%%0%1%d");

    std::string format         = (s_bf_precision_format_format % precision).str();
    output                    += (boost::format(format) % fractional_part).str();
    std::string::iterator end  = output.end() - 1;

    while (*end == '0')
      --end;
    if (*end == '.')
      --end;

    output.erase(end + 1, output.end());
  }

  return output;
}

void
fix_format(const char *fmt,
           std::string &new_fmt) {
#if defined(COMP_MINGW) || defined(COMP_MSC)
  new_fmt    = "";
  int len    = strlen(fmt);
  bool state = false;
  int i;

  for (i = 0; i < len; i++) {
    if (fmt[i] == '%') {
      state = !state;
      new_fmt += '%';

    } else if (!state)
      new_fmt += fmt[i];

    else {
      if (((i + 3) <= len) && (fmt[i] == 'l') && (fmt[i + 1] == 'l') &&
          ((fmt[i + 2] == 'u') || (fmt[i + 2] == 'd'))) {
        new_fmt += "I64";
        new_fmt += fmt[i + 2];
        i += 2;
        state = false;

      } else {
        new_fmt += fmt[i];
        if (isalpha(fmt[i]))
          state = false;
      }
    }
  }

#else

  new_fmt = fmt;

#endif
}

