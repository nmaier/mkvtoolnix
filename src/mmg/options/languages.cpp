/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "options" dialog -- languages tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/wx.h>
#include <wx/config.h>
#include <wx/listbox.h>
#include <wx/statline.h>

#include "common/extern_data.h"
#include "common/strings/editing.h"
#include "common/translation.h"
#include "common/wx.h"
#include "mmg/mmg_dialog.h"
#include "mmg/mmg.h"
#include "mmg/options/languages.h"

wxArrayString optdlg_languages_tab::s_languages;

optdlg_languages_tab::optdlg_languages_tab(wxWindow *parent,
                                           mmg_options_t &options)
  : optdlg_base_tab(parent, options)
{
  // Setup static variables.

  if (s_languages.IsEmpty()) {
    size_t start = 0;
    size_t sic_idx;
    for (sic_idx = 0; sorted_iso_codes.Count() > sic_idx; ++sic_idx)
      if (sorted_iso_codes[sic_idx] == Z("---all---")) {
        start = sic_idx + 1;
        break;
      }

    for (sic_idx = start; sorted_iso_codes.Count() > sic_idx; ++sic_idx)
      s_languages.Add(sorted_iso_codes[sic_idx]);
  }

  // Create the controls.

  lb_popular_languages = new wxListBox(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, s_languages, wxLB_EXTENDED);

  // Set the defaults.

  size_t pl_idx;
  for (pl_idx = 0; m_options.popular_languages.Count() > pl_idx; ++pl_idx) {
    size_t lang_idx;
    for (lang_idx = 0; s_languages.Count() > lang_idx; ++lang_idx)
      if (extract_language_code(s_languages[lang_idx]) == m_options.popular_languages[pl_idx]) {
        lb_popular_languages->SetSelection(lang_idx);
        break;
      }
  }

  lb_popular_languages->SetFirstItem(0);

  // Create the layout.

  wxBoxSizer *siz_all = new wxBoxSizer(wxVERTICAL);

  siz_all->AddSpacer(5);

  siz_all->Add(new wxStaticText(this, wxID_ANY, Z("Common languages")), 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_all->AddSpacer(5);
  siz_all->Add(new wxStaticLine(this),                                  0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_all->AddSpacer(5);

  siz_all->Add(new wxStaticText(this, wxID_ANY, Z("Select the languages you want to be shown at the top\nof language drop down boxes.")), 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_all->AddSpacer(5);
  siz_all->Add(new wxStaticText(this, wxID_ANY, Z("mmg will reset to the default list if no entry is selected.")),                        0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_all->AddSpacer(5);
  siz_all->Add(new wxStaticText(this, wxID_ANY, Z("Changes to this list do not take effect until mmg is restarted.")),                    0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_all->AddSpacer(5);

  siz_all->Add(lb_popular_languages, 1, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_all->AddSpacer(5);

  SetSizer(siz_all);
}

void
optdlg_languages_tab::save_options() {
  wxArrayInt selections;
  lb_popular_languages->GetSelections(selections);

  if (selections.IsEmpty()) {
    m_options.init_popular_languages();
    return;
  }

  m_options.popular_languages.Clear();

  size_t idx;
  for (idx = 0; selections.Count() > idx; ++idx)
    m_options.popular_languages.Add(extract_language_code(lb_popular_languages->GetString(selections[idx])));
}

wxString
optdlg_languages_tab::get_title() {
  return Z("Languages");
}

IMPLEMENT_CLASS(optdlg_languages_tab, optdlg_base_tab);
