/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   "global" tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <ctype.h>

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
  wxStaticBoxSizer *siz_fs_title, *siz_split, *siz_linking_box, *siz_chapters;
  wxStaticBoxSizer *siz_gl_tags;
  wxFlexGridSizer *siz_linking, *siz_chap_l1_l2;
  wxBoxSizer *siz_all, *siz_line;
  wxBoxSizer *siz_chap_l1, *siz_chap_l2, *siz_chap_l3;
  wxButton *b_browse_chapters, *b_browse_global_tags;
  uint32_t i;

  siz_fs_title =
    new wxStaticBoxSizer(new wxStaticBox(this, -1, wxT("File/segment title")),
                         wxHORIZONTAL);
  siz_fs_title->Add(new wxStaticText(this, -1, wxT("File/segment title:")),
                    0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
  tc_title = new wxTextCtrl(this, ID_TC_SEGMENTTITLE, wxT(""));
  tc_title->SetToolTip(TIP("This is the title that players may show "
                           "as the 'main title' for this movie."));
  siz_fs_title->Add(tc_title, 1, wxALIGN_CENTER_VERTICAL | wxGROW |
                    wxLEFT | wxRIGHT, 5);

  siz_split =
    new wxStaticBoxSizer(new wxStaticBox(this, -1, wxT("Split")),
                         wxVERTICAL);
  siz_line = new wxBoxSizer(wxHORIZONTAL);
  cb_split = new wxCheckBox(this, ID_CB_SPLIT, wxT("Enable splitting"));
  cb_split->SetToolTip(TIP("Enables splitting of the output into more than "
                           "one file. You can split after a given size "
                           "or after a given amount of time has passed."));
  siz_line->Add(cb_split, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

  cb_link = new wxCheckBox(this, ID_CB_LINK, wxT("link files"));
  cb_link->SetToolTip(TIP("Use 'segment linking' for the resulting "
                          "files. For an in-depth explanation of this "
                          "feature consult the mkvmerge documentation."));
  cb_link->SetValue(false);
  cb_link->Enable(false);
  siz_line->Add(cb_link, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  siz_line->Add(1, 15, 0, 0, 0);

  st_split_max_files = new wxStaticText(this, wxID_STATIC,
                                  wxT("max. number of files:"));
  st_split_max_files->Enable(false);
  siz_line->Add(st_split_max_files, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  tc_split_max_files = new wxTextCtrl(this, ID_TC_SPLITMAXFILES, wxT(""));
  tc_split_max_files->SetToolTip(TIP("The maximum number of files that will "
                                     "be created even if the last file might "
                                     "contain more bytes/time than wanted. "
                                     "Useful e.g. when you want exactly two "
                                     "files."));
  tc_split_max_files->Enable(false);
  siz_line->Add(tc_split_max_files, 0, wxALIGN_CENTER_VERTICAL, 0);

  siz_split->Add(siz_line, 0, wxGROW | wxLEFT | wxRIGHT, 5);

  siz_split->Add(0, 5, 0, 0, 0);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  st_split = new wxStaticText(this, -1, wxT("Split..."));
  st_split->Enable(false);
  siz_line->Add(st_split, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);

  rb_split_by_size =
    new wxRadioButton(this, ID_RB_SPLITBYSIZE, wxT("by size:"),
                      wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
  rb_split_by_size->Enable(false);
//   rb_split_by_size->SetSizeHints(0, -1);
  siz_line->Add(rb_split_by_size, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  cob_split_by_size =
    new wxComboBox(this, ID_CB_SPLITBYSIZE, wxT(""), wxDefaultPosition,
                   wxSize(100, -1), 0, NULL, wxCB_DROPDOWN);
  cob_split_by_size->Append(wxT(""));
  cob_split_by_size->Append(wxT("350M"));
  cob_split_by_size->Append(wxT("650M"));
  cob_split_by_size->Append(wxT("700M"));
  cob_split_by_size->Append(wxT("703M"));
  cob_split_by_size->Append(wxT("800M"));
  cob_split_by_size->Append(wxT("1000M"));
  cob_split_by_size->SetToolTip(TIP("The size after which a new output file "
                                    "is being started. The letters 'G', 'M' "
                                    "and 'K' can be used to indicate giga/"
                                    "mega/kilo bytes respectively. All units "
                                    "are based on 1024 (G = 1024^3, M = "
                                    "1024^2, K = 1024)."));
  cob_split_by_size->Enable(false);
  siz_line->Add(cob_split_by_size, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT |
                wxGROW, 10);

  rb_split_by_time = new wxRadioButton(this, ID_RB_SPLITBYTIME,
                                       wxT("by time:"));
  rb_split_by_time->Enable(false);
  siz_line->Add(rb_split_by_time, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  cob_split_by_time =
    new wxComboBox(this, ID_CB_SPLITBYTIME, wxT(""), wxDefaultPosition,
                   wxSize(100, -1), 0, NULL, wxCB_DROPDOWN);
  cob_split_by_time->Append(wxT(""));
  cob_split_by_time->Append(wxT("01:00:00"));
  cob_split_by_time->Append(wxT("1800s"));
  cob_split_by_time->SetToolTip(TIP("The time after which a new output file "
                                    "is being started. The time can be given "
                                    "either in the form HH:MM:SS or as the "
                                    "number of seconds followed by 's'. "
                                    "Examples: 01:00:00 (after one hour) or "
                                    "1800s (after 1800 seconds)."));
  cob_split_by_time->Enable(false);
//   cob_split_by_time->SetSizeHints(0, -1);
  siz_line->Add(cob_split_by_time, 1, wxALIGN_CENTER_VERTICAL | wxGROW, 0);

//   siz_line2 = new wxBoxSizer(wxHORIZONTAL);
//   rb_split_chapters =
//     new wxRadioButton(this, ID_RB_SPLITAFTERCHAPTERS, wxT("after chapters"));
//   rb_split_chapters->Enable(false);
//   siz_line2->Add(rb_split_chapters, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);

//   tc_split_chapters = new wxTextCtrl(this, ID_TC_SPLITAFTERCHAPTERS, wxT(""));
//   tc_split_chapters->Enable(false);
//   siz_line2->Add(tc_split_chapters, 0, wxALIGN_CENTER_VERTICAL, 0);

//   siz_fg->Add(siz_line2, 0, wxRIGHT, 10);

//   rb_split_each_chapter =
//     new wxRadioButton(this, ID_RB_SPLITAFTEREACHCHAPTER,
//                       wxT("after each chapter"));
//   rb_split_each_chapter->Enable(false);
//   siz_fg->Add(rb_split_each_chapter, 0, 0, 0);

  siz_split->Add(siz_line, 0, wxLEFT | wxRIGHT | wxGROW, 5);

  siz_linking_box =
    new wxStaticBoxSizer(new wxStaticBox(this, -1,
                                         wxT("File/segment linking")),
                         wxVERTICAL);
  siz_linking = new wxFlexGridSizer(2, 2);
  siz_linking->AddGrowableCol(1);
  siz_linking->Add(new wxStaticText(this, -1, wxT("Previous segment UID:")),
                   0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
  tc_previous_segment_uid = new wxTextCtrl(this, ID_TC_PREVIOUSSEGMENTUID,
                                           wxT(""));
  tc_previous_segment_uid->SetToolTip(TIP("For an in-depth explanantion of "
                                          "file/segment linking and this "
                                          "feature please read mkvmerge's "
                                          "documentation."));
  siz_linking->Add(tc_previous_segment_uid, 1, wxGROW |
                   wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);

  siz_linking->Add(new wxStaticText(this, -1, wxT("Next segment UID:")),
                   0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
  tc_next_segment_uid = new wxTextCtrl(this, ID_TC_PREVIOUSSEGMENTUID,
                                       wxT(""));
  tc_next_segment_uid->SetToolTip(TIP("For an in-depth explanantion of "
                                      "file/segment linking and this "
                                      "feature please read mkvmerge's "
                                      "documentation."));
  siz_linking->Add(tc_next_segment_uid, 1, wxGROW | wxALIGN_CENTER_VERTICAL |
                   wxLEFT | wxRIGHT, 5);
  siz_linking_box->Add(siz_linking, 1, wxGROW, 0);

  siz_chapters =
    new wxStaticBoxSizer(new wxStaticBox(this, -1, wxT("Chapters")),
                         wxVERTICAL);
  siz_chap_l1_l2 = new wxFlexGridSizer(2, 2);
  siz_chap_l1_l2->AddGrowableCol(1);
  siz_chap_l1_l2->Add(new wxStaticText(this, -1, wxT("Chapter file:")),
                      0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
  siz_chap_l1 = new wxBoxSizer(wxHORIZONTAL);
  tc_chapters = new wxTextCtrl(this, ID_TC_CHAPTERS, wxT(""));
  tc_chapters->SetToolTip(TIP("mkvmerge supports two chapter formats: The "
                              "OGM like text format and the full featured "
                              "XML format."));
  siz_chap_l1->Add(tc_chapters, 1, wxALIGN_CENTER_VERTICAL | wxGROW |
                   wxRIGHT, 5);
  b_browse_chapters = new wxButton(this, ID_B_BROWSECHAPTERS, wxT("Browse"));
  b_browse_chapters->SetToolTip(TIP("mkvmerge supports two chapter formats: "
                                    "The OGM like text format and the full "
                                    "featured XML format."));
  siz_chap_l1->Add(b_browse_chapters, 0, wxALIGN_CENTER_VERTICAL | wxLEFT |
                   wxRIGHT, 5);
  siz_chap_l1_l2->Add(siz_chap_l1, 1, wxGROW, 0);

  siz_chap_l1_l2->Add(new wxStaticText(this, -1, wxT("Language:")),
                      0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
  cob_chap_language =
    new wxComboBox(this, ID_CB_CHAPTERLANGUAGE, wxT(""),
                   wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN);
  cob_chap_language->Append(wxT(""));
  for (i = 0; i < sorted_iso_codes.Count(); i++)
    cob_chap_language->Append(sorted_iso_codes[i]);
  cob_chap_language->SetToolTip(TIP("mkvmerge supports two chapter formats: "
                                    "The OGM like text format and the full "
                                    "featured XML format. This option "
                                    "specifies the language to be associated "
                                    "with chapters if the OGM chapter format "
                                    "is used. It is ignored for XML chapter "
                                    "files."));
  siz_chap_l2 = new wxBoxSizer(wxHORIZONTAL);
  siz_chap_l2->Add(cob_chap_language, 1, wxALIGN_CENTER_VERTICAL | wxGROW |
                   wxRIGHT, 5);

  siz_chap_l2->Add(new wxStaticText(this, -1, wxT("Charset:")),
                   0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 10);

  cob_chap_charset =
    new wxComboBox(this, ID_CB_CHAPTERCHARSET, wxT(""), wxDefaultPosition,
                   wxDefaultSize, 0, NULL, wxCB_DROPDOWN);
  cob_chap_charset->Append(wxT(""));
  for (i = 0; i < sorted_charsets.Count(); i++)
    cob_chap_charset->Append(sorted_charsets[i]);
  cob_chap_charset->SetToolTip(TIP("mkvmerge supports two chapter formats: "
                                   "The OGM like text format and the full "
                                   "featured XML format. If the OGM format "
                                   "is used and the file's charset is not "
                                   "recognized correctly then this option "
                                   "can be used to correct that. This option "
                                   "is ignored for XML chapter files."));
  siz_chap_l2->Add(cob_chap_charset, 1, wxALIGN_CENTER_VERTICAL | wxGROW |
                   wxRIGHT, 5);
  siz_chap_l1_l2->Add(siz_chap_l2, 1, wxGROW | wxTOP | wxBOTTOM, 2);
  siz_chapters->Add(siz_chap_l1_l2, 0, wxGROW, 0);

  siz_chap_l3 = new wxBoxSizer(wxHORIZONTAL);
  siz_chap_l3->Add(new wxStaticText(this, -1, wxT("Cue name format:")),
                   0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
  tc_cue_name_format = new wxTextCtrl(this, ID_TC_CUENAMEFORMAT, wxT(""),
                                      wxDefaultPosition, wxSize(150, -1));
  tc_cue_name_format->SetToolTip(TIP("mkvmerge can read CUE sheets for audio "
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
  siz_chap_l3->Add(tc_cue_name_format, 0, wxALIGN_CENTER_VERTICAL | wxLEFT |
                   wxRIGHT, 5);
  siz_chapters->Add(siz_chap_l3, 0, wxTOP, 2);

  siz_gl_tags =
    new wxStaticBoxSizer(new wxStaticBox(this, -1, wxT("Global tags")),
                         wxHORIZONTAL);
  siz_gl_tags->Add(new wxStaticText(this, -1, wxT("Tag file:")),
                   0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);

  tc_global_tags = new wxTextCtrl(this, ID_TC_GLOBALTAGS, wxT(""));
  siz_gl_tags->Add(0, 5, 0, 0, 0);
  siz_gl_tags->Add(tc_global_tags, 1, wxALIGN_CENTER_VERTICAL | wxGROW |
                   wxTOP | wxBOTTOM, 2);
  siz_gl_tags->Add(0, 5, 0, 0, 0);
  b_browse_global_tags = new wxButton(this, ID_B_BROWSEGLOBALTAGS,
                                      wxT("Browse"));
  b_browse_global_tags->SetToolTip(TIP("The difference between tags associated"
                                       " with a track and global tags is "
                                       "explained in mkvmerge's documentation."
                                       " In short: global tags apply to the "
                                       "complete file while the tags you can "
                                       "add on the 'input' tab apply to only "
                                       "one track."));
  siz_gl_tags->Add(b_browse_global_tags, 0, wxALIGN_CENTER_VERTICAL | wxLEFT |
                   wxRIGHT, 5);

  siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->Add(siz_fs_title, 0, wxGROW | wxALL, 5);
  siz_all->Add(siz_split, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);
  siz_all->Add(siz_linking_box, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);
  siz_all->Add(siz_chapters, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);
  siz_all->Add(siz_gl_tags, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);
  SetSizer(siz_all);
}

void
tab_global::on_browse_global_tags(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, wxT("Choose the tags file"), last_open_dir, wxT(""),
                   wxT("Tag files (*.xml)|*.xml|" ALLFILES), wxOPEN);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    tc_global_tags->SetValue(dlg.GetPath());
  }
}

void
tab_global::on_browse_chapters(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, wxT("Choose the chapter file"), last_open_dir,
                   wxT(""),
                   wxT("Chapter files (*.xml;*.txt;*.cue)|*.xml;*.txt;*.cue|"
                       ALLFILES), wxOPEN);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    tc_chapters->SetValue(dlg.GetPath());
  }
}

void
tab_global::on_split_clicked(wxCommandEvent &evt) {
  bool ec, er;

  ec = cb_split->IsChecked();
  er = rb_split_by_size->GetValue();
  st_split_max_files->Enable(ec);
  st_split->Enable(ec);
  rb_split_by_size->Enable(ec);
  cob_split_by_size->Enable(ec && er);
  rb_split_by_time->Enable(ec);
  cob_split_by_time->Enable(ec && !er);
  cb_link->Enable(ec);
  tc_split_max_files->Enable(ec);
}

void
tab_global::on_splitby_time_clicked(wxCommandEvent &evt) {
  cob_split_by_size->Enable(false);
  cob_split_by_time->Enable(true);
  rb_split_by_size->SetValue(false);
  rb_split_by_time->SetValue(true);
}

void
tab_global::on_splitby_size_clicked(wxCommandEvent &evt) {
  cob_split_by_size->Enable(true);
  cob_split_by_time->Enable(false);
  rb_split_by_size->SetValue(true);
  rb_split_by_time->SetValue(false);
}

void
tab_global::load(wxConfigBase *cfg) {
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

  // Compatibility with older mmg versions. Those contained a checkbox
  // labeled "don't link" and the corresponding option.
  b = false;
  if (cfg->Read(wxT("link"), &b))
    cb_link->SetValue(b);
  else if (cfg->Read(wxT("dont_link"), &b))
    cb_link->SetValue(!b);
  else
    cb_link->SetValue(false);

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

  cfg->Read(wxT("title_was_present"), &title_was_present, false);
}

void
tab_global::save(wxConfigBase *cfg) {
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
      if ((s.length() == 0) || !parse_int(s, dummy_i)) {
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
        if ((s.length() == 0) || !parse_int(s, dummy_i) ||
            (dummy_i <= 0)) {
          wxMessageBox(wxT("The format of the split time is invalid."),
                       wxT("mkvmerge GUI error"), wxOK | wxCENTER |
                       wxICON_ERROR);
          return false;
        }

      } else if (((s.length() == 8) &&
                  ((s[2] != ':') || (s[5] != ':') ||
                   !isdigit(s[0]) || !isdigit(s[1]) || !isdigit(s[3]) ||
                   !isdigit(s[4]) || !isdigit(s[6]) || !isdigit(s[7])))
                 ||
                 ((s.length() == 12) &&
                  ((s[2] != ':') || (s[5] != ':') || (s[8] != '.') ||
                   !isdigit(s[0]) || !isdigit(s[1]) || !isdigit(s[3]) ||
                   !isdigit(s[4]) || !isdigit(s[6]) || !isdigit(s[7]) ||
                   !isdigit(s[9]) || !isdigit(s[10]) || !isdigit(s[11])))
                 ||
                 ((s.length() != 8) && (s.length() != 12))) {
        wxMessageBox(wxT("The format of the split time is invalid."),
                     wxT("mkvmerge GUI error"),
                     wxOK | wxCENTER | wxICON_ERROR);
        return false;
      }
    }

    s = wxMB(tc_split_max_files->GetValue());
    strip(s);
    if ((s.length() > 0) && (!parse_int(s, dummy_i) ||
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
  EVT_RADIOBUTTON(ID_RB_SPLITBYSIZE, tab_global::on_splitby_size_clicked)
  EVT_RADIOBUTTON(ID_RB_SPLITBYTIME, tab_global::on_splitby_time_clicked)
END_EVENT_TABLE();
