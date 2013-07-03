/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "extra" tab -- "User defined track options" sub-tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/wx.h>
#include <wx/dnd.h>
#include <wx/file.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/regex.h>
#include <wx/statline.h>

#include "common/extern_data.h"
#include "common/iso639.h"
#include "merge/mkvmerge.h"
#include "mmg/mmg.h"
#include "mmg/mmg_dialog.h"
#include "mmg/tabs/input.h"
#include "mmg/tabs/global.h"


tab_input_extra::tab_input_extra(wxWindow *parent,
                                 tab_input *ti)
  : wxPanel(parent, -1, wxDefaultPosition, wxSize(100, 400), wxTAB_TRAVERSAL)
  , input(ti) {

  wxFlexGridSizer *siz_fg;
  wxBoxSizer *siz_all;

  siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->AddSpacer(TOPBOTTOMSPACING);

  siz_fg = new wxFlexGridSizer(2);
  siz_fg->AddGrowableCol(1);

  st_cues = new wxStaticText(this, wxID_STATIC, Z("Cues:"));
  st_cues->Enable(false);
  siz_fg->Add(st_cues, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  cob_cues = new wxMTX_COMBOBOX_TYPE(this, ID_CB_CUES, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_DROPDOWN | wxCB_READONLY);
  cob_cues->SetSizeHints(0, -1);
  siz_fg->Add(cob_cues, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  st_compression = new wxStaticText(this, -1, Z("Compression:"));
  st_compression->Enable(false);
  siz_fg->Add(st_compression, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  cob_compression = new wxMTX_COMBOBOX_TYPE(this, ID_CB_COMPRESSION, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_DROPDOWN | wxCB_READONLY);
  cob_compression->SetSizeHints(0, -1);
  siz_fg->Add(cob_compression, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  st_user_defined = new wxStaticText(this, -1, Z("User defined options:"));
  st_user_defined->Enable(false);
  siz_fg->Add(st_user_defined, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  tc_user_defined = new wxTextCtrl(this, ID_TC_USER_DEFINED, wxEmptyString);
  tc_user_defined->SetSizeHints(0, -1);
  tc_user_defined->Enable(false);
  siz_fg->Add(tc_user_defined, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  siz_all->Add(siz_fg, 0, wxGROW | wxLEFT | wxRIGHT, LEFTRIGHTSPACING);

  siz_all->AddSpacer(TOPBOTTOMSPACING);

  SetSizer(siz_all);
}

void
tab_input_extra::setup_cues() {
  cob_cues_translations.clear();
  cob_cues_translations.add(wxT("default"),           Z("default"));
  cob_cues_translations.add(wxT("only for I frames"), Z("only for I frames"));
  cob_cues_translations.add(wxT("for all frames"),    Z("for all frames"));
  cob_cues_translations.add(wxT("none"),              Z("none"));

  if (!cob_cues->GetCount()) {
    auto entries = wxArrayString{};
    entries.Alloc(cob_cues_translations.entries.size());
    for (auto const &entry : cob_cues_translations.entries)
      entries.Add(entry.translated);
    append_combobox_items(cob_cues, entries);

  } else {
    auto selection = cob_cues->GetSelection();
    auto idx       = 0;
    for (auto const &entry : cob_cues_translations.entries)
      cob_cues->SetString(idx++, entry.translated);
    cob_cues->SetSelection(selection);
  }
}

void
tab_input_extra::setup_compression() {
  int selection = cob_compression->GetSelection();

  auto entries  = wxArrayString{};
  entries.Alloc(3);
  entries.Add(wxEmptyString);
  entries.Add(wxEmptyString);
  entries.Add(wxT("zlib"));

  cob_compression->Clear();
  append_combobox_items(cob_compression, entries);

  cob_compression_translations.clear();
  cob_compression_translations.add(wxT("none"), Z("none"));

  selection = cob_compression->GetSelection();
  cob_compression->SetString(1, Z("none"));

  cob_compression->SetSelection((0 > selection) || (cob_compression->GetCount() >= static_cast<unsigned int>(selection)) ? 0 : selection);
}

void
tab_input_extra::translate_ui() {
  st_cues->SetLabel(Z("Cues:"));
  cob_cues->SetToolTip(TIP("Selects for which blocks mkvmerge will produce index entries ( = cue entries). \"default\" is a good choice for almost all situations."));
  st_compression->SetLabel(Z("Compression:"));
  cob_compression->SetToolTip(TIP("Sets the compression algorithm to be used for this track. "
                                  "If no option is selected mkvmerge will decide whether or not to compress and which algorithm to use based on the track type. "
                                  "Most track types are not compressed at all. "));
  st_user_defined->SetLabel(Z("User defined options:"));
  tc_user_defined->SetToolTip(TIP("Free-form edit field for user defined options for this track. What you input here is added after all the other options "
                                  "mmg adds so that you could overwrite any of mmg's options for this track. "
                                  "All occurences of the string \"<TID>\" will be replaced by the track's track ID."));

  setup_cues();
  setup_compression();
}

void
tab_input_extra::set_track_mode(mmg_track_t *t) {
  bool not_appending = t && !t->appending;
  bool normal_track  = t && (('a' == t->type) || ('s' == t->type) || ('v' == t->type));
  wxString ctype     = t ? t->ctype.Lower() : wxT("");

  st_cues->Enable(not_appending && normal_track);
  cob_cues->Enable(not_appending && normal_track);
  st_compression->Enable(not_appending && normal_track);
  cob_compression->Enable(not_appending && normal_track);
  st_user_defined->Enable(normal_track);
  tc_user_defined->Enable(normal_track);

  if (t)
    return;

  bool saved_dcvn             = input->dont_copy_values_now;
  input->dont_copy_values_now = true;

  set_combobox_selection(cob_cues, Z("default"));
  tc_user_defined->SetValue(wxEmptyString);
  set_combobox_selection(cob_compression, wxEmptyString);

  input->dont_copy_values_now = saved_dcvn;
}

void
tab_input_extra::on_user_defined_changed(wxCommandEvent &) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->user_defined = tc_user_defined->GetValue();
}

void
tab_input_extra::on_cues_selected(wxCommandEvent &) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->cues = cob_cues_translations.to_english(cob_cues->GetStringSelection());
}

void
tab_input_extra::on_compression_selected(wxCommandEvent &) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->compression = cob_compression_translations.to_english(cob_compression->GetStringSelection());
}

IMPLEMENT_CLASS(tab_input_extra, wxPanel);
BEGIN_EVENT_TABLE(tab_input_extra, wxPanel)
  EVT_COMBOBOX(ID_CB_CUES,        tab_input_extra::on_cues_selected)
  EVT_COMBOBOX(ID_CB_COMPRESSION, tab_input_extra::on_compression_selected)
  EVT_TEXT(ID_TC_USER_DEFINED,    tab_input_extra::on_user_defined_changed)
END_EVENT_TABLE();
