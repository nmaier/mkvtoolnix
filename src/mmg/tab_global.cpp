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

  new wxStaticBox(this, -1, _("File/segment title"), wxPoint(10, 5),
                  wxSize(475, 50));
  new wxStaticText(this, -1, _("File/segment title:"), wxPoint(15, 25),
                   wxDefaultSize);
  tc_title =
    new wxTextCtrl(this, ID_TC_SEGMENTTITLE, _(""), wxPoint(160, 25 + YOFF),
                   wxSize(318, -1), 0);
  tc_title->SetToolTip(_T("This is the title that players may show "
                          "as the 'main title' for this movie."));

  new wxStaticBox(this, -1, _("Split"), wxPoint(10, 60), wxSize(475, 75));

  cb_split =
    new wxCheckBox(this, ID_CB_SPLIT, _("Enable splitting"),
                   wxPoint(15, 80), wxDefaultSize, 0);
  cb_split->SetToolTip(_T("Enables splitting of the output into more than "
                          "one file. You can split after a given size "
                          "or after a given amount of time has passed."));

  rb_split_by_size =
    new wxRadioButton(this, ID_RB_SPLITBYSIZE, _("by size:"),
                      wxPoint(130, 80), wxDefaultSize, wxRB_GROUP);
  rb_split_by_size->Enable(false);
  cob_split_by_size =
    new wxComboBox(this, ID_CB_SPLITBYSIZE, _(""), wxPoint(200, 80 + YOFF),
                   wxSize(100, -1), 0, NULL, wxCB_DROPDOWN);
  cob_split_by_size->Append("");
  cob_split_by_size->Append("350M");
  cob_split_by_size->Append("650M");
  cob_split_by_size->Append("700M");
  cob_split_by_size->Append("703M");
  cob_split_by_size->Append("800M");
  cob_split_by_size->Append("1000M");
  cob_split_by_size->SetToolTip(_T("The size after which a new output file "
                                   "is being started. The letters 'G', 'M' "
                                   "and 'K' can be used to indicate giga/"
                                   "mega/kilo bytes respectively. All units "
                                   "are based on 1024 (G = 1024^3, M = 1024^2,"
                                   " K = 1024)."));
  cob_split_by_size->Enable(false);

  rb_split_by_time =
    new wxRadioButton(this, ID_RB_SPLITBYTIME, _("by time:"),
                      wxPoint(310, 80), wxDefaultSize, 0);
  rb_split_by_time->Enable(false);
  cob_split_by_time =
    new wxComboBox(this, ID_CB_SPLITBYTIME, _(""), wxPoint(380, 80 + YOFF),
                   wxSize(100, -1), 0, NULL, wxCB_DROPDOWN);
  cob_split_by_time->Append("");
  cob_split_by_time->Append("01:00:00");
  cob_split_by_time->Append("1800s");
  cob_split_by_time->SetToolTip(_T("The time after which a new output file "
                                   "is being started. The time can be given "
                                   "either in the form HH:MM:SS or as the "
                                   "number of seconds followed by 's'. "
                                   "Examples: 01:00:00 (after one hour) or "
                                   "1800s (after 1800 seconds)."));
  cob_split_by_time->Enable(false);

  cb_link =
    new wxCheckBox(this, ID_CB_LINK, _("link files"),
                   wxPoint(15, 105), wxDefaultSize, 0);
  cb_link->SetToolTip(_T("Use 'segment linking' for the resulting "
                         "files. For an in-depth explanation of this "
                         "feature consult the mkvmerge documentation."));
  cb_link->SetValue(false);
  cb_link->Enable(false);

  new wxStaticText(this, wxID_STATIC, _("max. number of files:"),
                   wxPoint(250, 105), wxDefaultSize, 0);
  tc_split_max_files =
    new wxTextCtrl(this, ID_TC_SPLITMAXFILES, _(""), wxPoint(380, 105 + YOFF),
                   wxSize(100, -1), 0);
  tc_split_max_files->SetToolTip(_T("The maximum number of files that will "
                                    "be created even if the last file might "
                                    "contain more bytes/time than wanted. "
                                    "Useful e.g. when you want exactly two "
                                    "files."));
  tc_split_max_files->Enable(false);

  new wxStaticBox(this, -1, _("File/segment linking"), wxPoint(10, 140),
                  wxSize(475, 75));
  new wxStaticText(this, -1, _("Previous segment UID:"), wxPoint(15, 160),
                   wxDefaultSize);
  tc_previous_segment_uid =
    new wxTextCtrl(this, ID_TC_PREVIOUSSEGMENTUID, _(""),
                   wxPoint(160, 160 + YOFF), wxSize(318, -1), 0);
  tc_previous_segment_uid->SetToolTip(_T("For an in-depth explanantion of "
                                         "file/segment linking and this "
                                         "feature please read mkvmerge's "
                                         "documentation."));
  new wxStaticText(this, -1, _("Next segment UID:"), wxPoint(15, 185),
                   wxDefaultSize);
  tc_next_segment_uid =
    new wxTextCtrl(this, ID_TC_PREVIOUSSEGMENTUID, _(""),
                   wxPoint(160, 185 + YOFF), wxSize(318, -1), 0);
  tc_next_segment_uid->SetToolTip(_T("For an in-depth explanantion of "
                                     "file/segment linking and this "
                                     "feature please read mkvmerge's "
                                     "documentation."));

  new wxStaticBox(this, -1, _("Chapters"), wxPoint(10, 220), wxSize(475, 100));
  new wxStaticText(this, -1, _("Chapter file:"), wxPoint(15, 240),
                   wxDefaultSize, 0);
  tc_chapters =
    new wxTextCtrl(this, ID_TC_CHAPTERS, _(""), wxPoint(100, 240 + YOFF),
                   wxSize(290, -1));
  tc_chapters->SetToolTip(_T("mkvmerge supports two chapter formats: The "
                             "OGM like text format and the full featured "
                             "XML format."));
  wxButton *b_browse_chapters =
    new wxButton(this, ID_B_BROWSECHAPTERS, _("Browse"),
                 wxPoint(400, 240 + YOFF), wxDefaultSize, 0);
  b_browse_chapters->SetToolTip(_T("mkvmerge supports two chapter formats: "
                                   "The OGM like text format and the full "
                                   "featured XML format."));

  new wxStaticText(this, -1, _("Language:"), wxPoint(15, 265),
                   wxDefaultSize, 0);
  cob_chap_language =
    new wxComboBox(this, ID_CB_CHAPTERLANGUAGE, _(""),
                   wxPoint(100, 265 + YOFF),
                   wxSize(160, -1), 0, NULL, wxCB_DROPDOWN);
  cob_chap_language->Append("");
  for (i = 0; i < sorted_iso_codes.Count(); i++)
    cob_chap_language->Append(sorted_iso_codes[i]);
  cob_chap_language->SetToolTip(_T("mkvmerge supports two chapter formats: "
                                   "The OGM like text format and the full "
                                   "featured XML format. This option "
                                   "specifies the language to be associated "
                                   "with chapters if the OGM chapter format "
                                   "is used. It is ignored for XML chapter "
                                   "files."));
  new wxStaticText(this, -1, _("Charset:"), wxPoint(270, 265),
                   wxDefaultSize, 0);
  cob_chap_charset =
    new wxComboBox(this, ID_CB_CHAPTERCHARSET, _(""),
                   wxPoint(330, 265 + YOFF),
                   wxSize(150, -1), 0, NULL, wxCB_DROPDOWN);
  cob_chap_charset->Append("");
  for (i = 0; i < sorted_charsets.Count(); i++)
    cob_chap_charset->Append(sorted_charsets[i]);
  cob_chap_charset->SetToolTip(_T("mkvmerge supports two chapter formats: "
                                  "The OGM like text format and the full "
                                  "featured XML format. If the OGM format "
                                  "is used and the file's charset is not "
                                  "recognized correctly then this option "
                                  "can be used to correct that. This option "
                                  "is ignored for XML chapter files."));
  new wxStaticText(this, -1, _("Cue name format:"), wxPoint(15, 290),
                   wxDefaultSize, 0);
  tc_cue_name_format =
    new wxTextCtrl(this, ID_TC_CUENAMEFORMAT, _(""), wxPoint(125, 290 + YOFF),
                   wxSize(135, -1));
  tc_cue_name_format->SetToolTip("mkvmerge can read CUE sheets for audio "
                                 "cds and automatically convert them to "
                                 "chapters. This option controls how the "
                                 "chapter names are created. The sequence "
                                 "'%p' is replaced by the track's PERFORMER, "
                                 "the sequence '%t' by the "
                                 "track's TITLE, '%n' by the track's number "
                                 "and '%N' by the track's number padded with "
                                 "a leading 0 for track numbers < 10. The "
                                 "rest is copied as is. If nothing is "
                                 "entered then '%p - %t' will be used.");

  new wxStaticBox(this, -1, _("Global tags"), wxPoint(10, 325),
                  wxSize(475, 50));
  new wxStaticText(this, -1, _("Tag file:"), wxPoint(15, 345),
                   wxDefaultSize, 0);
  tc_global_tags =
    new wxTextCtrl(this, ID_TC_GLOBALTAGS, _(""), wxPoint(100, 345 + YOFF),
                   wxSize(290, -1));
  wxButton *b_browse_global_tags =
    new wxButton(this, ID_B_BROWSEGLOBALTAGS, _("Browse"),
                 wxPoint(400, 345 + YOFF), wxDefaultSize, 0);
  b_browse_global_tags->SetToolTip(_T("The difference between tags associated "
                                      "with a track and global tags is "
                                      "explained in mkvmerge's documentation. "
                                      "Most of the time you probably want to "
                                      "use the tags associated with a track "
                                      "on the 'input' tab."));
}

