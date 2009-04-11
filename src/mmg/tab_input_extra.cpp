/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "extra" tab -- "User defined track options" sub-tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <wx/wxprec.h>

#include <wx/wx.h>
#include <wx/dnd.h>
#include <wx/file.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/regex.h>
#include <wx/statline.h>

#include "common.h"
#include "extern_data.h"
#include "iso639.h"
#include "mkvmerge.h"
#include "mmg.h"
#include "mmg_dialog.h"
#include "tab_input.h"
#include "tab_global.h"

using namespace std;

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

  cob_cues = new wxMTX_COMBOBOX_TYPE(this, ID_CB_CUES, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
  cob_cues->SetToolTip(TIP("Selects for which blocks mkvmerge will produce index entries ( = cue entries). \"default\" is a good choice for almost all situations."));
  cob_cues->SetSizeHints(0, -1);
  siz_fg->Add(cob_cues, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  st_user_defined = new wxStaticText(this, -1, Z("User defined options:"));
  st_user_defined->Enable(false);
  siz_fg->Add(st_user_defined, 0, wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  tc_user_defined = new wxTextCtrl(this, ID_TC_USER_DEFINED, wxEmptyString);
  tc_user_defined->SetToolTip(TIP("Free-form edit field for user defined options for this track. What you input here is added after all the other options "
                                  "mmg adds so that you could overwrite any of mmg's options for this track. "
                                  "All occurences of the string \"<TID>\" will be replaced by the track's track ID."));
  tc_user_defined->SetSizeHints(0, -1);
  tc_user_defined->Enable(false);
  siz_fg->Add(tc_user_defined, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL, STDSPACING);

  siz_all->Add(siz_fg, 0, wxGROW | wxLEFT | wxRIGHT, LEFTRIGHTSPACING);

  siz_all->AddSpacer(TOPBOTTOMSPACING);

  setup_cues();

  SetSizer(siz_all);
}

void
tab_input_extra::setup_cues() {
  cob_cues_translations.add(wxT("default"),           Z("default"));
  cob_cues_translations.add(wxT("only for I frames"), Z("only for I frames"));
  cob_cues_translations.add(wxT("for all frames"),    Z("for all frames"));
  cob_cues_translations.add(wxT("none"),              Z("none"));

  int i;
  for (i = 0; cob_cues_translations.entries.size() > i; ++i)
    cob_cues->Append(cob_cues_translations.entries[i].translated);
}

void
tab_input_extra::set_track_mode(mmg_track_t *t) {
  bool enable = (NULL != t) && !t->appending;

  st_cues->Enable(enable);
  cob_cues->Enable(enable);
  st_user_defined->Enable(NULL != t);
  tc_user_defined->Enable(NULL != t);

  if (NULL == t) {
    bool saved_dcvn = input->dont_copy_values_now;
    input->dont_copy_values_now = true;

    set_combobox_selection(cob_cues, Z("default"));
    tc_user_defined->SetValue(wxEmptyString);

    input->dont_copy_values_now = saved_dcvn;
  }
}

void
tab_input_extra::on_user_defined_changed(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->user_defined = tc_user_defined->GetValue();
}

void
tab_input_extra::on_cues_selected(wxCommandEvent &evt) {
  if (input->dont_copy_values_now || (input->selected_track == -1))
    return;

  tracks[input->selected_track]->cues = cob_cues_translations.to_english(cob_cues->GetStringSelection());
}

IMPLEMENT_CLASS(tab_input_extra, wxPanel);
BEGIN_EVENT_TABLE(tab_input_extra, wxPanel)
  EVT_COMBOBOX(ID_CB_CUES,     tab_input_extra::on_cues_selected)
  EVT_TEXT(ID_TC_USER_DEFINED, tab_input_extra::on_user_defined_changed)
END_EVENT_TABLE();
