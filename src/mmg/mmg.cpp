#include "wx/wxprec.h"

#include "wx/wx.h"
#include "wx/notebook.h"
#include "wx/listctrl.h"
#include "wx/statline.h"

#include "mmg.h"
#include "common.h"
#include "extern_data.h"
#include "iso639.h"

mmg_dialog::mmg_dialog(): wxFrame(NULL, -1, "mkvmerge GUI", wxPoint(0, 0),
                                  wxSize(520, 600), wxCAPTION |
                                  wxMINIMIZE_BOX | wxSYSTEM_MENU) {
  int i;

  wxPanel *panel = new wxPanel(this, -1);
  wxBoxSizer *item2 = new wxBoxSizer(wxVERTICAL);
  this->SetSizer(item2);
  this->SetAutoLayout(true);

  wxNotebook *notebook =
    new wxNotebook(this, ID_NOTEBOOK, wxDefaultPosition, wxSize(500, 500),
                   wxNB_TOP);
  wxPanel *page1 =
    new wxPanel(notebook, ID_PANEL, wxDefaultPosition, wxSize(100, 400),
                wxSUNKEN_BORDER|wxTAB_TRAVERSAL);
  wxStaticText *item5 =
    new wxStaticText(page1, wxID_STATIC, _("Input files:"), wxPoint(5, 5),
                     wxDefaultSize, 0);
  lb_input_files =
    new wxListBox(page1, ID_LB_INPUTFILES, wxPoint(5, 24), wxSize(435, 60), 0);

  b_add_file =
    new wxButton(page1, ID_B_ADDFILE, _("+"), wxPoint(450, 24),
                 wxSize(24, -1), 0);
  b_remove_file =
    new wxButton(page1, ID_B_REMOVEFILE, _("-"), wxPoint(450, 56),
                 wxSize(24, -1), 0);
  b_remove_file->Enable(false);
  wxStaticText *item9 =
    new wxStaticText(page1, wxID_STATIC, _("File options:"), wxPoint(5, 90),
                     wxDefaultSize, 0);
  cb_no_chapters =
    new wxCheckBox(page1, ID_CB_NOCHAPTERS, _("No chapters"), wxPoint(5, 110),
                   wxSize(100, -1), 0);
  cb_no_chapters->SetValue(false);
  cb_no_chapters->SetToolTip(_("Do not copy chapters from this file. Only "
                               "applies to Matroska files."));
  cb_no_chapters->Enable(false);
  wxStaticText *item11 =
    new wxStaticText(page1, wxID_STATIC, _("Tracks:"), wxPoint(5, 140),
                     wxDefaultSize, 0);
  clb_tracks =
    new wxCheckListBox(page1, ID_CLB_TRACKS, wxPoint(5, 160), wxSize(480, 100),
                       0, NULL, 0);
  clb_tracks->Enable(false);
  wxStaticText *item13 =
    new wxStaticText(page1, wxID_STATIC, _("Track options:"), wxPoint(5, 265),
                     wxDefaultSize, 0);
  wxStaticText *item14 =
    new wxStaticText(page1, wxID_STATIC, _("Language:"), wxPoint(5, 285),
                     wxDefaultSize, 0);
  wxString *cob_language_strings = NULL;
  cob_language =
    new wxComboBox(page1, ID_CB_LANGUAGE, _(""), wxPoint(120, 285),
                   wxSize(100, -1), 0, cob_language_strings,
                   wxCB_DROPDOWN | wxCB_SORT | wxCB_READONLY);
  cob_language->SetToolTip(_("Language for this track. Select one of the "
                             "ISO639-2 language codes."));
  cob_language->Enable(false);
  wxStaticText *item16 =
    new wxStaticText(page1, wxID_STATIC, _("Delay (in ms):"),
                     wxPoint(255, 285), wxDefaultSize, 0);
  tc_delay =
    new wxTextCtrl(page1, ID_TC_DELAY, _(""), wxPoint(370, 285),
                   wxSize(100, -1), 0);
  tc_delay->SetToolTip(_("Delay this track by a couple of ms. Can be "
                         "negative. Only applies to audio and subtitle tracks."
                         " Some audio formats cannot be delayed at the "
                         "moment."));
  tc_delay->Enable(false);
  wxStaticText *item18 =
    new wxStaticText(page1, wxID_STATIC, _("Track name:"), wxPoint(5, 310),
                     wxDefaultSize, 0);
  tc_track_name =
    new wxTextCtrl(page1, ID_TC_TRACKNAME, _(""), wxPoint(120, 310),
                   wxSize(100, -1), 0);
  tc_track_name->SetToolTip(_("Name for this track, e.g. \"director's "
                              "comments\"."));
  tc_track_name->Enable(false);
  wxStaticText *item20 =
    new wxStaticText(page1, wxID_STATIC, _("Stretch by:"), wxPoint(255, 310),
                     wxDefaultSize, 0);
  tc_stretch =
    new wxTextCtrl(page1, ID_TC_STRETCH, _(""), wxPoint(370, 310),
                   wxSize(100, -1), 0);
  tc_stretch->SetToolTip(_("Stretch the audio or subtitle track by a factor. "
                           "This should be a positive floating point number. "
                           "Not all formats can be stretched at the moment."));
  tc_stretch->Enable(false);
  wxStaticText *item22 =
    new wxStaticText(page1, wxID_STATIC, _("Cues:"), wxPoint(5, 335),
                     wxDefaultSize, 0);
  wxString *cob_cues_strings = NULL;
  cob_cues =
    new wxComboBox(page1, ID_CB_CUES, _(""), wxPoint(120, 335),
                   wxSize(100, -1), 0, cob_cues_strings, wxCB_READONLY);
  cob_cues->SetToolTip(_("Selects for which blocks mkvmerge will produce "
                         "index entries ( = cue entries). \"default\" is a "
                         "good choice for almost all situations."));
  cob_cues->Append(_("default"));
  cob_cues->Append(_("only for I frames"));
  cob_cues->Append(_("for all frames"));
  cob_cues->Append(_("none"));
  cob_cues->Enable(false);
  wxStaticText *item24 =
    new wxStaticText(page1, wxID_STATIC, _("Subtitle charset:"),
                     wxPoint(255, 335), wxDefaultSize, 0);
  wxString *cob_sub_charsetStrings = NULL;
  cob_sub_charset =
    new wxComboBox(page1, ID_CB_SUBTITLECHARSET, _(""), wxPoint(370, 335),
                   wxDefaultSize, 0, cob_sub_charsetStrings,
                   wxCB_DROPDOWN|wxCB_SORT);
  cob_sub_charset->SetToolTip(_("Selects the character set a subtitle file "
                                "was written with. Only needed for non-UTF "
                                "files that mkvmerge does not detect "
                                "correctly."));
  cob_sub_charset->Enable(false);
  cob_sub_charset->Append(_("default"));
  for (i = 0; sub_charsets[i] != NULL; i++)
    cob_sub_charset->Append(_(sub_charsets[i]));
  cb_default =
    new wxCheckBox(page1, ID_CB_MAKEDEFAULT, _("Make default track"),
                   wxPoint(5, 355), wxSize(200, -1), 0);
  cb_default->SetValue(false);
  cb_default->SetToolTip(_("Make this track the default track for its type "
                           "(audio, video, subtitles). Players should prefer "
                           "tracks with the default track flag set."));
  cb_default->Enable(false);
  cb_aac_is_sbr =
    new wxCheckBox(page1, ID_CB_AACISSBR, _("AAC is SBR/HE-AAC/AAC+"),
                   wxPoint(255, 355), wxSize(200, -1), 0);
  cb_aac_is_sbr->SetValue(false);
  cb_aac_is_sbr->SetToolTip(_("This track contains SBR AAC/HE-AAC/AAC+ data. "
                              "Only needed for AAC input files, because SBR "
                              "AAC cannot be detected automatically for "
                              "these files. Not needed for AAC tracks read "
                              "from MP4 or Matroska files."));

  cb_aac_is_sbr->Enable(false);

  wxStaticText *item29 =
    new wxStaticText(page1, wxID_STATIC, _("Tags:"), wxPoint(5, 385),
                     wxDefaultSize, 0);
  tc_tags =
    new wxTextCtrl(page1, ID_TC_TAGS, _(""), wxPoint(120, 385),
                   wxSize(250, -1), 0);
  tc_tags->Enable(false);
  b_browse_tags =
    new wxButton(page1, ID_B_BROWSETAGS, _("Browse"), wxPoint(390, 385),
                 wxDefaultSize, 0);
  b_browse_tags->Enable(false);

  notebook->AddPage(page1, _("Input"));
  item2->Add(notebook, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

  wxStaticBox *item28Static = new wxStaticBox(this, -1, _("Output filename"));
  wxStaticBoxSizer *item28 = new wxStaticBoxSizer(item28Static, wxHORIZONTAL);
  item2->Add(item28, 0, wxALIGN_CENTER_HORIZONTAL|wxALL, 5);

  tc_output =
    new wxTextCtrl(this, ID_TC_OUTPUT, _(""), wxDefaultPosition,
                   wxSize(400, -1), 0);
  item28->Add(tc_output, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

  b_browse_output =
    new wxButton(this, ID_B_BROWSEOUTPUT, _("Browse"), wxDefaultPosition,
                 wxDefaultSize, 0);
  item28->Add(b_browse_output, 0, wxALIGN_CENTER_VERTICAL|wxALL, 5);

  files = new vector<mmg_file_t>;
}

void mmg_dialog::on_browse_output(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, "Choose an output file", "", "",
                   _T("Matroska A/V files (*.mka;*.mkv)|*.mka;*.mkv|"
                      "All Files (*.*)|*.*"), wxSAVE | wxOVERWRITE_PROMPT);
  if(dlg.ShowModal() == wxID_OK)
    tc_output->SetValue(dlg.GetPath());
}

wxString &break_line(wxString &line, int break_after) {
  uint32_t i, chars;
  wxString broken;

  for (i = 0, chars = 0; i < line.Length(); i++) {
    if (chars >= break_after) {
      if ((line[i] == ',') || (line[i] == '.') || (line[i] == '-')) {
        broken += line[i];
        broken += "\n";
        chars = 0;
      } else if (line[i] == ' ') {
        broken += "\n";
        chars = 0;
      } else if (line[i] == '(') {
        broken += "\n(";
        chars = 0;
      } else {
        broken += line[i];
        chars++;
      }
    } else if ((chars != 0) || (broken[i] != ' ')) {
      broken += line[i];
      chars++;
    }
  }

  line = broken;
  return line;
}

void mmg_dialog::on_add_file(wxCommandEvent &evt) {
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
        track.selected = true;

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

void mmg_dialog::on_remove_file(wxCommandEvent &evt) {
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
}

void mmg_dialog::on_file_selected(wxCommandEvent &evt) {
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
    clb_tracks->Check(i, t->selected);
  }

  clb_tracks->Enable(true);
}

void mmg_dialog::on_nochapters_clicked(wxCommandEvent &evt) {
  if (selected_file -1)
    (*files)[selected_file].no_chapters = cb_no_chapters->GetValue();
}

void mmg_dialog::on_track_selected(wxCommandEvent &evt) {
  
}

class mmg_app: public wxApp {
public:
  virtual bool OnInit();
};

bool mmg_app::OnInit() {
  mmg_dialog *dialog = new mmg_dialog();
  dialog->Show(true);

  return true;
}

IMPLEMENT_CLASS(mmg_dialog, wxFrame);
BEGIN_EVENT_TABLE(mmg_dialog, wxFrame)
  EVT_BUTTON(ID_B_ADDFILE, mmg_dialog::on_add_file)
  EVT_BUTTON(ID_B_REMOVEFILE, mmg_dialog::on_remove_file)
  EVT_BUTTON(ID_B_BROWSEOUTPUT, mmg_dialog::on_browse_output)

  EVT_LISTBOX(ID_LB_INPUTFILES, mmg_dialog::on_file_selected)

  EVT_CHECKBOX(ID_CB_NOCHAPTERS, mmg_dialog::on_nochapters_clicked)

END_EVENT_TABLE();

IMPLEMENT_APP(mmg_app)
