/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   version information

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_VERSION_H
#define MTX_COMMON_VERSION_H

#include "common/common_pch.h"

#include "common/xml/xml.h"

#define MTX_VERSION_CHECK_URL "http://mkvtoolnix-releases.bunkus.org/latest-release.xml"
#define MTX_RELEASES_INFO_URL "http://mkvtoolnix-releases.bunkus.org/releases.xml"
#define MTX_DOWNLOAD_URL      "http://www.bunkus.org/videotools/mkvtoolnix/downloads.html"
#define MTX_CHANGELOG_URL     "http://mkvtoolnix-releases.bunkus.org/doc/ChangeLog"

struct version_number_t {
  unsigned int parts[5];
  bool valid;

  version_number_t();
  version_number_t(const std::string &s);
  version_number_t(const version_number_t &v);

  bool operator <(const version_number_t &cmp) const;
  int compare(const version_number_t &cmp) const;

  std::string to_string() const;
};

struct mtx_release_version_t {
  version_number_t current_version, latest_source, latest_windows_build;
  std::map<std::string, std::string> urls;
  bool valid;

  mtx_release_version_t();
};

enum version_info_flags_e {
  vif_timestamp    = 0x0001,
  vif_untranslated = 0x0002,
  vif_architecture = 0x0004,

  vif_none         = 0x0000,
  vif_default      = vif_architecture,
  vif_full         = (0xffff & ~vif_untranslated),
};

std::string get_version_info(const std::string &program, version_info_flags_e flags = vif_default);
int compare_current_version_to(const std::string &other_version_str);
version_number_t get_current_version();

# if defined(HAVE_CURL_EASY_H)
mtx_release_version_t get_latest_release_version();
mtx::xml::document_cptr get_releases_info();
# endif  // defined(HAVE_CURL_EASY_H)

#endif  // MTX_COMMON_VERSION_H
