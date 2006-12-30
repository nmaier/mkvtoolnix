/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   "input" tab -- "General track options" sub-tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include "wx/wxprec.h"

#include "wx/wx.h"
#include "wx/dnd.h"
#include "wx/file.h"
#include "wx/listctrl.h"
#include "wx/notebook.h"
#include "wx/regex.h"
#include "wx/statline.h"

#include "common.h"
#include "extern_data.h"
#include "iso639.h"
#include "mkvmerge.h"
#include "mmg.h"
#include "mmg_dialog.h"
#include "tab_input.h"
#include "tab_global.h"

using namespace std;

tab_input_general::tab_input_general(wxWindow *parent,
                                     tab_input *ti):
  wxPanel(parent, -1, wxDefaultPosition, wxSize(100, 400),
          wxTAB_TRAVERSAL),
  input(ti) {

  wxFlexGridSizer *siz_fg;
  wxBoxSizer *siz_all, *siz_line;

  siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->AddSpacer(TOPBOTTOMSPACING);

  siz_fg = new wxFlexGridSizer(2);
  siz_fg->AddGrowableCol(1);

  st_track_name = new wxStaticText(this, wxID_STATIC, wxT("Track name:"));
  st_track_name->Enable(false);
  siz_fg->Add(st_track_name, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);
  tc_track_name = new wxTextCtrl(this, ID_TC_TRACKNAME, wxT(""));
  tc_track_name->SetToolTip(TIP("Name for this track, e.g. \"director's "
                                "comments\"."));
  tc_track_name->SetSizeHints(0, -1);
  siz_fg->Add(tc_track_name, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL,
              STDSPACING);

  st_language = new wxStaticText(this, wxID_STATIC, wxT("Language:"));
  st_language->Enable(false);
  siz_fg->Add(st_language, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  cob_language =
    new wxComboBox(this, ID_CB_LANGUAGE, wxT(""), wxDefaultPosition,
                   wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
  cob_language->SetToolTip(TIP("Language for this track. Select one of the "
                               "ISO639-2 language codes."));
  siz_fg->Add(cob_language, 1, wxGROW | wxALL, STDSPACING);

  st_cues = new wxStaticText(this, wxID_STATIC, wxT("Cues:"));
  st_cues->Enable(false);
  siz_fg->Add(st_cues, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  cob_cues =
    new wxComboBox(this, ID_CB_CUES, wxT(""), wxDefaultPosition,
                   wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
  cob_cues->SetToolTip(TIP("Selects for which blocks mkvmerge will produce "
                           "index entries ( = cue entries). \"default\" is a "
                           "good choice for almost all situations."));
  cob_cues->SetSizeHints(0, -1);
  siz_fg->Add(cob_cues, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL,
              STDSPACING);

  siz_all->Add(siz_fg, 0, wxGROW | wxLEFT | wxRIGHT, LEFTRIGHTSPACING);

  cb_default =
    new wxCheckBox(this, ID_CB_MAKEDEFAULT, wxT("Make default track"));
  cb_default->SetValue(false);
  cb_default->SetToolTip(TIP("Make this track the default track for its type "
                             "(audio, video, subtitles). Players should "
                             "prefer tracks with the default track flag "
                             "set."));

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(cb_default, 0, wxALL | STDSPACING);
  siz_all->Add(siz_line, 0, wxLEFT | wxRIGHT, LEFTRIGHTSPACING);

  siz_fg = new wxFlexGridSizer(3);
  siz_fg->AddGrowableCol(1);

  st_tags = new wxStaticText(this, wxID_STATIC, wxT("Tags:"));
  st_tags->Enable(false);
  siz_fg->Add(st_tags, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  tc_tags = new wxTextCtrl(this, ID_TC_TAGS, wxT(""));
  siz_fg->Add(tc_tags, 1, wxGROW | wxALL, STDSPACING);
  b_browse_tags = new wxButton(this, ID_B_BROWSETAGS, wxT("Browse"));
  siz_fg->Add(b_browse_tags, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  st_timecodes = new wxStaticText(this, wxID_STATIC, wxT("Timecodes:"));
  st_timecodes->Enable(false);
  siz_fg->Add(st_timecodes, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  tc_timecodes = new wxTextCtrl(this, ID_TC_TIMECODES, wxT(""));
  tc_timecodes->SetToolTip(TIP("mkvmerge can read and use timecodes from an "
                               "external text file. This feature is a very "
                               "advanced feature. Almost all users should "
                               "leave this entry empty."));
  siz_fg->Add(tc_timecodes, 1, wxGROW | wxALL, STDSPACING);
  b_browse_timecodes = new wxButton(this, ID_B_BROWSE_TIMECODES,
                                    wxT("Browse"));
  b_browse_timecodes->SetToolTip(TIP("mkvmerge can read and use timecodes "
                                     "from an external text file. This "
                                     "feature is a very advanced feature. "
                                     "Almost all users should leave this "
                                     "entry empty."));
  siz_fg->Add(b_browse_timecodes, 0, wxALIGN_CENTER_VERTICAL | wxALL,
              STDSPACING);

  siz_all->Add(siz_fg, 0, wxGROW | wxLEFT | wxRIGHT, LEFTRIGHTSPACING);

//   siz_all->AddSpacer(TOPBOTTOMSPACING);

  setup_languages();
  setup_cues();

  SetSizer(siz_all);
}

void
tab_input_general::setup_cues() {
  cob_cues->Append(wxT("default"));
  cob_cues->Append(wxT("only for I frames"));
  cob_cues->Append(wxT("for all frames"));
  cob_cues->Append(wxT("none"));
}

void
tab_input_general::setup_languages() {
  wxArrayString popular_languages;
  wxString language;
  bool found;
  int i, j;

  if (sorted_iso_codes.Count() == 0) {
    for (i = 0; iso639_languages[i].iso639_2_code != NULL; i++) {
      if (iso639_languages[i].english_name == NULL)
        language = wxU(iso639_languages[i].iso639_2_code);
      else
        language.Printf(wxT("%s (%s)"),
                        wxUCS(iso639_languages[i].iso639_2_code),
                        wxUCS(iso639_languages[i].english_name));
      sorted_iso_codes.Add(language);
    }
    sorted_iso_codes.Sort();

    for (i = 0; iso639_languages[i].iso639_2_code != NULL; i++) {
      if (!is_popular_language_code(iso639_languages[i].iso639_2_code))
        continue;
      for (j = 0, found = false; j < popular_languages.Count(); j++)
        if (extract_language_code(popular_languages[j]) ==
            wxU(iso639_languages[i].iso639_2_code)) {
          found = true;
          break;
        }
      if (!found) {
        language.Printf(wxT("%s (%s)"),
                        wxUCS(iso639_languages[i].iso639_2_code),
                        wxUCS(iso639_languages[i].english_name));
        popular_languages.Add(language);
      }
    }
    popular_languages.Sort();

    sorted_iso_codes.Insert(wxT("und (Undetermined)"), 0);
    sorted_iso_codes.Insert(wxT("---common---"), 1);
    for (i = 0; i < popular_languages.Count(); i++)
      sorted_iso_codes.Insert(popular_languages[i], i + 2);
    sorted_iso_codes.Insert(wxT("---all---"), i + 2);
  }

  for (i = 0; i < sorted_iso_codes.Count(); i++)
    cob_language->Append(sorted_iso_codes[i]);
  cob_language->SetSizeHints(0, -1);
}

void
tab_input_general::set_track_mode(mmg_track_t *t) {
  bool enable = (NULL != t) && !t->appending;

  st_language->Enable(enable);
  cob_language->Enable(enable);
  st_track_name->Enable(enable);
  tc_track_name->Enable(enable);
  st_cues->Enable(enable);
  cob_cues->Enable(enable);
  st_tags->Enable(enable);
  tc_tags->Enable(enable);
  b_browse_tags->Enable(enable);
  st_timecodes->Enable(NULL != t);
  tc_timecodes->Enable(NULL != t);
  b_browse_timecodes->Enable(NULL != t);
  cb_default->Enable(enable);

  if (NULL == t) {
    bool saved_dcvn = input->dont_copy_values_now;
    input->dont_copy_values_now = true;

    set_combobox_selection(cob_language, sorted_iso_codes[0]);
    tc_track_name->SetValue(wxT(""));
    set_combobox_selection(cob_cues, wxT("default"));
    cb_default->SetValue(false);
    tc_tags->SetValue(wxT(""));
    tc_timecodes->SetValue(wxT(""));

    input->dont_copy_values_now = saved_dcvn;
  }
}

void
tab_input_general::on_default_track_clicked(wxCommandEvent &evt) {
  mmg_track_t *t;

  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  t = tracks[input->selected_track];
  if (cb_default->GetValue()) {
    int idx;

    idx = default_track_checked(t->type);
    if (-1 != idx)
      tracks[idx]->default_track = false;
  }

  t->default_track = cb_default->GetValue();
}

void
tab_input_general::on_language_selected(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->language =
    cob_language->GetStringSelection();
}

void
tab_input_general::on_cues_selected(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->cues = cob_cues->GetStringSelection();
}

void
tab_input_general::on_browse_tags(wxCommandEvent &evt) {
  if (input->selected_track == -1)
    return;

  wxFileDialog dlg(NULL, wxT("Choose a tag file"), last_open_dir, wxT(""),
                   wxT("Tag files (*.xml;*.txt)|*.xml;*.txt|" ALLFILES),
                   wxOPEN);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    tracks[input->selected_track]->tags = dlg.GetPath();
    tc_tags->SetValue(dlg.GetPath());
  }
}

void
tab_input_general::on_browse_timecodes_clicked(wxCommandEvent &evt) {
  if (input->selected_track == -1)
    return;

  wxFileDialog dlg(NULL, wxT("Choose a timecodes file"), last_open_dir,
                   wxT(""), wxT("Timecode files (*.tmc;*.txt)|*.tmc;*.txt|"
                                ALLFILES), wxOPEN);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    tracks[input->selected_track]->timecodes = dlg.GetPath();
    tc_timecodes->SetValue(dlg.GetPath());
  }
}

void
tab_input_general::on_tags_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->tags = tc_tags->GetValue();
}

void
tab_input_general::on_timecodes_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->timecodes = tc_timecodes->GetValue();
}

void
tab_input_general::on_track_name_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->track_name = tc_track_name->GetValue();
}

IMPLEMENT_CLASS(tab_input_general, wxPanel);
BEGIN_EVENT_TABLE(tab_input_general, wxPanel)
  EVT_BUTTON(ID_B_BROWSETAGS, tab_input_general::on_browse_tags)
  EVT_BUTTON(ID_B_BROWSE_TIMECODES,
             tab_input_general::on_browse_timecodes_clicked)
  EVT_TEXT(ID_TC_TAGS, tab_input_general::on_tags_changed)
  EVT_TEXT(ID_TC_TIMECODES, tab_input_general::on_timecodes_changed)
  EVT_CHECKBOX(ID_CB_MAKEDEFAULT, tab_input_general::on_default_track_clicked)
  EVT_COMBOBOX(ID_CB_LANGUAGE, tab_input_general::on_language_selected)
  EVT_COMBOBOX(ID_CB_CUES, tab_input_general::on_cues_selected)
  EVT_TEXT(ID_TC_TRACKNAME, tab_input_general::on_track_name_changed)
END_EVENT_TABLE();

