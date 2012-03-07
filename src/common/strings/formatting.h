/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   string formatting functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_STRING_FORMATTING_H
#define __MTX_COMMON_STRING_FORMATTING_H

#include "common/common_pch.h"

#include "common/strings/editing.h"

#define FMT_TIMECODE "%02d:%02d:%02d.%03d"
#define ARG_TIMECODEINT(t) (int32_t)( (t) / 60 /   60 / 1000),         \
                           (int32_t)(((t) / 60        / 1000) %   60), \
                           (int32_t)(((t)             / 1000) %   60), \
                           (int32_t)( (t)                     % 1000)
#define ARG_TIMECODE(t)    ARG_TIMECODEINT((int64_t)(t))
#define ARG_TIMECODE_NS(t) ARG_TIMECODE((t) / 1000000)
#define FMT_TIMECODEN "%02d:%02d:%02d.%09d"
#define ARG_TIMECODENINT(t) (int32_t)( (t) / 60 / 60 / 1000000000),               \
                            (int32_t)(((t) / 60      / 1000000000) %         60), \
                            (int32_t)(((t)           / 1000000000) %         60), \
                            (int32_t)( (t)                         % 1000000000)
#define ARG_TIMECODEN(t) ARG_TIMECODENINT((int64_t)(t))

#define WRAP_AT_TERMINAL_WIDTH -1

std::string format_timecode(int64_t timecode, unsigned int precision = 9);
std::string format_paragraph(const std::string &text_to_wrap,
                             int indent_column                    = 0,
                             const std::string &indent_first_line = empty_string,
                             std::string indent_following_lines   = empty_string,
                             int wrap_column                      = WRAP_AT_TERMINAL_WIDTH,
                             const char *break_chars              = " ,.)/:");

std::wstring format_paragraph(const std::wstring &text_to_wrap,
                              int indent_column                     = 0,
                              const std::wstring &indent_first_line = L" ",
                              std::wstring indent_following_lines   = L" ",
                              int wrap_column                       = WRAP_AT_TERMINAL_WIDTH,
                              const std::wstring &break_chars       = L" ,.)/:");

void fix_format(const char *fmt, std::string &new_fmt);

std::string to_string(int value);
std::string to_string(unsigned int value);
std::string to_string(int64_t value);
std::string to_string(uint64_t value);
std::string to_string(double value, unsigned int precision);
std::string to_string(int64_t numerator, int64_t denominator, unsigned int precision);

std::string to_hex(const unsigned char *buf, size_t size, bool compact = false);
inline std::string
to_hex(const std::string &buf,
       bool compact = false) {
  return to_hex(reinterpret_cast<const unsigned char *>(buf.c_str()), buf.length(), compact);
}

std::string create_minutes_seconds_time_string(unsigned int seconds, bool omit_minutes_if_zero = false);

#endif  // __MTX_COMMON_STRING_FORMATTING_H
