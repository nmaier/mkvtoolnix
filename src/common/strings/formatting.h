/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   string formatting functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_STRINGS_FORMATTING_H
#define MTX_COMMON_STRINGS_FORMATTING_H

#include "common/common_pch.h"

#include <ostream>
#include <sstream>

#include "common/strings/editing.h"
#include "common/timecode.h"

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

template<typename T>
std::string
format_timecode(basic_timecode_c<T> const &timecode,
                unsigned int precision = 9) {
  return format_timecode(timecode.to_ns(), precision);
}

template<typename T>
std::ostream &
operator <<(std::ostream &out,
            basic_timecode_c<T> const &timecode) {
  if (timecode.valid())
    out << format_timecode(timecode);
  else
    out << "<InvTC>";
  return out;
}

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

inline std::string
to_string(std::string const &value) {
  return value;
}

std::string to_string(double value, unsigned int precision);
std::string to_string(int64_t numerator, int64_t denominator, unsigned int precision);

template<typename T>
std::string
to_string(basic_timecode_c<T> const &timecode) {
  return format_timecode(timecode.to_ns());
}


template<typename T>
std::string
to_string(T const &value) {
  std::stringstream ss;
  ss << value;
  return ss.str();
}

std::string to_hex(const unsigned char *buf, size_t size, bool compact = false);
inline std::string
to_hex(const std::string &buf,
       bool compact = false) {
  return to_hex(reinterpret_cast<const unsigned char *>(buf.c_str()), buf.length(), compact);
}
inline std::string
to_hex(memory_cptr const &buf,
       bool compact = false) {
  return to_hex(buf->get_buffer(), buf->get_size(), compact);
}

std::string create_minutes_seconds_time_string(unsigned int seconds, bool omit_minutes_if_zero = false);

#endif  // MTX_COMMON_STRINGS_FORMATTING_H
