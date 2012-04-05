/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   declaration for the "additional parts" dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_MMG_TABS_ADDITIONAL_PARTS_DLG_H
#define MTX_MMG_TABS_ADDITIONAL_PARTS_DLG_H

#include "common/common_pch.h"

#include <wx/wx.h>

#include <wx/dialog.h>
#include <wx/filename.h>
#include <wx/listctrl.h>

#define ID_ADDPARTS_B_ADD    19000
#define ID_ADDPARTS_B_REMOVE 19001
#define ID_ADDPARTS_B_UP     19002
#define ID_ADDPARTS_B_DOWN   19003
#define ID_ADDPARTS_B_SORT   19004
#define ID_ADDPARTS_B_CLOSE  19005
#define ID_ADDPARTS_LV_FILES 19006

class additional_parts_dialog: public wxDialog {
  DECLARE_CLASS(additional_parts_dialog);
  DECLARE_EVENT_TABLE();
protected:
  wxListView *m_lv_files;
  wxButton *m_b_add, *m_b_remove, *m_b_up, *m_b_down, *m_b_sort, *m_b_close;
  wxFileName m_primary_file_name;
  std::vector<wxFileName> m_files;

public:
  additional_parts_dialog(wxWindow *parent, wxFileName const &primary_file_name, std::vector<wxFileName> const &files);

  void on_add(wxCommandEvent &evt);
  void on_remove(wxCommandEvent &evt);
  void on_up(wxCommandEvent &evt);
  void on_down(wxCommandEvent &evt);
  void on_sort(wxCommandEvent &evt);
  void on_close(wxCommandEvent &evt);
  void on_item_selected(wxListEvent &evt);

  std::vector<wxFileName> get_file_names();

  void enable_buttons();
};

#endif // MTX_MMG_TABS_ADDITIONAL_PARTS_DLG_H
