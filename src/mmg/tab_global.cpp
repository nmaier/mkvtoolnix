/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  tab_global.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <flo.wagner@gmx.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief "global" tab
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include "wx/wxprec.h"

#include "wx/wx.h"
#include "wx/notebook.h"
#include "wx/listctrl.h"
#include "wx/statline.h"
#include "wx/config.h"

#include "common.h"
#include "mmg.h"
#include "tab_global.h"

tab_global::tab_global(wxWindow *parent):
  wxPanel(parent, -1, wxDefaultPosition, wxSize(100, 400),
          wxTAB_TRAVERSAL) {
  uint32_t i;

  new wxStaticBox(this, -1, wxT("File/segment title"), wxPoint(10, 5),
                  wxSize(475, 50));
  new wxStaticText(this, -1, wxT("File/segment title:"), wxPoint(15, 25),
                   wxDefaultSize);
  tc_title =
    new wxTextCtrl(this, ID_TC_SEGMENTTITLE, wxT(""), wxPoint(160, 25 + YOFF),
                   wxSize(318, -1), 0);
  tc_title->SetToolTip(wxT("This is the title that players may show "
                           "as the 'main title' for this movie."));

  new wxStaticBox(this, -1, wxT("Split"), wxPoint(10, 60), wxSize(475, 75));

  cb_split =
    new wxCheckBox(this, ID_CB_SPLIT, wxT("Enable splitting"),
                   wxPoint(15, 80), wxDefaultSize, 0);
  cb_split->SetToolTip(wxT("Enables splitting of the output into more than "
                           "one file. You can split after a given size "
                           "or after a given amount of time has passed."));

  rb_split_by_size =
    new wxRadioButton(this, ID_RB_SPLITBYSIZE, wxT("by size:"),
                      wxPoint(130, 80), wxDefaultSize, wxRB_GROUP);
  rb_split_by_size->Enable(false);
  cob_split_by_size =
    new wxComboBox(this, ID_CB_SPLITBYSIZE, wxT(""), wxPoint(200, 80 + YOFF),
                   wxSize(100, -1), 0, NULL, wxCB_DROPDOWN);
  cob_split_by_size->Append(wxT(""));
  cob_split_by_size->Append(wxT("350M"));
  cob_split_by_size->Append(wxT("650M"));
  cob_split_by_size->Append(wxT("700M"));
  cob_split_by_size->Append(wxT("703M"));
  cob_split_by_size->Append(wxT("800M"));
  cob_split_by_size->Append(wxT("1000M"));
  cob_split_by_size->SetToolTip(wxT("The size after which a new output file "
                                    "is being started. The letters 'G', 'M' "
                                    "and 'K' can be used to indicate giga/"
                                    "mega/kilo bytes respectively. All units "
                                    "are based on 1024 (G = 1024^3, M = "
                                    "1024^2, K = 1024)."));
  cob_split_by_size->Enable(false);

  rb_split_by_time =
    new wxRadioButton(this, ID_RB_SPLITBYTIME, wxT("by time:"),
                      wxPoint(310, 80), wxDefaultSize, 0);
  rb_split_by_time->Enable(false);
  cob_split_by_time =
    new wxComboBox(this, ID_CB_SPLITBYTIME, wxT(""), wxPoint(380, 80 + YOFF),
                   wxSize(100, -1), 0, NULL, wxCB_DROPDOWN);
  cob_split_by_time->Append(wxT(""));
  cob_split_by_time->Append(wxT("01:00:00"));
  cob_split_by_time->Append(wxT("1800s"));
  cob_split_by_time->SetToolTip(wxT("The time after which a new output file "
                                    "is being started. The time can be given "
                                    "either in the form HH:MM:SS or as the "
                                    "number of seconds followed by 's'. "
                                    "Examples: 01:00:00 (after one hour) or "
                                    "1800s (after 1800 seconds)."));
  cob_split_by_time->Enable(false);

  cb_link =
    new wxCheckBox(this, ID_CB_LINK, wxT("link files"),
                   wxPoint(15, 105), wxDefaultSize, 0);
  cb_link->SetToolTip(wxT("Use 'segment linking' for the resulting "
                          "files. For an in-depth explanation of this "
                          "feature consult the mkvmerge documentation."));
  cb_link->SetValue(false);
  cb_link->Enable(false);

  new wxStaticText(this, wxID_STATIC, wxT("max. number of files:"),
                   wxPoint(250, 105), wxDefaultSize, 0);
  tc_split_max_files =
    new wxTextCtrl(this, ID_TC_SPLITMAXFILES, wxT(""),
                   wxPoint(380, 105 + YOFF), wxSize(100, -1), 0);
  tc_split_max_files->SetToolTip(wxT("The maximum number of files that will "
                                     "be created even if the last file might "
                                     "contain more bytes/time than wanted. "
                                     "Useful e.g. when you want exactly two "
                                     "files."));
  tc_split_max_files->Enable(false);

  new wxStaticBox(this, -1, wxT("File/segment linking"), wxPoint(10, 140),
                  wxSize(475, 75));
  new wxStaticText(this, -1, wxT("Previous segment UID:"), wxPoint(15, 160),
                   wxDefaultSize);
  tc_previous_segment_uid =
    new wxTextCtrl(this, ID_TC_PREVIOUSSEGMENTUID, wxT(""),
                   wxPoint(160, 160 + YOFF), wxSize(318, -1), 0);
  tc_previous_segment_uid->SetToolTip(wxT("For an in-depth explanantion of "
                                          "file/segment linking and this "
                                          "feature please read mkvmerge's "
                                          "documentation."));
  new wxStaticText(this, -1, wxT("Next segment UID:"), wxPoint(15, 185),
                   wxDefaultSize);
  tc_next_segment_uid =
    new wxTextCtrl(this, ID_TC_PREVIOUSSEGMENTUID, wxT(""),
                   wxPoint(160, 185 + YOFF), wxSize(318, -1), 0);
  tc_next_segment_uid->SetToolTip(wxT("For an in-depth explanantion of "
                                      "file/segment linking and this "
                                      "feature please read mkvmerge's "
                                      "documentation."));

  new wxStaticBox(this, -1, wxT("Chapters"), wxPoint(10, 220),
                  wxSize(475, 100));
  new wxStaticText(this, -1, wxT("Chapter file:"), wxPoint(15, 240),
                   wxDefaultSize, 0);
  tc_chapters =
    new wxTextCtrl(this, ID_TC_CHAPTERS, wxT(""), wxPoint(100, 240 + YOFF),
                   wxSize(290, -1));
  tc_chapters->SetToolTip(wxT("mkvmerge supports two chapter formats: The "
                              "OGM like text format and the full featured "
                              "XML format."));
  wxButton *b_browse_chapters =
    new wxButton(this, ID_B_BROWSECHAPTERS, wxT("Browse"),
                 wxPoint(400, 240 + YOFF), wxDefaultSize, 0);
  b_browse_chapters->SetToolTip(wxT("mkvmerge supports two chapter formats: "
                                    "The OGM like text format and the full "
                                    "featured XML format."));

  new wxStaticText(this, -1, wxT("Language:"), wxPoint(15, 265),
                   wxDefaultSize, 0);
  cob_chap_language =
    new wxComboBox(this, ID_CB_CHAPTERLANGUAGE, wxT(""),
                   wxPoint(100, 265 + YOFF),
                   wxSize(160, -1), 0, NULL, wxCB_DROPDOWN);
  cob_chap_language->Append(wxT(""));
  for (i = 0; i < sorted_iso_codes.Count(); i++)
    cob_chap_language->Append(sorted_iso_codes[i]);
  cob_chap_language->SetToolTip(wxT("mkvmerge supports two chapter formats: "
                                    "The OGM like text format and the full "
                                    "featured XML format. This option "
                                    "specifies the language to be associated "
                                    "with chapters if the OGM chapter format "
                                    "is used. It is ignored for XML chapter "
                                    "files."));
  new wxStaticText(this, -1, wxT("Charset:"), wxPoint(270, 265),
                   wxDefaultSize, 0);
  cob_chap_charset =
    new wxComboBox(this, ID_CB_CHAPTERCHARSET, wxT(""),
                   wxPoint(330, 265 + YOFF),
                   wxSize(150, -1), 0, NULL, wxCB_DROPDOWN);
  cob_chap_charset->Append(wxT(""));
  for (i = 0; i < sorted_charsets.Count(); i++)
    cob_chap_charset->Append(sorted_charsets[i]);
  cob_chap_charset->SetToolTip(wxT("mkvmerge supports two chapter formats: "
                                   "The OGM like text format and the full "
                                   "featured XML format. If the OGM format "
                                   "is used and the file's charset is not "
                                   "recognized correctly then this option "
                                   "can be used to correct that. This option "
                                   "is ignored for XML chapter files."));
  new wxStaticText(this, -1, wxT("Cue name format:"), wxPoint(15, 290),
                   wxDefaultSize, 0);
  tc_cue_name_format =
    new wxTextCtrl(this, ID_TC_CUENAMEFORMAT, wxT(""),
                   wxPoint(125, 290 + YOFF), wxSize(135, -1));
  tc_cue_name_format->SetToolTip(wxT("mkvmerge can read CUE sheets for audio "
                                     "cds and automatically convert them to "
                                     "chapters. This option controls how the "
                                     "chapter names are created. The sequence "
                                     "'%p' is replaced by the track's "
                                     "PERFORMER, the sequence '%t' by the "
                                     "track's TITLE, '%n' by the track's "
                                     "number and '%N' by the track's number "
                                     "padded with "
                                     "a leading 0 for track numbers < 10. The "
                                     "rest is copied as is. If nothing is "
                                     "entered then '%p - %t' will be used."));

  new wxStaticBox(this, -1, wxT("Global tags"), wxPoint(10, 325),
                  wxSize(475, 50));
  new wxStaticText(this, -1, wxT("Tag file:"), wxPoint(15, 345),
                   wxDefaultSize, 0);
  tc_global_tags =
    new wxTextCtrl(this, ID_TC_GLOBALTAGS, wxT(""), wxPoint(100, 345 + YOFF),
                   wxSize(290, -1));
  wxButton *b_browse_global_tags =
    new wxButton(this, ID_B_BROWSEGLOBALTAGS, wxT("Browse"),
                 wxPoint(400, 345 + YOFF), wxDefaultSize, 0);
  b_browse_global_tags->SetToolTip(wxT("The difference between tags associated"
                                       " with a track and global tags is "
                                       "explained in mkvmerge's documentation."
                                       " In short: global tags apply to the "
                                       "complete file while the tags you can "
                                       "add on the 'input' tab apply to only "
                                       "one track."));
}

