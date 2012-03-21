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

#include "common/memory.h"
#include "common/strings/editing.h"

const std::string empty_string("");

std::vector<std::string>
split(std::string const &text,
      boost::regex const &pattern,
      size_t max,
      boost::match_flag_type match_flags) {
  std::vector<std::string> results;

  auto match = boost::make_regex_iterator(text, pattern, match_flags);
  boost::sregex_iterator end;
  size_t previous_match_end = 0;

  while (   (match != end)
         && (   (0 == max)
             || ((results.size() + 1) < max))) {
    results.push_back(text.substr(previous_match_end, match->position(static_cast<boost::sregex_iterator::value_type::size_type>(0)) - previous_match_end));
    previous_match_end = match->position(static_cast<boost::sregex_iterator::value_type::size_type>(0)) + match->length(0);
    ++match;
  }

  if (previous_match_end <= text.size())
    results.push_back(text.substr(std::min(previous_match_end, std::max<size_t>(text.size(), 1) - 1), text.size() - previous_match_end));

  return results;
}

std::string
join(const char *pattern,
     const std::vector<std::string> &strings) {
  if (strings.empty())
    return "";

  std::string dst = strings[0];
  size_t i;
  for (i = 1; i < strings.size(); i++) {
    dst += pattern;
    dst += strings[i];
  }

  return dst;
}

void
strip_back(std::string &s,
           bool newlines) {
  const char *c = s.c_str();
  int len       = s.length();
  int i         = 0;

  if (newlines)
    while ((i < len) && (isblanktab(c[len - i - 1]) || iscr(c[len - i - 1])))
      ++i;
  else
    while ((i < len) && isblanktab(c[len - i - 1]))
      ++i;

  if (i > 0)
    s.erase(len - i, i);
}

void
strip(std::string &s,
      bool newlines) {
  const char *c = s.c_str();
  int i         = 0;

  if (newlines)
    while ((c[i] != 0) && (isblanktab(c[i]) || iscr(c[i])))
      i++;
  else
    while ((c[i] != 0) && isblanktab(c[i]))
      i++;

  if (i > 0)
    s.erase(0, i);

  strip_back(s, newlines);
}

void
strip(std::vector<std::string> &v,
      bool newlines) {
  size_t i;

  for (i = 0; i < v.size(); i++)
    strip(v[i], newlines);
}

std::string &
shrink_whitespace(std::string &s) {
  size_t i                     = 0;
  bool previous_was_whitespace = false;
  while (s.length() > i) {
    if (!isblanktab(s[i])) {
      previous_was_whitespace = false;
      ++i;
      continue;
    }
    if (previous_was_whitespace)
      s.erase(i, 1);
    else {
      previous_was_whitespace = true;
      ++i;
    }
  }

  return s;
}

std::string
escape(const std::string &source) {
  std::string dst;
  std::string::const_iterator src;

  for (src = source.begin(); src < source.end(); src++) {
    if (*src == '\\')
      dst += "\\\\";
    else if (*src == '"')
      dst += "\\2";               // Yes, this IS a trick ;)
    else if (*src == ' ')
      dst += "\\s";
    else if (*src == ':')
      dst += "\\c";
    else if (*src == '#')
      dst += "\\h";
    else
      dst += *src;
  }

  return dst;
}

std::string
unescape(const std::string &source) {
  std::string dst;
  std::string::const_iterator src, next_char;

  src = source.begin();
  next_char = src + 1;
  while (src != source.end()) {
    if (*src == '\\') {
      if (next_char == source.end()) // This is an error...
        dst += '\\';
      else {
        if (*next_char == '2')
          dst += '"';
        else if (*next_char == 's')
          dst += ' ';
        else if (*next_char == 'c')
          dst += ':';
        else if (*next_char == 'h')
          dst += '#';
        else
          dst += *next_char;
        src++;
      }
    } else
      dst += *src;
    src++;
    next_char = src + 1;
  }

  return dst;
}

std::string
get_displayable_string(const char *src,
                       int max_len) {
  std::string result;
  int len = (-1 == max_len) ? strlen(src) : max_len;

  for (int i = 0; i < len; ++i)
    result += (' ' > src[i]) ? '?' : src[i];

  return result;
}

size_t
utf8_strlen(const std::string &s) {
  size_t length = 0;
  size_t index  = 0;

  while (s.length() > index) {
    unsigned char c  = s[index];
    size_t num_bytes = ((c & 0x80) == 0x00) ?  1
                     : ((c & 0xe0) == 0xc0) ?  2
                     : ((c & 0xf0) == 0xe0) ?  3
                     : ((c & 0xf8) == 0xf0) ?  4
                     : ((c & 0xfc) == 0xf8) ?  5
                     : ((c & 0xfe) == 0xfc) ?  6
                     :                        99;

    if (99 == num_bytes)
      mxerror(boost::format(Y("utf8_strlen(): Invalid UTF-8 char. First byte: 0x%|1$02x|")) % static_cast<unsigned int>(c));

    ++length;
    index += num_bytes;
  }

  return length;
}
