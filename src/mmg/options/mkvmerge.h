/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   declarations for the options dialog -- mkvmerge tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_MMG_OPTIONS_MKVMERGE_H
#define MTX_MMG_OPTIONS_MKVMERGE_H

#include "common/common_pch.h"

#include <wx/log.h>
#include <wx/panel.h>

#include "mmg/options/tab_base.h"

#define ID_TC_MKVMERGE      15000
#define ID_B_BROWSEMKVMERGE 15001
#define ID_COB_PRIORITY     15002

class optdlg_mkvmerge_tab: public optdlg_base_tab {
  DECLARE_CLASS(optdlg_mkvmerge_tab);
  DECLARE_EVENT_TABLE();
protected:
  wxTextCtrl *tc_mkvmerge;
  wxMTX_COMBOBOX_TYPE *cob_priority;

public:                         // Static
  static translation_table_c cob_priority_translations;

public:
  optdlg_mkvmerge_tab(wxWindow *parent, mmg_options_t &options);

  void on_browse_mkvmerge(wxCommandEvent &evt);

  void select_priority(const wxString &priority);
  wxString get_selected_priority();

  virtual void save_options();
  virtual wxString get_title();
};

#endif // MTX_MMG_OPTIONS_MKVMERGE_H
