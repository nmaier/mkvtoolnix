/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "input" tab -- "General track options" sub-tab

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


tab_input_general::tab_input_general(wxWindow *parent,
                                     tab_input *ti)
  : wxPanel(parent, -1, wxDefaultPosition, wxSize(100, 400), wxTAB_TRAVERSAL)
  , input(ti) {

  wxFlexGridSizer *siz_fg;
  wxBoxSizer *siz_line;

  siz_fg = new wxFlexGridSizer(2);
  siz_fg->AddGrowableCol(1);

  st_track_name = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  st_track_name->Enable(false);
  siz_fg->Add(st_track_name, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);
  tc_track_name = new wxTextCtrl(this, ID_TC_TRACKNAME, wxEmptyString);
  tc_track_name->SetSizeHints(0, -1);
  siz_fg->Add(tc_track_name, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  st_language = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  st_language->Enable(false);
  siz_fg->Add(st_language, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  cob_language = new wxMTX_COMBOBOX_TYPE(this, ID_CB_LANGUAGE, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_DROPDOWN | wxCB_READONLY);
  siz_fg->Add(cob_language, 1, wxGROW | wxALL, STDSPACING);
  cob_language->SetSizeHints(0, -1);

  st_default = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  siz_fg->Add(st_default, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  cob_default = new wxMTX_COMBOBOX_TYPE(this, ID_CB_MAKEDEFAULT, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_DROPDOWN | wxCB_READONLY);
  siz_fg->Add(cob_default, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  st_forced = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  siz_fg->Add(st_forced, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  cob_forced = new wxMTX_COMBOBOX_TYPE(this, ID_CB_FORCED_TRACK, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_DROPDOWN | wxCB_READONLY);
  siz_fg->Add(cob_forced, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  st_tags = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  st_tags->Enable(false);
  siz_fg->Add(st_tags, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  siz_line = new wxBoxSizer(wxHORIZONTAL);

  tc_tags = new wxTextCtrl(this, ID_TC_TAGS, wxEmptyString);
  siz_line->Add(tc_tags, 1, wxGROW | wxALL, STDSPACING);
  b_browse_tags = new wxButton(this, ID_B_BROWSETAGS, wxEmptyString);
  siz_line->Add(b_browse_tags, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  siz_fg->Add(siz_line, 1, wxGROW | wxALIGN_CENTER_VERTICAL, 0);

  st_timecodes = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  st_timecodes->Enable(false);
  siz_fg->Add(st_timecodes, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  siz_line = new wxBoxSizer(wxHORIZONTAL);

  tc_timecodes = new wxTextCtrl(this, ID_TC_TIMECODES, wxEmptyString);
  siz_line->Add(tc_timecodes, 1, wxGROW | wxALL, STDSPACING);
  b_browse_timecodes = new wxButton(this, ID_B_BROWSE_TIMECODES, wxEmptyString);
  siz_line->Add(b_browse_timecodes, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  siz_fg->Add(siz_line, 1, wxGROW | wxALIGN_CENTER_VERTICAL, 0);

  SetSizerAndFit(siz_fg);
}

void
tab_input_general::setup_default_track() {
  cob_default_translations.clear();
  cob_default_translations.add(wxT("default"), Z("default"));
  cob_default_translations.add(wxT("yes"),     Z("yes"));
  cob_default_translations.add(wxT("no"),      Z("no"));

  if (!cob_default->GetCount()) {
    auto entries = wxArrayString{};
    entries.Alloc(cob_default_translations.entries.size());
    for (auto idx = cob_default_translations.entries.size(); 0 < idx; --idx)
      entries.Add(wxEmptyString);
    append_combobox_items(cob_default, entries);
  }

  auto selection = cob_default->GetSelection();
  auto idx       = 0;
  for (auto const &entry : cob_default_translations.entries)
    cob_default->SetString(idx++, entry.translated);
  cob_default->SetSelection(selection);
}

void
tab_input_general::setup_forced_track() {
  cob_forced_translations.clear();
  cob_forced_translations.add(wxT("no"),  Z("no"));
  cob_forced_translations.add(wxT("yes"), Z("yes"));

  if (!cob_forced->GetCount()) {
    auto entries = wxArrayString{};
    entries.Alloc(cob_default_translations.entries.size());
    for (auto idx = cob_forced_translations.entries.size(); 0 < idx; --idx)
      entries.Add(wxEmptyString);
    append_combobox_items(cob_forced, entries);
  }

  auto selection = cob_forced->GetSelection();
  auto idx       = 0;
  for (auto const &entry : cob_forced_translations.entries)
    cob_forced->SetString(idx++, entry.translated);
  cob_forced->SetSelection(selection);
}

void
tab_input_general::setup_languages() {
  size_t i;

  sorted_iso_codes.Clear();
  sorted_iso_codes.Add(Z("und (Undetermined)"));
  sorted_iso_codes.Add(Z("---common---"));

  std::map<wxString, bool> is_popular;
  for (i = 0; i < mdlg->options.popular_languages.Count(); ++i)
    is_popular[ mdlg->options.popular_languages[i] ] = true;

  for (auto &lang : iso639_languages) {
    wxString code = wxU(lang.iso639_2_code);
    if (!is_popular[code])
      continue;

    sorted_iso_codes.Add(wxString::Format(wxT("%s (%s)"), code.c_str(), wxUCS(lang.english_name)));
    is_popular[code] = false;
  }

  sorted_iso_codes.Add(Z("---all---"));

  wxArrayString temp;
  for (auto &lang : iso639_languages)
    temp.Add(wxString::Format(wxT("%s (%s)"), wxUCS(lang.iso639_2_code), wxUCS(lang.english_name)));
  temp.Sort();

  for (i = 0; temp.Count() > i; ++i)
    if ((0 == i) || (temp[i - 1].Lower() != temp[i].Lower()))
      sorted_iso_codes.Add(temp[i]);

  size_t selection = cob_language->GetSelection();
  cob_language->Clear();
  append_combobox_items(cob_language, sorted_iso_codes);
  cob_language->SetSelection(selection);
}

void
tab_input_general::translate_ui() {
  st_track_name->SetLabel(Z("Track name:"));
  tc_track_name->SetToolTip(TIP("Name for this track, e.g. \"director's comments\"."));
  st_language->SetLabel(Z("Language:"));
  cob_language->SetToolTip(TIP("Language for this track. Select one of the ISO639-2 language codes."));
  st_default->SetLabel(Z("Default track flag:"));
  cob_default->SetToolTip(TIP("Make this track the default track for its type (audio, video, subtitles). Players should prefer tracks with the default track flag set."));
  st_forced->SetLabel(Z("Forced track flag:"));
  cob_forced->SetToolTip(TIP("Mark this track as 'forced'. Players must play this track."));
  st_tags->SetLabel(Z("Tags:"));
  b_browse_tags->SetLabel(Z("Browse"));
  st_timecodes->SetLabel(Z("Timecodes:"));
  tc_timecodes->SetToolTip(TIP("mkvmerge can read and use timecodes from an external text file. This feature is a very advanced feature. Almost all users should leave this entry empty."));
  b_browse_timecodes->SetLabel(Z("Browse"));
  b_browse_timecodes->SetToolTip(TIP("mkvmerge can read and use timecodes from an external text file. This feature is a very advanced feature. "
                                     "Almost all users should leave this entry empty."));

  setup_languages();
  setup_default_track();
  setup_forced_track();
}

void
tab_input_general::set_track_mode(mmg_track_t *t) {
  bool normal_track    = t && (('a' == t->type) || ('s' == t->type) || ('v' == t->type));
  bool enable          = t && !t->appending && normal_track;
  bool enable_chapters = t && ('c' == t->type);

  st_language->Enable(enable || enable_chapters);
  cob_language->Enable(enable || enable_chapters);
  st_track_name->Enable(enable);
  tc_track_name->Enable(enable);
  st_tags->Enable(enable);
  tc_tags->Enable(enable);
  b_browse_tags->Enable(enable);
  st_timecodes->Enable(enable);
  tc_timecodes->Enable(enable);
  b_browse_timecodes->Enable(enable);
  st_default->Enable(enable);
  cob_default->Enable(enable);
  st_forced->Enable(enable);
  cob_forced->Enable(enable);

  if (t)
    return;

  bool saved_dcvn             = input->dont_copy_values_now;
  input->dont_copy_values_now = true;

  set_combobox_selection(cob_language, sorted_iso_codes[0]);
  tc_track_name->SetValue(wxEmptyString);
  cob_default->SetSelection(0);
  cob_forced->SetSelection(0);
  tc_tags->SetValue(wxEmptyString);
  tc_timecodes->SetValue(wxEmptyString);

  input->dont_copy_values_now = saved_dcvn;
}

void
tab_input_general::on_default_track_selected(wxCommandEvent &) {
  mmg_track_t *t;

  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  t = tracks[input->selected_track];
  if (cob_default->GetSelection() == 1) {
    int idx = default_track_checked(t->type);
    if (-1 != idx)
      tracks[idx]->default_track = 0;
  }

  t->default_track = cob_default->GetSelection();
}

void
tab_input_general::on_forced_track_selected(wxCommandEvent &) {
  if (input->dont_copy_values_now || (-1 == input->selected_track))
    return;

  tracks[input->selected_track]->forced_track = cob_forced->GetSelection() == 1;
}

void
tab_input_general::on_language_selected(wxCommandEvent &) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->language = cob_language->GetStringSelection();
}

void
tab_input_general::on_browse_tags(wxCommandEvent &) {
  if (input->selected_track == -1)
    return;

  wxFileDialog dlg(nullptr, Z("Choose a tag file"), last_open_dir, wxEmptyString, wxString::Format(Z("Tag files (*.xml;*.txt)|*.xml;*.txt|%s"), ALLFILES.c_str()), wxFD_OPEN);
  if(dlg.ShowModal() != wxID_OK)
    return;

  last_open_dir = dlg.GetDirectory();
  tracks[input->selected_track]->tags = dlg.GetPath();
  tc_tags->SetValue(dlg.GetPath());
}

void
tab_input_general::on_browse_timecodes_clicked(wxCommandEvent &) {
  if (input->selected_track == -1)
    return;

  wxFileDialog dlg(nullptr, Z("Choose a timecodes file"), last_open_dir, wxEmptyString, wxString::Format(Z("Timecode files (*.tmc;*.txt)|*.tmc;*.txt|%s"), ALLFILES.c_str()), wxFD_OPEN);
  if(dlg.ShowModal() != wxID_OK)
    return;

  last_open_dir = dlg.GetDirectory();
  tracks[input->selected_track]->timecodes = dlg.GetPath();
  tc_timecodes->SetValue(dlg.GetPath());
}

void
tab_input_general::on_tags_changed(wxCommandEvent &) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->tags = tc_tags->GetValue();
}

void
tab_input_general::on_timecodes_changed(wxCommandEvent &) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->timecodes = tc_timecodes->GetValue();
}

void
tab_input_general::on_track_name_changed(wxCommandEvent &) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->track_name = tc_track_name->GetValue();
}

IMPLEMENT_CLASS(tab_input_general, wxPanel);
BEGIN_EVENT_TABLE(tab_input_general, wxPanel)
  EVT_BUTTON(ID_B_BROWSETAGS,       tab_input_general::on_browse_tags)
  EVT_BUTTON(ID_B_BROWSE_TIMECODES, tab_input_general::on_browse_timecodes_clicked)
  EVT_TEXT(ID_TC_TAGS,              tab_input_general::on_tags_changed)
  EVT_TEXT(ID_TC_TIMECODES,         tab_input_general::on_timecodes_changed)
  EVT_COMBOBOX(ID_CB_MAKEDEFAULT,   tab_input_general::on_default_track_selected)
  EVT_COMBOBOX(ID_CB_FORCED_TRACK,  tab_input_general::on_forced_track_selected)
  EVT_COMBOBOX(ID_CB_LANGUAGE,      tab_input_general::on_language_selected)
  EVT_TEXT(ID_TC_TRACKNAME,         tab_input_general::on_track_name_changed)
END_EVENT_TABLE();
