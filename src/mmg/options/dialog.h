/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   declarations for the options dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_MMG_OPTIONS_DIALOG_H
#define MTX_MMG_OPTIONS_DIALOG_H

#include "common/common_pch.h"

#include <vector>
#include <wx/log.h>

#include "mmg/options/tab_base.h"

class options_dialog: public wxDialog {
  DECLARE_CLASS(options_dialog);
  DECLARE_EVENT_TABLE();
public:
  mmg_options_t &m_options;

  wxNotebook *nb_tabs;
  std::vector<optdlg_base_tab *> tabs;

public:
  options_dialog(wxWindow *parent, mmg_options_t &options);

  void on_ok(wxCommandEvent &evt);
};

#endif // MTX_MMG_OPTIONS_DIALOG_H