void tab_global::on_browse_global_tags(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, wxT("Choose the tags file"), last_open_dir, wxT(""),
                   wxT("Tag files (*.xml)|*.xml|" ALLFILES), wxOPEN);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    tc_global_tags->SetValue(dlg.GetPath());
  }
}

void tab_global::on_browse_chapters(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, wxT("Choose the chapter file"), last_open_dir,
                   wxT(""),
                   wxT("Chapter files (*.xml;*.txt;*.cue)|*.xml;*.txt;*.cue|"
                       ALLFILES), wxOPEN);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    tc_chapters->SetValue(dlg.GetPath());
  }
}

void tab_global::on_split_clicked(wxCommandEvent &evt) {
  bool ec, er;

  ec = cb_split->IsChecked();
  er = rb_split_by_size->GetValue();
  rb_split_by_size->Enable(ec);
  cob_split_by_size->Enable(ec && er);
  rb_split_by_time->Enable(ec);
  cob_split_by_time->Enable(ec && !er);
  cb_link->Enable(ec);
  tc_split_max_files->Enable(ec);
}

void tab_global::on_splitby_clicked(wxCommandEvent &evt) {
  bool er;

  er = rb_split_by_size->GetValue();
  cob_split_by_size->Enable(er);
  cob_split_by_time->Enable(!er);
}

