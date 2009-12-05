/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   version information

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <boost/format.hpp>

#include "common/common.h"
#include "common/version.h"

#define VERSIONNAME "Tutu"

std::string
get_version_info(const std::string &program,
                 bool full) {
  std::string short_version_info = (boost::format("%1% v%2% ('%3%')") % program % VERSION % VERSIONNAME).str();
  if (!full)
    return short_version_info;

  return (boost::format(Y("%1% built on %2% %3%")) % short_version_info % __DATE__ % __TIME__).str();
}
