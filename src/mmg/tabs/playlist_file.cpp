/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   playlist_file_t, playlist_file_track_t

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "mmg/tabs/playlist_file.h"

playlist_file_t::playlist_file_t(wxString const &p_file_name,
                                 wxArrayString const &p_output)
  : file_name{p_file_name}
  , size{0}
  , duration{0}
  , chapters{0}
  , output{p_output}
{
}
