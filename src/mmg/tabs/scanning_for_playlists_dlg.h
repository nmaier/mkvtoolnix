/*
  mkvmerge GUI -- utility for splicing together matroska files
  from component media subtypes

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html

  scanning_for_playlists_dlg

  Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_MMG_TABS_SCANNING_FOR_PLAYLISTS_DLG_H
#define MTX_MMG_TABS_SCANNING_FOR_PLAYLISTS_DLG_H

#include "common/common_pch.h"

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/gauge.h>

#if defined(SYS_WINDOWS)
# include "mmg/taskbar_progress.h"
#endif  // SYS_WINDOWS

#include "common/wx.h"
#include "mmg/tabs/playlist_file.h"

class scan_directory_thread_c;

class scanning_for_playlists_dlg : public wxDialog{
  DECLARE_CLASS(scanning_for_playlists_dlg);
  DECLARE_EVENT_TABLE();
protected:
  wxStaticText *m_st_progress, *m_st_remaining_time;
  wxGauge *m_g_progress;
  wxButton *m_b_abort;
#if defined(SYS_WINDOWS)
  taskbar_progress_c *m_taskbar_progress;
#endif  // SYS_WINDOWS

  std::vector<playlist_file_cptr> m_playlists;

  class scan_directory_thread_c *m_scanner;
  bool m_aborted;

  size_t m_progress, m_max_progress;
  uint64_t m_start_time, m_next_remaining_time_update;

public:
  scanning_for_playlists_dlg(wxWindow *parent, wxString const &original_file_name, wxArrayString const &original_output, std::vector<wxString> const &other_file_names);
  ~scanning_for_playlists_dlg();

  int scan();
  std::vector<playlist_file_cptr> const &get_playlists() const;

protected:
  void parse_output(wxString const &file_name, wxArrayString const &output);
  std::pair< std::map<wxString, wxString>, std::vector<wxString> > parse_properties(wxString const &line, wxString const &wanted_content) const;

  void on_abort(wxCommandEvent &evt);
  void on_scanner_finished(wxCommandEvent &evt);
  void on_progress_changed(wxCommandEvent &evt);

  void update_gauge(size_t progress);
};

#endif // MTX_MMG_TABS_SCANNING_FOR_PLAYLISTS_DLG_H
