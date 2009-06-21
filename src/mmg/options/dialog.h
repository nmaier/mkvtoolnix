/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   declarations for the options dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MMG_OPTIONS_DIALOG_H
#define __MMG_OPTIONS_DIALOG_H

#include "common/os.h"

#include <wx/log.h>

#include <vector>

#include "mmg/options/mkvmerge.h"
#include "mmg/options/mmg.h"

class options_dialog: public wxDialog {
  DECLARE_CLASS(options_dialog);
  DECLARE_EVENT_TABLE();
public:
  mmg_options_t &m_options;

  wxNotebook *nb_tabs;
  optdlg_mkvmerge_tab *tab_mkvmerge;
  optdlg_mmg_tab *tab_mmg;

public:
  options_dialog(wxWindow *parent, mmg_options_t &options);

  void on_ok(wxCommandEvent &evt);
};

#endif // __MMG_OPTIONS_DIALOG_H
