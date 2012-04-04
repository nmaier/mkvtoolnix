/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions used in all programs, helper functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/strings/formatting.h"
#include "common/strings/utf8.h"
#include "common/terminal.h"
#include "common/translation.h"

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
  for (size_t i = 0; i < precision; ++i)
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

std::wstring
format_paragraph(const std::wstring &text_to_wrap,
                 int indent_column,
                 const std::wstring &indent_first_line,
                 std::wstring indent_following_lines,
                 int wrap_column,
                 const std::wstring &break_chars) {
  std::wstring text   = indent_first_line;
  int current_column  = get_width_in_em(text);
  bool break_anywhere = translation_c::get_active_translation().m_line_breaks_anywhere;

  if (WRAP_AT_TERMINAL_WIDTH == wrap_column)
    wrap_column = get_terminal_columns() - 1;

  if ((0 != indent_column) && (current_column >= indent_column)) {
    text           += L"\n";
    current_column  = 0;
  }

  if (indent_following_lines.empty())
    indent_following_lines = std::wstring(indent_column, L' ');

  text                                += std::wstring(indent_column - current_column, L' ');
  current_column                       = indent_column;
  std::wstring::size_type current_pos  = 0;
  bool first_word_in_line              = true;
  bool needs_space                     = false;

  while (text_to_wrap.length() > current_pos) {
    std::wstring::size_type word_start = text_to_wrap.find_first_not_of(L" ", current_pos);
    if (std::string::npos == word_start)
      break;

    if (word_start != current_pos)
      needs_space = true;

    std::wstring::size_type word_end = text_to_wrap.find_first_of(break_chars, word_start);
    bool next_needs_space            = false;
    if (std::wstring::npos == word_end)
      word_end = text_to_wrap.length();

    else if (text_to_wrap[word_end] != L' ')
      ++word_end;

    else
      next_needs_space = true;

    std::wstring word    = text_to_wrap.substr(word_start, word_end - word_start);
    bool needs_space_now = needs_space && (text_to_wrap.substr(word_start, 1).find_first_of(break_chars) == std::wstring::npos);
    size_t word_length   = get_width_in_em(word);
    size_t new_column    = current_column + (needs_space_now ? 0 : 1) + word_length;

    if (break_anywhere && (new_column >= static_cast<size_t>(wrap_column))) {
      size_t offset = 0;
      while (((word_end - 1) > word_start) && ((128 > text_to_wrap[word_end - 1]) || ((new_column - offset) >= static_cast<size_t>(wrap_column)))) {
        offset   += get_width_in_em(text_to_wrap[word_end - 1]);
        word_end -= 1;
      }

      if (0 != offset)
        next_needs_space = false;

      word_length -= offset;
      new_column  -= offset;
      word.erase(word_end - word_start);
    }

    if (!first_word_in_line && (new_column >= static_cast<size_t>(wrap_column))) {
      text               += L"\n" + indent_following_lines;
      current_column      = indent_column;
      first_word_in_line  = true;
    }

    if (!first_word_in_line && needs_space_now) {
      text += L" ";
      ++current_column;
    }

    text               += word;
    current_column     += word_length;
    current_pos         = word_end;
    first_word_in_line  = false;
    needs_space         = next_needs_space;
  }

  text += L"\n";

  return text;
}

std::string
format_paragraph(const std::string &text_to_wrap,
                 int indent_column,
                 const std::string &indent_first_line,
                 std::string indent_following_lines,
                 int wrap_column,
                 const char *break_chars) {
  return to_utf8(format_paragraph(to_wide(text_to_wrap), indent_column, to_wide(indent_first_line), to_wide(indent_following_lines), wrap_column, to_wide(break_chars)));
}

std::string
to_hex(const unsigned char *buf,
       size_t size,
       bool compact) {
  static boost::format s_bf_to_hex("0x%|1$02x|");
  static boost::format s_bf_to_hex_compact("%|1$02x|");

  std::string hex;
  for (size_t idx = 0; idx < size; ++idx)
    hex += (compact || hex.empty() ? std::string{""} : std::string{" "}) + ((compact ? s_bf_to_hex_compact : s_bf_to_hex) % static_cast<unsigned int>(buf[idx])).str();

  return hex;
}

std::string
create_minutes_seconds_time_string(unsigned int seconds,
                                   bool omit_minutes_if_zero) {
  unsigned int minutes = seconds / 60;
  seconds              = seconds % 60;

  std::string  result  = (boost::format(NY("%1% second", "%1% seconds", seconds)) % seconds).str();

  if (!minutes && omit_minutes_if_zero)
    return result;

  return (  boost::format("%1% %2%")
          % (boost::format(NY("%1% minute", "%1% minutes", minutes)) % minutes)
          % result).str();
}