void tab_global::load(wxConfigBase *cfg) {
  wxString s;
  bool b, ec, er;

  cfg->SetPath(wxT("/global"));
  cfg->Read(wxT("segment_title"), &s);
  tc_title->SetValue(s);

  ec = false;
  er = true;
  cfg->Read(wxT("enable_splitting"), &ec);
  cb_split->SetValue(ec);
  cfg->Read(wxT("split_by_size"), &er);
  if (er)
    rb_split_by_size->SetValue(true);
  else
    rb_split_by_time->SetValue(true);
  cfg->Read(wxT("split_after_bytes"), &s);
  cob_split_by_size->SetValue(s);
  cfg->Read(wxT("split_after_time"), &s);
  cob_split_by_time->SetValue(s);
  cfg->Read(wxT("split_max_files"), &s);
  tc_split_max_files->SetValue(s);
  b = false;
  if (cfg->Read(wxT("link"), &b))
    cb_link->SetValue(b);
  else if (cfg->Read(wxT("dont_link"), &b))
    cb_link->SetValue(!b);
  else
    cb_link->SetValue(true);

  rb_split_by_size->Enable(ec);
  cob_split_by_size->Enable(ec && er);
  rb_split_by_time->Enable(ec);
  cob_split_by_time->Enable(ec && !er);
  cb_link->Enable(ec);
  tc_split_max_files->Enable(ec);

  cfg->Read(wxT("previous_segment_uid"), &s);
  tc_previous_segment_uid->SetValue(s);
  cfg->Read(wxT("next_segment_uid"), &s);
  tc_next_segment_uid->SetValue(s);

  cfg->Read(wxT("chapters"), &s);
  tc_chapters->SetValue(s);
  cfg->Read(wxT("chapter_language"), &s);
  cob_chap_language->SetValue(s);
  cfg->Read(wxT("chapter_charset"), &s);
  cob_chap_charset->SetValue(s);
  cfg->Read(wxT("cue_name_format"), &s);
  tc_cue_name_format->SetValue(s);

  cfg->Read(wxT("global_tags"), &s);
  tc_global_tags->SetValue(s);

  if (!cfg->Read(wxT("title_was_present"), &b))
    b = false;
  title_was_present = b;
}

