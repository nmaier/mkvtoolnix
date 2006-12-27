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
  wxBoxSizer *siz_all, *siz_line, *siz_line2;
  wxBoxSizer *siz_chap_l1, *siz_chap_l2, *siz_chap_l3, *siz_col;
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
    new wxStaticBoxSizer(new wxStaticBox(this, -1, wxT("Splitting")),
                         wxVERTICAL);
  cb_split = new wxCheckBox(this, ID_CB_SPLIT, wxT("Enable splitting..."));
  cb_split->SetToolTip(TIP("Enables splitting of the output into more than "
                           "one file. You can split after a given size, "
                           "after a given amount of time has passed in each "
                           "file or after a list of timecodes."));
  siz_split->Add(cb_split, 0, wxLEFT | wxTOP | wxBOTTOM, 5);



  siz_line = new wxBoxSizer(wxHORIZONTAL);

  rb_split_by_size =
    new wxRadioButton(this, ID_RB_SPLITBYSIZE, wxT("...after this size:"),
                      wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
  rb_split_by_size->Enable(false);
//   rb_split_by_size->SetSizeHints(0, -1);
  siz_line2 = new wxBoxSizer(wxHORIZONTAL);
  siz_line2->Add(rb_split_by_size, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
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
                                    "is started. The letters 'G', 'M' "
                                    "and 'K' can be used to indicate giga/"
                                    "mega/kilo bytes respectively. All units "
                                    "are based on 1024 (G = 1024^3, M = "
                                    "1024^2, K = 1024)."));
  cob_split_by_size->Enable(false);
  siz_line2->Add(cob_split_by_size, 1, wxALIGN_CENTER_VERTICAL | wxRIGHT |
                wxGROW, 10);

  rb_split_by_time = new wxRadioButton(this, ID_RB_SPLITBYTIME,
                                       wxT("...after this duration:"));
  rb_split_by_time->Enable(false);
  siz_line2->Add(rb_split_by_time, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  cob_split_by_time =
    new wxComboBox(this, ID_CB_SPLITBYTIME, wxT(""), wxDefaultPosition,
                   wxSize(100, -1), 0, NULL, wxCB_DROPDOWN);
  cob_split_by_time->Append(wxT(""));
  cob_split_by_time->Append(wxT("01:00:00"));
  cob_split_by_time->Append(wxT("1800s"));
  cob_split_by_time->SetToolTip(TIP("The duration after which a new output "
                                    "file is started. The time can be given "
                                    "either in the form HH:MM:SS.nnnnnnnnn "
                                    "or as the number of seconds followed by "
                                    "'s'. You may omit the number of hours "
                                    "'HH' and the number of nanoseconds "
                                    "'nnnnnnnnn'. If given then you may use "
                                    "up to nine digits after the decimal "
                                    "point. "
                                    "Examples: 01:00:00 (after one hour) or "
                                    "1800s (after 1800 seconds)."));
  cob_split_by_time->Enable(false);
  siz_line2->Add(cob_split_by_time, 1, wxALIGN_CENTER_VERTICAL | wxGROW, 0);

  siz_col = new wxBoxSizer(wxVERTICAL);
  siz_col->Add(siz_line2, 0, wxGROW, 0);

  siz_line2 = new wxBoxSizer(wxHORIZONTAL);
  rb_split_after_timecodes =
    new wxRadioButton(this, ID_RB_SPLITAFTERTIMECODES,
                      wxT("...after timecodes:"));
  rb_split_after_timecodes->Enable(false);
  siz_line2->Add(rb_split_after_timecodes, 0, wxALIGN_CENTER_VERTICAL |
                 wxRIGHT, 5);

  tc_split_after_timecodes = new wxTextCtrl(this, ID_TC_SPLITAFTERTIMECODES,
                                      wxT(""));
  tc_split_after_timecodes->
    SetToolTip(TIP("The timecodes after which a new output "
                   "file is started. The timecodes can be given "
                   "either in the form HH:MM:SS.nnnnnnnnn "
                   "or as the number of seconds followed by "
                   "'s'. You may omit the number of hours "
                   "'HH' and the number of nanoseconds "
                   "'nnnnnnnnn'. If given then you may use "
                   "up to nine digits after the decimal "
                   "point. If two or more "
                   "timecodes are used then you have to separate them with "
                   "commas. The formats can be mixed, too. "
                   "Examples: 01:00:00,01:30:00 (after one hour and after "
                   "one hour and thirty minutes) or "
                   "1800s,3000s,00:10:00 (after three, five and ten "
                   "minutes)."));
  tc_split_after_timecodes->Enable(false);
  siz_line2->Add(tc_split_after_timecodes, 1,
                 wxALIGN_CENTER_VERTICAL | wxGROW, 5);

  siz_col->Add(siz_line2, 0, wxBOTTOM | wxTOP | wxGROW, 5);
  siz_line->Add(siz_col, 1, wxALIGN_TOP | wxGROW | wxLEFT, 10);

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



  siz_line = new wxBoxSizer(wxHORIZONTAL);
  cb_link = new wxCheckBox(this, ID_CB_LINK, wxT("link files"));
  cb_link->SetToolTip(TIP("Use 'segment linking' for the resulting "
                          "files. For an in-depth explanation of this "
                          "feature consult the mkvmerge documentation."));
  cb_link->SetValue(false);
  cb_link->Enable(false);
  siz_line->Add(cb_link, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 10);
  siz_line->Add(0, 1, 1, wxGROW, 0);

  st_split_max_files = new wxStaticText(this, wxID_STATIC,
                                  wxT("max. number of files:"));
  st_split_max_files->Enable(false);
  siz_line->Add(st_split_max_files, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  tc_split_max_files = new wxTextCtrl(this, ID_TC_SPLITMAXFILES, wxT(""));
  tc_split_max_files->SetToolTip(TIP("The maximum number of files that will "
                                     "be created even if the last file might "
                                     "contain more bytes/time than wanted. "
                                     "Useful e.g. when you want exactly two "
                                     "files. If you leave this empty then "
                                     "there is no limit for the number of "
                                     "files mkvmerge might create."));
  tc_split_max_files->Enable(false);
  siz_line->Add(tc_split_max_files, 0, wxALIGN_CENTER_VERTICAL, 0);

  siz_split->Add(siz_line, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxGROW, 5);

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
  bool ec, es, et;

  ec = cb_split->IsChecked();
  es = rb_split_by_size->GetValue();
  et = rb_split_by_time->GetValue();

  st_split_max_files->Enable(ec);
  cb_link->Enable(ec);
  tc_split_max_files->Enable(ec);

  rb_split_by_size->Enable(ec);
  cob_split_by_size->Enable(ec && es);

  rb_split_by_time->Enable(ec);
  cob_split_by_time->Enable(ec && et);

  rb_split_after_timecodes->Enable(ec);
  tc_split_after_timecodes->Enable(ec && !es && !et);
}

void
tab_global::on_splitby_time_clicked(wxCommandEvent &evt) {
  cob_split_by_size->Enable(false);
  cob_split_by_time->Enable(true);
  tc_split_after_timecodes->Enable(false);

  rb_split_by_size->SetValue(false);
  rb_split_by_time->SetValue(true);
  rb_split_after_timecodes->SetValue(false);
}

void
tab_global::on_splitby_size_clicked(wxCommandEvent &evt) {
  cob_split_by_size->Enable(true);
  cob_split_by_time->Enable(false);
  tc_split_after_timecodes->Enable(false);

  rb_split_by_size->SetValue(true);
  rb_split_by_time->SetValue(false);
  rb_split_after_timecodes->SetValue(false);
}

void
tab_global::on_splitafter_timecodes_clicked(wxCommandEvent &evt) {
  cob_split_by_size->Enable(false);
  cob_split_by_time->Enable(false);
  tc_split_after_timecodes->Enable(true);

  rb_split_by_size->SetValue(false);
  rb_split_by_time->SetValue(false);
  rb_split_after_timecodes->SetValue(true);
}

void
tab_global::load(wxConfigBase *cfg,
                 int version) {
  wxString s;
  bool b, ec, er;

  cfg->SetPath(wxT("/global"));
  cfg->Read(wxT("segment_title"), &s, wxT(""));
  tc_title->SetValue(s);

  cfg->Read(wxT("enable_splitting"), &ec, false);
  cb_split->SetValue(ec);

  rb_split_by_size->Enable(ec);
  cob_split_by_size->Enable(false);
  rb_split_by_time->Enable(ec);
  cob_split_by_time->Enable(false);
  rb_split_after_timecodes->Enable(ec);
  tc_split_after_timecodes->Enable(false);

  cb_link->Enable(ec);
  st_split_max_files->Enable(ec);
  tc_split_max_files->Enable(ec);

  if (1 == version) {
    // Version 1 only know two split methods: by time and by size.
    // It stored that in the boolean "split_by_size".
    cfg->Read(wxT("split_by_size"), &er, true);
    rb_split_by_size->SetValue(er);
    cob_split_by_size->Enable(ec && er);
    rb_split_by_time->SetValue(!er);
    cob_split_by_time->Enable(ec && !er);
  } else {
    wxString split_mode;

    rb_split_by_time->SetValue(false);
    rb_split_by_size->SetValue(false);
    cfg->Read(wxT("split_mode"), &split_mode, wxT("size"));
    if (split_mode == wxT("duration")) {
      rb_split_by_time->SetValue(true);
      cob_split_by_time->Enable(true);
    } else if (split_mode == wxT("timecodes")) {
      rb_split_after_timecodes->SetValue(true);
      tc_split_after_timecodes->Enable(true);
    } else {
      rb_split_by_size->SetValue(true);
      cob_split_by_size->Enable(true);
    }
  }
  cfg->Read(wxT("split_after_bytes"), &s);
  set_combobox_selection(cob_split_by_size, s);
  cfg->Read(wxT("split_after_time"), &s);
  set_combobox_selection(cob_split_by_time, s);
  cfg->Read(wxT("split_after_timecodes"), &s);
  tc_split_after_timecodes->SetValue(s);
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

  cfg->Read(wxT("previous_segment_uid"), &s);
  tc_previous_segment_uid->SetValue(s);
  cfg->Read(wxT("next_segment_uid"), &s);
  tc_next_segment_uid->SetValue(s);

  cfg->Read(wxT("chapters"), &s);
  tc_chapters->SetValue(s);
  cfg->Read(wxT("chapter_language"), &s);
  set_combobox_selection(cob_chap_language, s);
  cfg->Read(wxT("chapter_charset"), &s);
  set_combobox_selection(cob_chap_charset, s);
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
  if (rb_split_by_size->GetValue())
    cfg->Write(wxT("split_mode"), wxT("size"));
  else if (rb_split_by_time->GetValue())
    cfg->Write(wxT("split_mode"), wxT("duration"));
  else
    cfg->Write(wxT("split_mode"), wxT("timecodes"));
  cfg->Write(wxT("split_after_bytes"), cob_split_by_size->GetValue());
  cfg->Write(wxT("split_after_time"), cob_split_by_time->GetValue());
  cfg->Write(wxT("split_after_timecodes"),
             tc_split_after_timecodes->GetValue());
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
tab_global::is_valid_split_size() {
  int64_t dummy_i, mod;
  char c;
  string s = wxMB(cob_split_by_size->GetValue());

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

  return true;
}

bool
tab_global::is_valid_split_timecode(string s,
                                    const wxString &type) {
  int64_t dummy_i;
  char c;

  strip(s);
  if (s.length() == 0) {
    wxMessageBox(wxT("Splitting by ") + type +
                 wxT(" was selected, but nothing was entered."),
                 wxT("mkvmerge GUI error"),
                 wxOK | wxCENTER | wxICON_ERROR);
    return false;
  }
  c = s[s.length() - 1];
  if (tolower(c) == 's') {
    s.erase(s.length() - 1);
    if ((s.length() == 0) || !parse_int(s, dummy_i) ||
        (dummy_i <= 0)) {
      wxMessageBox(wxT("The format of the split ") + type +
                   wxT(" is invalid."),
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
    wxMessageBox(wxT("The format of the split ") + type + wxT(" is invalid."),
                 wxT("mkvmerge GUI error"),
                 wxOK | wxCENTER | wxICON_ERROR);
    return false;
  }

  return true;
}

bool
tab_global::is_valid_split_timecode_list() {
  string s = wxMB(tc_split_after_timecodes->GetValue());
  vector<string> parts;
  vector<string>::const_iterator i;

  parts = split(s, ",");
  mxforeach(i, parts)
    if (!is_valid_split_timecode(*i, wxT("timecode list")))
      return false;

  return true;
}

bool
tab_global::validate_settings() {
  int64_t dummy_i;
  string s;

  if (cb_split->GetValue()) {
    if (rb_split_by_size->GetValue()) {
      if (!is_valid_split_size())
        return false;

    } else if (rb_split_by_time->GetValue()) {
      if (!is_valid_split_timecode(wxMB(cob_split_by_time->GetValue()),
                                   wxT("duration")))
        return false;

    } else if (!is_valid_split_timecode_list())
      return false;

    s = wxMB(tc_split_max_files->GetValue());
    strip(s);
    if ((s.length() > 0) &&
        (!parse_int(s, dummy_i) || (dummy_i <= 1))) {
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
  EVT_RADIOBUTTON(ID_RB_SPLITAFTERTIMECODES,
                  tab_global::on_splitafter_timecodes_clicked)
END_EVENT_TABLE();
