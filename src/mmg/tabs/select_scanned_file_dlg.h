/*
  mkvmerge GUI -- utility for splicing together matroska files
  from component media subtypes

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html

  select_scanned_file_dlg

  Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_MMG_TABS_SELECT_SCANED_FILE_DLG_H
#define MTX_MMG_TABS_SELECT_SCANED_FILE_DLG_H

#include "common/common_pch.h"

#include <wx/imaglist.h>
#include <wx/string.h>
#include <wx/stattext.h>
#include <wx/listctrl.h>
#include <wx/button.h>
#include <wx/dialog.h>

#include "mmg/window_geometry_saver.h"
#include "mmg/tabs/playlist_file.h"

#define ID_LC_PLAYLIST_FILE 16050

class select_scanned_file_dlg : public wxDialog {
  DECLARE_CLASS(select_scanned_file_dlg);
  DECLARE_EVENT_TABLE();
protected:
  wxStaticText* m_st_duration,  *m_st_size, *m_st_chapters;
  wxListCtrl *m_lc_files, *m_lc_tracks, *m_lc_items;
  wxImageList m_sort_arrows;

  std::vector<playlist_file_cptr> m_playlists;
  int m_selected_playlist_idx;

  window_geometry_saver_c m_geometry_saver;

  size_t m_sorted_by_column;
  bool m_sorted_ascending;

public:
  select_scanned_file_dlg(wxWindow *parent, std::vector<playlist_file_cptr> const &playlists, wxString const &orig_file_name);
  ~select_scanned_file_dlg();

  void on_ok(wxCommandEvent &evt);
  void on_cancel(wxCommandEvent &evt);
  void on_file_selected(wxListEvent &evt);
  void on_file_activated(wxListEvent &evt);
  void on_column_clicked(wxListEvent &evt);

  void update_info();

  void sort_by(size_t column, bool ascending);
  void set_column_image(size_t column, int image);

  int get_selected_playlist_idx() const;
};

#endif // MTX_MMG_TABS_SELECT_SCANED_FILE_DLG_H
