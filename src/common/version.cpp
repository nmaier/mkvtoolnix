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

#define VERSIONNAME "Bouncin' Back"

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

static unsigned int
_parse_version_number(const std::string &version_str) {
  static boost::regex s_version_number_re("(\\d+)\\.(\\d+)\\.(\\d+)\(?:\\.(\\d+))?", boost::regex::perl);

  boost::match_results<std::string::const_iterator> matches;
  if (!boost::regex_search(version_str, matches, s_version_number_re))
    return 0;

  unsigned int version = 0, idx;
  for (idx = 1; 4 >= idx; ++idx) {
    version <<= 8;
    if (!matches[idx].str().empty()) {
      uint32_t number = 0;
      parse_uint(matches[idx].str(), number);
      version += number;
    }
  }

  return version;
}

int
compare_current_version_to(const std::string &other_version_str) {
  unsigned int my_version    = _parse_version_number(VERSION);
  unsigned int other_version = _parse_version_number(other_version_str);

  return my_version < other_version ? -1
       : my_version > other_version ? +1
       :                               0;
}