void tab_global::save(wxConfigBase *cfg) {
  cfg->SetPath(wxT("/global"));
  cfg->Write(wxT("segment_title"), tc_title->GetValue());

  cfg->Write(wxT("enable_splitting"), cb_split->IsChecked());
  cfg->Write(wxT("split_by_size"), rb_split_by_size->GetValue());
  cfg->Write(wxT("split_after_bytes"), cob_split_by_size->GetValue());
  cfg->Write(wxT("split_after_time"), cob_split_by_time->GetValue());
  cfg->Write(wxT("split_max_files"), tc_split_max_files->GetValue());
  cfg->Write(wxT("link"), cb_link->IsChecked());

  cfg->Write(wxT("previous_segment_uid"), tc_previous_segment_uid->GetValue());
  cfg->Write(wxT("next_segment_uid"), tc_next_segment_uid->GetValue());

  cfg->Write(wxT("chapters"), tc_chapters->GetValue());
  cfg->Write(wxT("chapter_language"), cob_chap_language->GetValue());
  cfg->Write(wxT("chapter_charset"), cob_chap_charset->GetValue());
  cfg->Write(wxT("cue_name_format"), tc_cue_name_format->GetValue());

  cfg->Write(wxT("global_tags"), tc_global_tags->GetValue());

  cfg->Write(wxT("title_was_present"), title_was_present);
}

