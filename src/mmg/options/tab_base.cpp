/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "options" dialog -- mkvmerge tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/wx.h>
#include <wx/config.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/process.h>
#include <wx/statline.h>

#include "common/wx.h"
#include "mmg/mmg_dialog.h"
#include "mmg/mmg.h"
#include "mmg/options/tab_base.h"

optdlg_base_tab::optdlg_base_tab(wxWindow *parent,
                                 mmg_options_t &options)
  : wxPanel(parent)
  , m_options(options)
{
}

void
optdlg_base_tab::save_options() {
}

bool
optdlg_base_tab::validate_choices() {
  return true;
}

IMPLEMENT_CLASS(optdlg_base_tab, wxPanel);
