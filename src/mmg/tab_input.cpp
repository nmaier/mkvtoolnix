/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  tab_input.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <root@sirelvis.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief "input" tab
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include "wx/wxprec.h"

#include "wx/wx.h"
#include "wx/notebook.h"
#include "wx/listctrl.h"
#include "wx/statline.h"

#include "mmg.h"
#include "common.h"
#include "extern_data.h"
#include "iso639.h"

tab_input::tab_input(wxWindow *parent):
  wxPanel(parent, -1, wxDefaultPosition, wxSize(100, 400), wxSUNKEN_BORDER |
          wxTAB_TRAVERSAL) {
  int i, selidx;
  wxString language;

  new wxStaticText(this, wxID_STATIC, _("Input files:"), wxPoint(5, 5),
                   wxDefaultSize, 0);
  lb_input_files =
    new wxListBox(this, ID_LB_INPUTFILES, wxPoint(5, 24), wxSize(435, 60), 0);

  b_add_file =
    new wxButton(this, ID_B_ADDFILE, _("+"), wxPoint(450, 24),
                 wxSize(24, -1), 0);
  b_remove_file =
    new wxButton(this, ID_B_REMOVEFILE, _("-"), wxPoint(450, 56),
                 wxSize(24, -1), 0);
  b_remove_file->Enable(false);
  new wxStaticText(this, wxID_STATIC, _("File options:"), wxPoint(5, 90),
                   wxDefaultSize, 0);
  cb_no_chapters =
    new wxCheckBox(this, ID_CB_NOCHAPTERS, _("No chapters"), wxPoint(5, 110),
                   wxSize(100, -1), 0);
  cb_no_chapters->SetValue(false);
  cb_no_chapters->SetToolTip(_("Do not copy chapters from this file. Only "
                               "applies to Matroska files."));
  cb_no_chapters->Enable(false);
  new wxStaticText(this, wxID_STATIC, _("Tracks:"), wxPoint(5, 140),
                   wxDefaultSize, 0);
  clb_tracks =
    new wxCheckListBox(this, ID_CLB_TRACKS, wxPoint(5, 160), wxSize(480, 100),
                       0, NULL, 0);
  clb_tracks->Enable(false);
  new wxStaticText(this, wxID_STATIC, _("Track options:"), wxPoint(5, 265),
                   wxDefaultSize, 0);
  new wxStaticText(this, wxID_STATIC, _("Language:"), wxPoint(5, 285),
                   wxDefaultSize, 0);
  wxString *cob_language_strings = NULL;
  cob_language =
    new wxComboBox(this, ID_CB_LANGUAGE, _(""), wxPoint(120, 285),
                   wxSize(100, -1), 0, cob_language_strings,
                   wxCB_DROPDOWN | wxCB_SORT | wxCB_READONLY);
  cob_language->SetToolTip(_("Language for this track. Select one of the "
                             "ISO639-2 language codes."));
  selidx = -1;
  for (i = 0; iso639_languages[i].iso639_2_code != NULL; i++) {
    if (iso639_languages[i].english_name == NULL)
      language = iso639_languages[i].iso639_2_code;
    else
      language.Printf("%s (%s)", iso639_languages[i].iso639_2_code,
                      iso639_languages[i].english_name);
    if (!strcmp(iso639_languages[i].iso639_2_code, "eng") && (selidx == -1)) {
      cob_language->SetValue(language);
      selidx = i;
    }
    cob_language->Append(language);
  }
  new wxStaticText(this, wxID_STATIC, _("Delay (in ms):"),
                   wxPoint(255, 285), wxDefaultSize, 0);
  tc_delay =
    new wxTextCtrl(this, ID_TC_DELAY, _(""), wxPoint(370, 285),
                   wxSize(100, -1), 0);
  tc_delay->SetToolTip(_("Delay this track by a couple of ms. Can be "
                         "negative. Only applies to audio and subtitle tracks."
                         " Some audio formats cannot be delayed at the "
                         "moment."));
  new wxStaticText(this, wxID_STATIC, _("Track name:"), wxPoint(5, 310),
                   wxDefaultSize, 0);
  tc_track_name =
    new wxTextCtrl(this, ID_TC_TRACKNAME, _(""), wxPoint(120, 310),
                   wxSize(100, -1), 0);
  tc_track_name->SetToolTip(_("Name for this track, e.g. \"director's "
                              "comments\"."));
  new wxStaticText(this, wxID_STATIC, _("Stretch by:"), wxPoint(255, 310),
                   wxDefaultSize, 0);
  tc_stretch =
    new wxTextCtrl(this, ID_TC_STRETCH, _(""), wxPoint(370, 310),
                   wxSize(100, -1), 0);
  tc_stretch->SetToolTip(_("Stretch the audio or subtitle track by a factor. "
                           "This should be a positive floating point number. "
                           "Not all formats can be stretched at the moment."));
  new wxStaticText(this, wxID_STATIC, _("Cues:"), wxPoint(5, 335),
                   wxDefaultSize, 0);
  wxString *cob_cues_strings = NULL;
  cob_cues =
    new wxComboBox(this, ID_CB_CUES, _(""), wxPoint(120, 335),
                   wxSize(100, -1), 0, cob_cues_strings, wxCB_DROPDOWN |
                   wxCB_READONLY);
  cob_cues->SetToolTip(_("Selects for which blocks mkvmerge will produce "
                         "index entries ( = cue entries). \"default\" is a "
                         "good choice for almost all situations."));
  cob_cues->Append(_("default"));
  cob_cues->Append(_("only for I frames"));
  cob_cues->Append(_("for all frames"));
  cob_cues->Append(_("none"));
  new wxStaticText(this, wxID_STATIC, _("Subtitle charset:"),
                   wxPoint(255, 335), wxDefaultSize, 0);
  wxString *cob_sub_charsetStrings = NULL;
  cob_sub_charset =
    new wxComboBox(this, ID_CB_SUBTITLECHARSET, _(""), wxPoint(370, 335),
                   wxDefaultSize, 0, cob_sub_charsetStrings,
                   wxCB_DROPDOWN|wxCB_SORT);
  cob_sub_charset->SetToolTip(_("Selects the character set a subtitle file "
                                "was written with. Only needed for non-UTF "
                                "files that mkvmerge does not detect "
                                "correctly."));
  cob_sub_charset->Append(_("default"));
  for (i = 0; sub_charsets[i] != NULL; i++)
    cob_sub_charset->Append(_(sub_charsets[i]));
  cb_default =
    new wxCheckBox(this, ID_CB_MAKEDEFAULT, _("Make default track"),
                   wxPoint(5, 355), wxSize(200, -1), 0);
  cb_default->SetValue(false);
  cb_default->SetToolTip(_("Make this track the default track for its type "
                           "(audio, video, subtitles). Players should prefer "
                           "tracks with the default track flag set."));
  cb_aac_is_sbr =
    new wxCheckBox(this, ID_CB_AACISSBR, _("AAC is SBR/HE-AAC/AAC+"),
                   wxPoint(255, 355), wxSize(200, -1), 0);
  cb_aac_is_sbr->SetValue(false);
  cb_aac_is_sbr->SetToolTip(_("This track contains SBR AAC/HE-AAC/AAC+ data. "
                              "Only needed for AAC input files, because SBR "
                              "AAC cannot be detected automatically for "
                              "these files. Not needed for AAC tracks read "
                              "from MP4 or Matroska files."));

  new wxStaticText(this, wxID_STATIC, _("Tags:"), wxPoint(5, 385),
                   wxDefaultSize, 0);
  tc_tags =
    new wxTextCtrl(this, ID_TC_TAGS, _(""), wxPoint(120, 385),
                   wxSize(250, -1), wxTE_READONLY);
  b_browse_tags =
    new wxButton(this, ID_B_BROWSETAGS, _("Browse"), wxPoint(390, 385),
                 wxDefaultSize, 0);

  no_track_mode();
  selected_file = -1;
  selected_track = -1;
}

