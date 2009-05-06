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
#include "common/string_formatting.h"

std::string
format_timecode(int64_t timecode,
                unsigned int precision) {
  std::string result = (boost::format("%|1$02d|:%|2$02d|:%|3$02d|")
                        % (int)( timecode / 60 / 60 / 1000000000)
                        % (int)((timecode      / 60 / 1000000000) % 60)
                        % (int)((timecode           / 1000000000) % 60)).str();

  if (9 < precision)
    precision = 9;

  if (precision) {
    std::string decimals = (boost::format(".%|1$09d|") % (int)(timecode % 1000000000)).str();

    if (decimals.length() > (precision + 1))
      decimals.erase(precision + 1);

    result += decimals;
  }

  return result;
}

std::string
to_string(int64_t i) {
  return (boost::format("%1%") % i).str();
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

