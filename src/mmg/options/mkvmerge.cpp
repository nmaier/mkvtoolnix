/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "options" dialog -- mkvmerge tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <algorithm>
#include <string>
#include <vector>

#include <wx/wxprec.h>

#include <wx/wx.h>
#include <wx/config.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/process.h>
#include <wx/statline.h>

#include "common/common_pch.h"
#include "common/strings/editing.h"
#include "common/translation.h"
#include "common/wx.h"
#include "mmg/mmg_dialog.h"
#include "mmg/mmg.h"
#include "mmg/options/mkvmerge.h"

translation_table_c optdlg_mkvmerge_tab::cob_priority_translations;

optdlg_mkvmerge_tab::optdlg_mkvmerge_tab(wxWindow *parent,
                                         mmg_options_t &options)
  : optdlg_base_tab(parent, options)
{
  // Setup static variables.

  if (cob_priority_translations.entries.empty()) {
#ifdef SYS_WINDOWS
    cob_priority_translations.add(wxT("highest"), Z("highest"));
    cob_priority_translations.add(wxT("higher"),  Z("higher"));
#endif
    cob_priority_translations.add(wxT("normal"),  Z("normal"));
    cob_priority_translations.add(wxT("lower"),   Z("lower"));
    cob_priority_translations.add(wxT("lowest"),  Z("lowest"));
  }

  // Create the controls.

  wxStaticText *st_mkvmerge = new wxStaticText(this, -1, Z("mkvmerge executable"));
  tc_mkvmerge               = new wxTextCtrl(this, ID_TC_MKVMERGE, m_options.mkvmerge);
  wxButton *b_browse        = new wxButton(this, ID_B_BROWSEMKVMERGE, Z("Browse"));

  wxStaticText *st_priority = new wxStaticText(this, -1, Z("Process priority:"));
  cob_priority              = new wxMTX_COMBOBOX_TYPE(this, ID_COB_PRIORITY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);

  cob_priority->SetToolTip(TIP("Sets the priority that mkvmerge will run with."));

  size_t i;
  for (i = 0; cob_priority_translations.entries.size() > i; ++i)
    cob_priority->Append(cob_priority_translations.entries[i].translated);

  // Set the defaults.

  select_priority(m_options.priority);

  // Create the layout.

  wxBoxSizer *siz_all = new wxBoxSizer(wxVERTICAL);

  siz_all->AddSpacer(5);

  siz_all->Add(new wxStaticText(this, wxID_ANY, Z("mkvmerge options")), 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_all->AddSpacer(5);
  siz_all->Add(new wxStaticLine(this),                                  0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_all->AddSpacer(5);

  wxFlexGridSizer *siz_fg = new wxFlexGridSizer(3);
  siz_fg->AddGrowableCol(1);

  siz_fg->Add(st_mkvmerge, 0, wxALIGN_CENTER_VERTICAL, 0);
  siz_fg->Add(tc_mkvmerge, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxLEFT, 5);
  siz_fg->Add(b_browse, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);

  siz_fg->Add(st_priority, 0, wxALIGN_CENTER_VERTICAL, 0);
  siz_fg->Add(cob_priority, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);

  siz_all->Add(siz_fg, 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_all->AddSpacer(5);

  SetSizer(siz_all);
}

void
optdlg_mkvmerge_tab::on_browse_mkvmerge(wxCommandEvent &) {
  wxFileDialog dlg(this, Z("Choose the mkvmerge executable"), tc_mkvmerge->GetValue().BeforeLast(PSEP), wxEmptyString,
#ifdef SYS_WINDOWS
                   wxString::Format(Z("Executable files (*.exe)|*.exe|%s"), ALLFILES.c_str()),
#else
                   wxT("All files (*)|*"),
#endif
                   wxFD_OPEN);

  if (dlg.ShowModal() != wxID_OK)
    return;

  wxString file_name(dlg.GetPath().AfterLast('/').AfterLast('\\').Lower());

  if ((file_name == wxT("mmg.exe")) || (file_name == wxT("mmg"))) {
    wxMessageBox(Z("Please do not select 'mmg' itself as the 'mkvmerge' executable."), Z("Wrong file chosen"), wxOK | wxCENTER | wxICON_ERROR);
    return;
  }

  tc_mkvmerge->SetValue(dlg.GetPath());
}

void
optdlg_mkvmerge_tab::select_priority(const wxString &priority) {
  cob_priority->SetValue(cob_priority_translations.to_translated(priority));
}

wxString
optdlg_mkvmerge_tab::get_selected_priority() {
  return cob_priority_translations.to_english(cob_priority->GetValue());
}

void
optdlg_mkvmerge_tab::save_options() {
  m_options.mkvmerge = tc_mkvmerge->GetValue();
  m_options.priority = get_selected_priority();
}

wxString
optdlg_mkvmerge_tab::get_title() {
  return Z("mkvmerge");
}

IMPLEMENT_CLASS(optdlg_mkvmerge_tab, optdlg_base_tab);
BEGIN_EVENT_TABLE(optdlg_mkvmerge_tab, optdlg_base_tab)
  EVT_BUTTON(ID_B_BROWSEMKVMERGE, optdlg_mkvmerge_tab::on_browse_mkvmerge)
END_EVENT_TABLE();