void tab_input::no_track_mode() {
  cob_language->Enable(false);
  tc_delay->Enable(false);
  tc_track_name->Enable(false);
  tc_stretch->Enable(false);
  cob_cues->Enable(false);
  cob_sub_charset->Enable(false);
  cb_default->Enable(false);
  cb_aac_is_sbr->Enable(false);
  tc_tags->Enable(false);
  b_browse_tags->Enable(false);
}

void tab_input::audio_track_mode() {
  cob_language->Enable(true);
  tc_delay->Enable(true);
  tc_track_name->Enable(true);
  tc_stretch->Enable(true);
  cob_cues->Enable(true);
  cob_sub_charset->Enable(false);
  cb_default->Enable(true);
  cb_aac_is_sbr->Enable(true);
  tc_tags->Enable(true);
  b_browse_tags->Enable(true);
}

void tab_input::video_track_mode() {
  cob_language->Enable(true);
  tc_delay->Enable(false);
  tc_track_name->Enable(true);
  tc_stretch->Enable(false);
  cob_cues->Enable(true);
  cob_sub_charset->Enable(false);
  cb_default->Enable(true);
  cb_aac_is_sbr->Enable(false);
  tc_tags->Enable(true);
  b_browse_tags->Enable(true);
}

void tab_input::subtitle_track_mode() {
  cob_language->Enable(true);
  tc_delay->Enable(true);
  tc_track_name->Enable(true);
  tc_stretch->Enable(true);
  cob_cues->Enable(true);
  cob_sub_charset->Enable(true);
  cb_default->Enable(true);
  cb_aac_is_sbr->Enable(false);
  tc_tags->Enable(true);
  b_browse_tags->Enable(true);
}

