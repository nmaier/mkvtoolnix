/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   the mmg_options_t struct

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/wx.h>

#include "common/extern_data.h"
#include "common/fs_sys_helpers.h"
#include "common/wx.h"
#include "mmg/mmg.h"

void
mmg_options_t::init_popular_languages(const wxString &list) {
  size_t i;

  popular_languages.clear();

  if (!list.IsEmpty()) {
    std::vector<wxString> codes = split(list, wxU(" "));
    for (i = 0; codes.size() > i; ++i)
      if (is_valid_iso639_2_code(wxMB(codes[i])))
        popular_languages.Add(codes[i]);
  }

  if (popular_languages.IsEmpty()) {
    std::map<std::string, bool> codes_found;
    for (auto lang : iso639_languages)
      if (!codes_found[lang.iso639_2_code] && is_popular_language_code(lang.iso639_2_code)) {
        popular_languages.Add(wxU(lang.iso639_2_code));
        codes_found[lang.iso639_2_code] = true;
      }
  }

  popular_languages.Sort();
}

void
mmg_options_t::validate() {
  if (   (ODM_FROM_FIRST_INPUT_FILE      != output_directory_mode)
      && (ODM_PARENT_OF_FIRST_INPUT_FILE != output_directory_mode)
      && (ODM_PREVIOUS                   != output_directory_mode)
      && (ODM_FIXED                      != output_directory_mode))
    output_directory_mode = ODM_FROM_FIRST_INPUT_FILE;

  if (   (priority != wxU("highest")) && (priority != wxU("higher")) && (priority != wxU("normal"))
      && (priority != wxU("lower"))   && (priority != wxU("lowest")))
    priority = wxU("normal");

  if (   (SDP_ALWAYS_ASK  != scan_directory_for_playlists)
      && (SDP_ALWAYS_SCAN != scan_directory_for_playlists)
      && (SDP_NEVER       != scan_directory_for_playlists))
    scan_directory_for_playlists = SDP_ALWAYS_ASK;
}

wxString
mmg_options_t::mkvmerge_exe() {
#if defined(SYS_WINDOWS)
  auto exe_with_ext = "mkvmerge.exe";
#else
  auto exe_with_ext = "mkvmerge";
#endif

  // Check whether or not the mkvmerge executable path is still set to
  // the default value "mkvmerge". If it is try getting the
  // installation path from the registry and use that for a more
  // precise location for mkvmerge.exe. Fall back to the old default
  // value "mkvmerge" if all else fails.
  auto exe = bfs::path{to_utf8(mkvmerge)};
  if ((exe.string() == "mkvmerge") || (exe.string() == exe_with_ext))
    exe = bfs::path{};

  if (exe.empty()) {
    exe = mtx::get_installation_path();
    if (!exe.empty() && bfs::exists(exe) && bfs::exists(exe / exe_with_ext))
      exe = exe / exe_with_ext;
    else
      exe = bfs::path{};
  }

  if (exe.empty())
    exe = exe_with_ext;

  wxLogMessage(wxT("mkvmerge_exe %s installation_path %s\n"), wxU(exe.string()).c_str(), wxU(mtx::get_installation_path().string()).c_str());

  return wxU(exe.string());
}
