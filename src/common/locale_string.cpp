/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   locale string splitting and creation

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/locale_string.h"

locale_string_c::locale_string_c(std::string locale_string) {
  boost::regex locale_re("^([[:alpha:]]+)?(_[[:alpha:]]+)?(\\.[^@]+)?(@.+)?", boost::regex::perl);
  boost::smatch matches;

  if (!boost::regex_match(locale_string, matches, locale_re))
    throw mtx::locale_string_format_x(locale_string);

  m_language  = matches[1].str();
  m_territory = matches[2].str();
  m_codeset   = matches[3].str();
  m_modifier  = matches[4].str();

  if (!m_territory.empty())
    m_territory.erase(0, 1);

  if (!m_codeset.empty())
    m_codeset.erase(0, 1);

  if (!m_modifier.empty())
    m_modifier.erase(0, 1);
}

locale_string_c &
locale_string_c::set_codeset_and_modifier(const locale_string_c &src) {
  m_codeset  = src.m_codeset;
  m_modifier = src.m_modifier;

  return *this;
}

std::string
locale_string_c::str(eval_type_e type) {
  std::string locale = m_language;

  if ((type & territory) && !m_territory.empty())
    locale += std::string("_") + m_territory;

  if ((type & codeset) && !m_codeset.empty())
    locale += std::string(".") + m_codeset;

  if ((type & modifier) && !m_modifier.empty())
    locale += std::string("@") + m_modifier;

  return locale;
}

