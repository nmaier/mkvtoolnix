/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper functions for WebMedia

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include "common/webmedia.h"

#include <boost/regex.hpp>
#include <string>

bool
is_webmedia_file_name(const std::string &file_name) {
  static boost::regex s_webmedia_file_name_re("\\.webm(?:edia|a|v)?$", boost::regex::perl);
  return boost::regex_search(file_name, s_webmedia_file_name_re);
}