bool
tab_global::validate_settings() {
  string s;
  int64_t dummy_i, mod;
  char c;

  if (cb_split->GetValue()) {
    if (rb_split_by_size->GetValue()) {
      s = wxMB(cob_split_by_size->GetValue());
      strip(s);
      if (s.length() == 0) {
        wxMessageBox(wxT("Splitting by size was selected, but no size has "
                         "been given."), wxT("mkvmerge GUI error"),
                     wxOK | wxCENTER | wxICON_ERROR);
        return false;
      }
      c = s[s.length() - 1];
      mod = 1;
      if (tolower(c) == 'k')
        mod = 1024;
      else if (tolower(c) == 'm')
        mod = 1024 * 1024;
      else if (tolower(c) == 'g')
        mod = 1024 * 1024 * 1024;
      if (mod != 1)
        s.erase(s.length() - 1);
      else if (!isdigit(c)) {
        wxMessageBox(wxT("The format of the split size is invalid."),
                     wxT("mkvmerge GUI error"),
                     wxOK | wxCENTER | wxICON_ERROR);
        return false;
      }
      if ((s.length() == 0) || !parse_int(s.c_str(), dummy_i)) {
        wxMessageBox(wxT("The format of the split size is invalid."),
                     wxT("mkvmerge GUI error"),
                     wxOK | wxCENTER | wxICON_ERROR);
        return false;
      }
      if ((dummy_i * mod) < 1024 * 1024) {
        wxMessageBox(wxT("The format of the split size is invalid (size too "
                         "small)."), wxT("mkvmerge GUI error"),
                     wxOK | wxCENTER | wxICON_ERROR);
        return false;
      }

    } else {
      s = wxMB(cob_split_by_time->GetValue());
      strip(s);
      if (s.length() == 0) {
        wxMessageBox(wxT("Splitting by time was selected, but no time has "
                         "been given."), wxT("mkvmerge GUI error"),
                     wxOK | wxCENTER | wxICON_ERROR);
        return false;
      }
      c = s[s.length() - 1];
      if (tolower(c) == 's') {
        s.erase(s.length() - 1);
        if ((s.length() == 0) || !parse_int(s.c_str(), dummy_i) ||
            (dummy_i <= 0)) {
          wxMessageBox(wxT("The format of the split time is invalid."),
                       wxT("mkvmerge GUI error"), wxOK | wxCENTER |
                       wxICON_ERROR);
          return false;
        }

      } else if ((s.length() != 8) || (s[2] != ':') || (s[5] != ':') ||
                 !isdigit(s[0]) || !isdigit(s[1]) || !isdigit(s[3]) ||
                 !isdigit(s[4]) || !isdigit(s[6]) || !isdigit(s[7])) {
        wxMessageBox(wxT("The format of the split time is invalid."),
                     wxT("mkvmerge GUI error"),
                     wxOK | wxCENTER | wxICON_ERROR);
        return false;
      }
    }

    s = wxMB(tc_split_max_files->GetValue());
    strip(s);
    if ((s.length() > 0) && (!parse_int(s.c_str(), dummy_i) ||
                             (dummy_i <= 1))) {
      wxMessageBox(wxT("Invalid number of max. split files given."),
                   wxT("mkvmerge GUI error"), wxOK | wxCENTER | wxICON_ERROR);
      return false;
    }
  }

  return true;
}

IMPLEMENT_CLASS(tab_global, wxPanel);
BEGIN_EVENT_TABLE(tab_global, wxPanel)
  EVT_BUTTON(ID_B_BROWSEGLOBALTAGS, tab_global::on_browse_global_tags)
  EVT_BUTTON(ID_B_BROWSECHAPTERS, tab_global::on_browse_chapters)
  EVT_CHECKBOX(ID_CB_SPLIT, tab_global::on_split_clicked)
  EVT_RADIOBUTTON(ID_RB_SPLITBYSIZE, tab_global::on_splitby_clicked)
  EVT_RADIOBUTTON(ID_RB_SPLITBYTIME, tab_global::on_splitby_clicked)
END_EVENT_TABLE();