void tab_input::on_add_file(wxCommandEvent &evt) {
  mmg_file_t file;
  wxString name, command, id, type, exact;
  wxArrayString output, errors;
  int result;
  unsigned int i;

  wxFileDialog dlg(NULL, "Choose an input file", "", last_open_dir,
                   _T("Media files (*.aac;*.ac3;*.ass;*.avi;*.dts;*.mp3;*.mka;"
                      "*.mkv;*.mov;*.mp4;*.ogm;*.ogg;*.rm;*.rmvb;*.srt;*.ssa;"
                      "*.wav)|"
                      "*.aac;*.ac3;*.ass;*.avi;*.dts;*.mp3;*.mka;*.mkv;*.mov;"
                      "*.mp4;*.ogm;*.ogg;*.rm;*.rmvb;*.srt;*.ssa;*.wav|"
                      "AAC (Advanced Audio Coding) (*.aac;*.mp4)|*.aac;*.mp4|"
                      "A/52 (aka AC3) (*.ac3)|*.ac3|"
                      "AVI (Audio/Video Interleaved) (*.avi)|*.avi|"
                      "DTS (Digital Theater System) (*.dts)|*.dts|"
                      "MPEG1 layer III audio (*.mp3)|*.mp3|"
                      "Matroska A/V files (*.mka;*.mkv)|*.mka;*.mkv|"
                      "QuickTime/MP4 A/V (*.mov;*.mp4)|*.mov;*.mp4|"
                      "Audio/Video embedded in OGG (*.ogg;*.ogm)|*.ogg;*.ogm|"
                      "RealMedia Files (*.rm;*.rmvb)|*.rm;*.rmvb|"
                      "SRT text subtitles (*.srt)|*.srt|"
                      "SSA/ASS text subtitles (*.ssa;*.ass)|*.ssa;*.ass|"
                      "WAVE (uncompressed PCM) (*.wav)|*.wav|"
                      "All Files (*.*)|*.*"), wxOPEN);

  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();

    command = "mkvmerge -i \"" + dlg.GetPath() + "\"";
    if ((result = wxExecute(command, output, errors)) != 0) {
      name.Printf("'mkvmerge -i' failed. Return code: %d\n\n", result);
      for (i = 0; i < output.Count(); i++)
        name += break_line(output[i]) + "\n";
      name += "\n";
      for (i = 0; i < errors.Count(); i++)
        name += break_line(errors[i]) + "\n";
      wxMessageBox(name, "'mkvmerge -i' failed");
      return;
    }

    memset(&file, 0, sizeof(mmg_file_t));
    file.tracks = new vector<mmg_track_t>;

    for (i = 0; i < output.Count(); i++) {
      if (output[i].Find("Track") == 0) {
        mmg_track_t track;

        memset(&track, 0, sizeof(mmg_track_t));
        id = output[i].AfterFirst(' ').AfterFirst(' ').BeforeFirst(':');
        type = output[i].AfterFirst(':').BeforeFirst('(').Mid(1).RemoveLast();
        exact = output[i].AfterFirst('(').BeforeFirst(')');
        if (type == "audio")
          track.type = 'a';
        else if (type == "video")
          track.type = 'v';
        else if (type == "subtitles")
          track.type = 's';
        else
          track.type = '?';
        parse_int(id, track.id);
        track.ctype = new wxString(exact);
        track.enabled = true;
        track.language = new wxString("eng (English)");
        track.sub_charset = new wxString("default");
        track.cues = new wxString("default");
        track.track_name = new wxString("");
        track.delay = new wxString("");
        track.stretch = new wxString("");
        track.tags = new wxString("");

        file.tracks->push_back(track);
      }
    }

    if (file.tracks->size() == 0) {
      delete file.tracks;
      wxMessageBox("The input file '" + dlg.GetPath() + "' does not contain "
                   "any tracks.", "No tracks found");
      return;
    }

    name = dlg.GetFilename();
    name += " (";
    name += last_open_dir;
    name += ")";
    lb_input_files->Append(name);

    file.file_name = new wxString(dlg.GetPath());
    files->push_back(file);
  }
}

