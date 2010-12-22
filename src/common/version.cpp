/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   version information

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <boost/regex.hpp>

#include "common/strings/parsing.h"
#include "common/version.h"

#define VERSIONNAME "Die Wiederkehr"

version_number_t::version_number_t()
  : valid(false)
{
  memset(parts, 0, 5 * sizeof(unsigned int));
}

version_number_t::version_number_t(const std::string &s)
  : valid(false)
{
  memset(parts, 0, 5 * sizeof(unsigned int));

  static boost::regex s_version_number_re("^(\\d+)\\.(\\d+)\\.(\\d+)\(?:\\.(\\d+))?", boost::regex::perl);

  boost::match_results<std::string::const_iterator> matches;
  if (!boost::regex_search(s, matches, s_version_number_re))
    return;

  size_t idx;
  for (idx = 1; 4 >= idx; ++idx) {
    if (!matches[idx].str().empty())
      parse_uint(matches[idx].str(), parts[idx - 1]);
    else
      parts[idx - 1] = 0;
  }

  valid = true;
}

version_number_t::version_number_t(const version_number_t &v) {
  memcpy(parts, v.parts, 5 * sizeof(unsigned int));
  valid = v.valid;
}

int
version_number_t::compare(const version_number_t &cmp)
  const
{
  size_t idx;
  for (idx = 0; 5 > idx; ++idx)
    if (parts[idx] < cmp.parts[idx])
      return -1;
    else if (parts[idx] > cmp.parts[idx])
      return 1;
  return 0;
}

bool
version_number_t::operator <(const version_number_t &cmp)
  const
{
  return compare(cmp) == -1;
}

std::string
get_version_info(const std::string &program,
                 bool full) {
  std::string short_version_info = (boost::format("%1% v%2% ('%3%')") % program % VERSION % VERSIONNAME).str();
#if !defined(HAVE_BUILD_TIMESTAMP)
  return short_version_info;
#else  // !defined(HAVE_BUILD_TIMESTAMP)
  if (!full)
    return short_version_info;

  return (boost::format(Y("%1% built on %2% %3%")) % short_version_info % __DATE__ % __TIME__).str();
#endif  // !defined(HAVE_BUILD_TIMESTAMP)
}

int
compare_current_version_to(const std::string &other_version_str) {
  return version_number_t(VERSION).compare(version_number_t(other_version_str));
}