void tab_global::on_browse_global_tags(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, "Choose the tags file", last_open_dir, "",
                   _T("Tag files (*.xml)|*.xml|" ALLFILES), wxOPEN);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    tc_global_tags->SetValue(dlg.GetPath());
  }
}

void tab_global::on_browse_chapters(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, "Choose the chapter file", last_open_dir, "",
                   _T("Chapter files (*.xml;*.txt;*.cue)|*.xml;*.txt;*.cue|"
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

  cfg->SetPath("/global");
  cfg->Read("segment_title", &s);
  tc_title->SetValue(s);

  ec = false;
  er = true;
  cfg->Read("enable_splitting", &ec);
  cb_split->SetValue(ec);
  cfg->Read("split_by_size", &er);
  if (er)
    rb_split_by_size->SetValue(true);
  else
    rb_split_by_time->SetValue(true);
  cfg->Read("split_after_bytes", &s);
  cob_split_by_size->SetValue(s);
  cfg->Read("split_after_time", &s);
  cob_split_by_time->SetValue(s);
  cfg->Read("split_max_files", &s);
  tc_split_max_files->SetValue(s);
  b = false;
  if (cfg->Read("link", &b))
    cb_link->SetValue(b);
  else if (cfg->Read("dont_link", &b))
    cb_link->SetValue(!b);
  else
    cb_link->SetValue(true);

  rb_split_by_size->Enable(ec);
  cob_split_by_size->Enable(ec && er);
  rb_split_by_time->Enable(ec);
  cob_split_by_time->Enable(ec && !er);
  cb_link->Enable(ec);
  tc_split_max_files->Enable(ec);

  cfg->Read("previous_segment_uid", &s);
  tc_previous_segment_uid->SetValue(s);
  cfg->Read("next_segment_uid", &s);
  tc_next_segment_uid->SetValue(s);

  cfg->Read("chapters", &s);
  tc_chapters->SetValue(s);
  cfg->Read("chapter_language", &s);
  cob_chap_language->SetValue(s);
  cfg->Read("chapter_charset", &s);
  cob_chap_charset->SetValue(s);
  cfg->Read("cue_name_format", &s);
  tc_cue_name_format->SetValue(s);

  cfg->Read("global_tags", &s);
  tc_global_tags->SetValue(s);
}

void tab_global::save(wxConfigBase *cfg) {
  cfg->SetPath("/global");
  cfg->Write("segment_title", tc_title->GetValue());

  cfg->Write("enable_splitting", cb_split->IsChecked());
  cfg->Write("split_by_size", rb_split_by_size->GetValue());
  cfg->Write("split_after_bytes", cob_split_by_size->GetValue());
  cfg->Write("split_after_time", cob_split_by_time->GetValue());
  cfg->Write("split_max_files", tc_split_max_files->GetValue());
  cfg->Write("link", cb_link->IsChecked());

  cfg->Write("previous_segment_uid", tc_previous_segment_uid->GetValue());
  cfg->Write("next_segment_uid", tc_next_segment_uid->GetValue());

  cfg->Write("chapters", tc_chapters->GetValue());
  cfg->Write("chapter_language", cob_chap_language->GetValue());
  cfg->Write("chapter_charset", cob_chap_charset->GetValue());
  cfg->Write("cue_name_format", tc_cue_name_format->GetValue());

  cfg->Write("global_tags", tc_global_tags->GetValue());
}

bool tab_global::validate_settings() {
  string s;
  int64_t dummy_i, mod;
  char c;

  if (cb_split->GetValue()) {
    if (rb_split_by_size->GetValue()) {
      s = cob_split_by_size->GetValue();
      strip(s);
      if (s.length() == 0) {
        wxMessageBox(_T("Splitting by size was selected, but no size has "
                        "been given."),
                     _T("mkvmerge GUI error"), wxOK | wxCENTER | wxICON_ERROR);
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
        wxMessageBox(_T("The format of the split size is invalid."),
                     _T("mkvmerge GUI error"), wxOK | wxCENTER | wxICON_ERROR);
        return false;
      }
      if ((s.length() == 0) || !parse_int(s.c_str(), dummy_i)) {
        wxMessageBox(_T("The format of the split size is invalid."),
                     _T("mkvmerge GUI error"), wxOK | wxCENTER | wxICON_ERROR);
        return false;
      }
      if ((dummy_i * mod) < 1024 * 1024) {
        wxMessageBox(_T("The format of the split size is invalid (size too "
                        "small)."),
                     _T("mkvmerge GUI error"), wxOK | wxCENTER | wxICON_ERROR);
        return false;
      }

    } else {
      s = cob_split_by_time->GetValue();
      strip(s);
      if (s.length() == 0) {
        wxMessageBox(_T("Splitting by time was selected, but no time has "
                        "been given."),
                     _T("mkvmerge GUI error"), wxOK | wxCENTER | wxICON_ERROR);
        return false;
      }
      c = s[s.length() - 1];
      if (tolower(c) == 's') {
        s.erase(s.length() - 1);
        if ((s.length() == 0) || !parse_int(s.c_str(), dummy_i) ||
            (dummy_i <= 0)) {
          wxMessageBox(_T("The format of the split time is invalid."),
                       _T("mkvmerge GUI error"), wxOK | wxCENTER |
                       wxICON_ERROR);
          return false;
        }

      } else if ((s.length() != 8) || (s[2] != ':') || (s[5] != ':') ||
                 !isdigit(s[0]) || !isdigit(s[1]) || !isdigit(s[3]) ||
                 !isdigit(s[4]) || !isdigit(s[6]) || !isdigit(s[7])) {
        wxMessageBox(_T("The format of the split time is invalid."),
                     _T("mkvmerge GUI error"), wxOK | wxCENTER | wxICON_ERROR);
        return false;
      }
    }

    s = tc_split_max_files->GetValue();
    strip(s);
    if ((s.length() > 0) && (!parse_int(s.c_str(), dummy_i) ||
                             (dummy_i <= 1))) {
      wxMessageBox(_T("Invalid number of max. split files given."),
                   _T("mkvmerge GUI error"), wxOK | wxCENTER | wxICON_ERROR);
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