void tab_input::on_remove_file(wxCommandEvent &evt) {
  mmg_file_t *f;
  mmg_track_t *t;
  vector<mmg_file_t>::iterator eit;
  uint32_t i;

  if (selected_file == -1)
    return;

  f = &(*files)[selected_file];
  for (i = 0; i < f->tracks->size(); i++) {
    t = &(*f->tracks)[i];
    delete t->ctype;
    delete t->language;
    delete t->sub_charset;
    delete t->cues;
    delete t->track_name;
    delete t->delay;
    delete t->stretch;
    delete t->tags;
  }
  delete f->tracks;
  delete f->file_name;
  eit = files->begin();
  eit += selected_file;
  files->erase(eit);
  lb_input_files->Delete(selected_file);
  selected_file = -1;
  cb_no_chapters->Enable(false);
  b_remove_file->Enable(false);
  clb_tracks->Enable(false);
  no_track_mode();
}

void tab_input::on_file_selected(wxCommandEvent &evt) {
  uint32_t i;
  mmg_file_t *f;
  mmg_track_t *t;
  wxString label;

  b_remove_file->Enable(true);
  cb_no_chapters->Enable(true);
  selected_file = lb_input_files->GetSelection();
  f = &(*files)[selected_file];
  cb_no_chapters->SetValue(f->no_chapters);

  clb_tracks->Clear();
  for (i = 0; i < f->tracks->size(); i++) {
    t = &(*f->tracks)[i];
    label.Printf("%s (ID %lld, type: %s)", t->ctype->c_str(), t->id,
                 t->type == 'a' ? "audio" :
                 t->type == 'v' ? "video" :
                 t->type == 's' ? "subtitles" : "unknown");
    clb_tracks->Append(label);
    clb_tracks->Check(i, t->enabled);
  }

  clb_tracks->Enable(true);
  selected_track = -1;
  no_track_mode();
}

void tab_input::on_nochapters_clicked(wxCommandEvent &evt) {
  if (selected_file -1)
    (*files)[selected_file].no_chapters = cb_no_chapters->GetValue();
}

void tab_input::on_track_selected(wxCommandEvent &evt) {
  mmg_file_t *f;
  mmg_track_t *t;

  if (selected_file == -1)
    return;

  selected_track = clb_tracks->GetSelection();
  f = &(*files)[selected_file];
  t = &(*f->tracks)[selected_track];

  if (t->type == 'a')
    audio_track_mode();
  else if (t->type == 'v')
    video_track_mode();
  else if (t->type == 's')
    subtitle_track_mode();

  cob_language->SetValue(*t->language);
  tc_track_name->SetValue(*t->track_name);
  cob_cues->SetValue(*t->cues);
  tc_delay->SetValue(*t->delay);
  tc_stretch->SetValue(*t->stretch);
  cob_sub_charset->SetValue(*t->sub_charset);
  tc_tags->SetValue(*t->tags);
  cb_default->SetValue(t->default_track);
  cb_aac_is_sbr->SetValue(t->aac_is_sbr);
}

void tab_input::on_track_enabled(wxCommandEvent &evt) {
  uint32_t i;
  mmg_file_t *f;

  if (selected_file == -1)
    return;

  f = &(*files)[selected_file];
  for (i = 0; i < f->tracks->size(); i++)
    (*f->tracks)[i].enabled = clb_tracks->IsChecked(i);
}

