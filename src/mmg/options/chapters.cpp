/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "options" dialog -- chapters tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/wx.h>
#include <wx/config.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/process.h>
#include <wx/statline.h>

#include "common/chapters/chapters.h"
#include "common/common_pch.h"
#include "common/extern_data.h"
#include "common/iso639.h"
#include "common/wx.h"
#include "mmg/mmg_dialog.h"
#include "mmg/mmg.h"
#include "mmg/options/chapters.h"

optdlg_chapters_tab::optdlg_chapters_tab(wxWindow *parent,
                                         mmg_options_t &options)
  : optdlg_base_tab(parent, options)
{
  // Create the controls.

  cob_language = new wxMTX_COMBOBOX_TYPE(this, wxID_STATIC, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_DROPDOWN | wxCB_READONLY);
  cob_country  = new wxMTX_COMBOBOX_TYPE(this, wxID_STATIC, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_DROPDOWN | wxCB_READONLY);

  // Set the defaults.

  wxString default_language = wxU(g_default_chapter_language);
  bool found                = false;
  unsigned int idx;
  for (idx = 0; sorted_iso_codes.Count() > idx; ++idx) {
    cob_language->Append(sorted_iso_codes[idx]);
    if (!found && (extract_language_code(sorted_iso_codes[idx]) == default_language)) {
      set_combobox_selection(cob_language,sorted_iso_codes[idx]);
      found = true;
    }
  }

  cob_country->Append(wxEmptyString);
  for (auto &cctld : cctlds)
    cob_country->Append(wxU(cctld));
  set_combobox_selection(cob_country, wxU(g_default_chapter_country));

  // Create the layout.

  wxBoxSizer *siz_all = new wxBoxSizer(wxVERTICAL);

  siz_all->AddSpacer(5);

  siz_all->Add(new wxStaticText(this, wxID_ANY, Z("Chapter options")), 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_all->AddSpacer(5);
  siz_all->Add(new wxStaticLine(this),                                 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_all->AddSpacer(5);

  siz_all->Add(new wxStaticText(this, wxID_STATIC,
                                Z("Here you can set the default values that mmg will use\n"
                                  "for each chapter that you create. These values can\n"
                                  "then be changed if needed. The default values will be\n"
                                  "saved when you exit mmg.")),
                 0, wxLEFT | wxRIGHT, 5);
  siz_all->AddSpacer(5);

  wxFlexGridSizer *siz_input = new wxFlexGridSizer(2);
  siz_input->AddGrowableCol(1);

  siz_input->Add(new wxStaticText(this, wxID_STATIC, Z("Language:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  siz_input->Add(cob_language, 0, wxGROW, 0);

  siz_input->Add(new wxStaticText(this, wxID_STATIC, Z("Country:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
  siz_input->Add(cob_country, 0, wxGROW, 0);

  siz_all->Add(siz_input, 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_all->AddSpacer(5);

  SetSizerAndFit(siz_all);
}

void
optdlg_chapters_tab::save_options() {
  std::string value = wxMB(extract_language_code(cob_language->GetValue()));
  if (is_valid_iso639_2_code(value.c_str()))
    g_default_chapter_language = value;

  value = wxMB(cob_country->GetValue());
  if (is_valid_cctld(value))
    g_default_chapter_country = value;
}

bool
optdlg_chapters_tab::validate_choices() {
  wxString value = extract_language_code(cob_language->GetValue());
  if (!is_valid_iso639_2_code(wxMB(value))) {
    wxMessageBox(wxString::Format(Z("The language '%s' is not a valid language and cannot be selected."), value.c_str()),
                 Z("Invalid language selected"), wxICON_ERROR | wxOK, this);
    return false;
  }

  value = cob_country->GetValue();
  if (!value.IsEmpty() && !is_valid_cctld(wxMB(value))) {
    wxMessageBox(wxString::Format(Z("The country '%s' is not a valid ccTLD and cannot be selected."), value.c_str()),
                 Z("Invalid country selected"), wxICON_ERROR | wxOK, this);
    return false;
  }

  return true;
}

wxString
optdlg_chapters_tab::get_title() {
  return Z("Chapters");
}

IMPLEMENT_CLASS(optdlg_chapters_tab, optdlg_base_tab);
