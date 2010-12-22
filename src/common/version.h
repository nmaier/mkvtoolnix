/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   version information

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_VERSION_H
#define __MTX_COMMON_VERSION_H

#include <string>

struct version_number_t {
  unsigned int parts[5];
  bool valid;

  version_number_t();
  version_number_t(const std::string &s);
  version_number_t(const version_number_t &v);

  bool operator <(const version_number_t &cmp) const;
  int compare(const version_number_t &cmp) const;
};

std::string MTX_DLL_API get_version_info(const std::string &program, bool full = false);
int MTX_DLL_API compare_current_version_to(const std::string &other_version_str);
version_number_t MTX_DLL_API get_current_version();

#endif  // __MTX_COMMON_VERSION_H
