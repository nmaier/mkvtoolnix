/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "options" dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/wx.h>
#include <wx/config.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/process.h>
#include <wx/statline.h>

#include "common/strings/editing.h"
#include "common/translation.h"
#include "common/wx.h"
#include "mmg/mmg_dialog.h"
#include "mmg/mmg.h"
#include "mmg/options/chapters.h"
#include "mmg/options/dialog.h"
#include "mmg/options/languages.h"
#include "mmg/options/mkvmerge.h"
#include "mmg/options/mmg.h"
#include "mmg/options/output.h"

options_dialog::options_dialog(wxWindow *parent,
                               mmg_options_t &options)
  : wxDialog(parent, -1, Z("Options"), wxDefaultPosition, wxDefaultSize)
  , m_options(options)
{

  nb_tabs = new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxDefaultSize, wxNB_TOP);

  tabs.push_back(new optdlg_mmg_tab(      nb_tabs, options));
  tabs.push_back(new optdlg_output_tab(   nb_tabs, options));
  tabs.push_back(new optdlg_mkvmerge_tab( nb_tabs, options));
  tabs.push_back(new optdlg_languages_tab(nb_tabs, options));
  tabs.push_back(new optdlg_chapters_tab( nb_tabs, options));

  for (auto tab : tabs)
    nb_tabs->AddPage(tab, tab->get_title());

  wxBoxSizer *siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->AddSpacer(5);

  siz_all->Add(nb_tabs, 1, wxALIGN_CENTER_HORIZONTAL | wxLEFT | wxRIGHT | wxGROW, 5);
  siz_all->AddSpacer(5);

  wxBoxSizer *siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->AddStretchSpacer();
  siz_line->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, 0, 0);
  siz_line->AddSpacer(5);

  siz_all->Add(siz_line, 0, wxGROW, 0);
  siz_all->AddSpacer(5);

  SetSizer(siz_all);

  siz_all->SetMinSize(wxSize(600, -1));
  siz_all->SetSizeHints(this);
}

void
options_dialog::on_ok(wxCommandEvent &) {
  unsigned int idx;
  for (idx = 0; tabs.size() > idx; ++idx) {
    if (!tabs[idx]->validate_choices()) {
      nb_tabs->SetSelection(idx);
      return;
    }

    tabs[idx]->save_options();
  }

  EndModal(wxID_OK);
}

IMPLEMENT_CLASS(options_dialog, wxDialog);
BEGIN_EVENT_TABLE(options_dialog, wxDialog)
  EVT_BUTTON(wxID_OK, options_dialog::on_ok)
END_EVENT_TABLE();