void tab_input::on_default_track_clicked(wxCommandEvent &evt) {
  if ((selected_file == -1) || (selected_track == -1))
    return;

  (*(*files)[selected_file].tracks)[selected_track].default_track =
    cb_default->GetValue();
}

void tab_input::on_aac_is_sbr_clicked(wxCommandEvent &evt) {
  if ((selected_file == -1) || (selected_track == -1))
    return;

  (*(*files)[selected_file].tracks)[selected_track].aac_is_sbr =
    cb_aac_is_sbr->GetValue();
}

void tab_input::on_language_selected(wxCommandEvent &evt) {
  if ((selected_file == -1) || (selected_track == -1))
    return;

  *(*(*files)[selected_file].tracks)[selected_track].language =
    cob_language->GetValue();
}

void tab_input::on_cues_selected(wxCommandEvent &evt) {
  if ((selected_file == -1) || (selected_track == -1))
    return;

  *(*(*files)[selected_file].tracks)[selected_track].cues =
    cob_cues->GetValue();
}

void tab_input::on_subcharset_selected(wxCommandEvent &evt) {
  if ((selected_file == -1) || (selected_track == -1))
    return;

  *(*(*files)[selected_file].tracks)[selected_track].sub_charset =
    cob_sub_charset->GetValue();
}

void tab_input::on_browse_tags(wxCommandEvent &evt) {
  if ((selected_file == -1) || (selected_track == -1))
    return;

  wxFileDialog dlg(NULL, "Choose a tag file", "", last_open_dir,
                   _T("Tag files (*.xml;*.txt)|*.xml;*.txt|"
                      "All Files (*.*)|*.*"), wxOPEN);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    *(*(*files)[selected_file].tracks)[selected_track].tags = dlg.GetPath();
    tc_tags->SetValue(dlg.GetPath());
  }
}

void tab_input::on_delay_changed(wxCommandEvent &evt) {
  if ((selected_file == -1) || (selected_track == -1))
    return;

  *(*(*files)[selected_file].tracks)[selected_track].delay =
    tc_delay->GetValue();
}

void tab_input::on_stretch_changed(wxCommandEvent &evt) {
  if ((selected_file == -1) || (selected_track == -1))
    return;

  *(*(*files)[selected_file].tracks)[selected_track].stretch =
    tc_stretch->GetValue();
}

void tab_input::on_track_name_changed(wxCommandEvent &evt) {
  if ((selected_file == -1) || (selected_track == -1))
    return;

  *(*(*files)[selected_file].tracks)[selected_track].track_name =
    tc_track_name->GetValue();
}

IMPLEMENT_CLASS(tab_input, wxPanel);
BEGIN_EVENT_TABLE(tab_input, wxPanel)
  EVT_BUTTON(ID_B_ADDFILE, tab_input::on_add_file)
  EVT_BUTTON(ID_B_REMOVEFILE, tab_input::on_remove_file)
  EVT_BUTTON(ID_B_BROWSETAGS, tab_input::on_browse_tags)

  EVT_LISTBOX(ID_LB_INPUTFILES, tab_input::on_file_selected)
  EVT_LISTBOX(ID_CLB_TRACKS, tab_input::on_track_selected)
  EVT_CHECKLISTBOX(ID_CLB_TRACKS, tab_input::on_track_enabled)

  EVT_CHECKBOX(ID_CB_NOCHAPTERS, tab_input::on_nochapters_clicked)
  EVT_CHECKBOX(ID_CB_MAKEDEFAULT, tab_input::on_default_track_clicked)
  EVT_CHECKBOX(ID_CB_AACISSBR, tab_input::on_aac_is_sbr_clicked)

  EVT_COMBOBOX(ID_CB_LANGUAGE, tab_input::on_language_selected)
  EVT_COMBOBOX(ID_CB_CUES, tab_input::on_cues_selected)
  EVT_COMBOBOX(ID_CB_SUBTITLECHARSET, tab_input::on_subcharset_selected)
  EVT_TEXT(ID_CB_SUBTITLECHARSET, tab_input::on_subcharset_selected)
  EVT_TEXT(ID_TC_DELAY, tab_input::on_delay_changed)
  EVT_TEXT(ID_TC_STRETCH, tab_input::on_stretch_changed)
  EVT_TEXT(ID_TC_TRACKNAME, tab_input::on_track_name_changed)

END_EVENT_TABLE();
