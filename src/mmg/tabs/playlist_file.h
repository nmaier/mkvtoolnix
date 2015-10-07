/*
  mkvmerge GUI -- utility for splicing together matroska files
  from component media subtypes

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html

  playlist_file_t, playlist_file_track_t

  Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_MMG_TABS_PLAYLIST_FILE_H
#define MTX_MMG_TABS_PLAYLIST_FILE_H

#include "common/common_pch.h"

#include <wx/arrstr.h>

struct playlist_file_track_t {
  wxString type, codec, language;
};

struct playlist_file_t {
  wxString file_name;
  uint64_t size, duration, chapters;
  std::vector<playlist_file_track_t> tracks;
  std::vector<wxString> files;
  wxArrayString output;

  playlist_file_t(wxString const &p_file_name, wxArrayString const &p_output);
};
typedef std::shared_ptr<playlist_file_t> playlist_file_cptr;

#endif  // MTX_MMG_TABS_PLAYLIST_FILE_H
