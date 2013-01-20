/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "global" tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/wx.h>
#include <wx/notebook.h>
#include <wx/listctrl.h>
#include <wx/statline.h>
#include <wx/config.h>
#include <wx/regex.h>
#include <wx/tooltip.h>

#include "common/strings/editing.h"
#include "common/strings/parsing.h"
#include "mmg/mmg.h"
#include "mmg/mmg_dialog.h"
#include "mmg/tabs/global.h"

tab_global::tab_global(wxWindow *parent):
  wxPanel(parent, -1, wxDefaultPosition, wxSize(100, 500), wxTAB_TRAVERSAL) {
  wxStaticBoxSizer *siz_fs_title, *siz_split, *siz_linking_box, *siz_chapters;
  wxFlexGridSizer *siz_linking, *siz_chap_l1_l2, *siz_fg;
  wxBoxSizer *siz_all, *siz_line;
  wxBoxSizer *siz_chap_l1, *siz_chap_l2, *siz_chap_l3;
  uint32_t i;

  sb_file_segment_title = new wxStaticBox(this,  -1, wxEmptyString);
  st_file_segment_title = new wxStaticText(this, -1, wxEmptyString);
  tc_title              = new wxTextCtrl(this, ID_TC_SEGMENTTITLE, wxEmptyString);

  st_tag_file           = new wxStaticText(this, -1, wxEmptyString);
  tc_global_tags        = new wxTextCtrl(this, ID_TC_GLOBALTAGS, wxEmptyString);
  b_browse_global_tags  = new wxButton(this, ID_B_BROWSEGLOBALTAGS);

  st_segmentinfo_file   = new wxStaticText(this, -1, wxEmptyString);
  tc_segmentinfo        = new wxTextCtrl(this, ID_TC_SEGMENTINFO, wxEmptyString);
  b_browse_segmentinfo  = new wxButton(this, ID_B_BROWSESEGMENTINFO);

  cb_webm_mode          = new wxCheckBox(this, ID_CB_WEBM_MODE, wxEmptyString);

  siz_fg = new wxFlexGridSizer(3, 2, 2, 5);
  siz_fg->AddGrowableCol(1);

  siz_fg->Add(st_file_segment_title, 0, wxALIGN_CENTER_VERTICAL);
  siz_fg->Add(tc_title,              1, wxALIGN_CENTER_VERTICAL | wxGROW);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_fg->Add(st_tag_file, 0, wxALIGN_CENTER_VERTICAL);
  siz_line->Add(tc_global_tags,       1, wxALIGN_CENTER_VERTICAL | wxGROW);
  siz_line->Add(b_browse_global_tags, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
  siz_fg->Add(siz_line, 1, wxALIGN_CENTER_VERTICAL | wxGROW);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_fg->Add(st_segmentinfo_file, 0, wxALIGN_CENTER_VERTICAL);
  siz_line->Add(tc_segmentinfo,       1, wxALIGN_CENTER_VERTICAL | wxGROW);
  siz_line->Add(b_browse_segmentinfo, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
  siz_fg->Add(siz_line, 1, wxALIGN_CENTER_VERTICAL | wxGROW);

  siz_fs_title = new wxStaticBoxSizer(sb_file_segment_title, wxVERTICAL);
  siz_fs_title->Add(siz_fg,       1, wxLEFT | wxRIGHT | wxGROW, 5);
  siz_fs_title->Add(cb_webm_mode, 0, wxLEFT | wxRIGHT,          5);

  // Beginning of splitting
  sb_split           = new wxStaticBox(this, -1, wxEmptyString);

  st_split_mode      = new wxStaticText(this, -1, wxEmptyString);
  cob_split_mode     = new wxMTX_COMBOBOX_TYPE(this, ID_COB_SPLIT_MODE, wxEmptyString, wxDefaultPosition, wxSize(100, -1), 0, nullptr, wxCB_DROPDOWN | wxCB_READONLY);
  st_split_args      = new wxStaticText(this, -1, wxEmptyString);
  cob_split_args     = new wxMTX_COMBOBOX_TYPE(this, ID_COB_SPLIT_MODE, wxEmptyString, wxDefaultPosition, wxSize(100, -1), 0, nullptr, wxCB_DROPDOWN);
  st_split_max_files = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  tc_split_max_files = new wxTextCtrl(this, ID_TC_SPLITMAXFILES, wxEmptyString);

  cb_link            = new wxCheckBox(this, ID_CB_LINK, wxEmptyString);
  cb_link->SetValue(false);

  siz_fg = new wxFlexGridSizer(3, 2, 2, 5);
  siz_fg->AddGrowableCol(1);

  siz_fg->Add(st_split_mode,  0, wxALIGN_CENTER_VERTICAL);
  siz_fg->Add(cob_split_mode, 1, wxALIGN_CENTER_VERTICAL | wxGROW);

  siz_fg->Add(st_split_args,  0, wxALIGN_CENTER_VERTICAL);
  siz_fg->Add(cob_split_args, 1, wxALIGN_CENTER_VERTICAL | wxGROW);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(tc_split_max_files, 1, wxALIGN_CENTER_VERTICAL | wxGROW);
  siz_line->Add(cb_link,            1, wxALIGN_CENTER_VERTICAL | wxGROW | wxLEFT, 15);

  siz_fg->Add(st_split_max_files, 0, wxALIGN_CENTER_VERTICAL);
  siz_fg->Add(siz_line,           1, wxALIGN_CENTER_VERTICAL | wxGROW);

  siz_split = new wxStaticBoxSizer(sb_split, wxVERTICAL);
  siz_split->Add(siz_fg, 1, wxLEFT | wxRIGHT | wxGROW, 5);

  enable_split_controls();
  // End of splitting

  sb_file_segment_linking = new wxStaticBox(this, -1, wxEmptyString);
  siz_linking_box         = new wxStaticBoxSizer(sb_file_segment_linking, wxVERTICAL);
  siz_linking             = new wxFlexGridSizer(2);
  siz_linking->AddGrowableCol(1);

  st_segment_uid = new wxStaticText(this, -1, wxEmptyString);
  siz_linking->Add(st_segment_uid, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
  tc_segment_uid = new wxTextCtrl(this, ID_TC_SEGMENTUID, wxEmptyString);
  siz_linking->Add(tc_segment_uid, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);

  st_previous_segment_uid = new wxStaticText(this, -1, wxEmptyString);
  siz_linking->Add(st_previous_segment_uid, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
  tc_previous_segment_uid = new wxTextCtrl(this, ID_TC_PREVIOUSSEGMENTUID, wxEmptyString);
  siz_linking->Add(tc_previous_segment_uid, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);

  st_next_segment_uid = new wxStaticText(this, -1, wxEmptyString);
  siz_linking->Add(st_next_segment_uid, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
  tc_next_segment_uid = new wxTextCtrl(this, ID_TC_PREVIOUSSEGMENTUID, wxEmptyString);
  siz_linking->Add(tc_next_segment_uid, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
  siz_linking_box->Add(siz_linking, 1, wxGROW, 0);

  sb_chapters     = new wxStaticBox(this,  -1, wxEmptyString);
  siz_chapters    = new wxStaticBoxSizer(sb_chapters, wxVERTICAL);
  st_chapter_file = new wxStaticText(this, -1, wxEmptyString);
  siz_chap_l1_l2  = new wxFlexGridSizer(2);
  siz_chap_l1_l2->AddGrowableCol(1);
  siz_chap_l1_l2->Add(st_chapter_file, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
  siz_chap_l1 = new wxBoxSizer(wxHORIZONTAL);
  tc_chapters = new wxTextCtrl(this, ID_TC_CHAPTERS, wxEmptyString);
  siz_chap_l1->Add(tc_chapters, 1, wxALIGN_CENTER_VERTICAL | wxGROW | wxRIGHT, 5);
  b_browse_chapters = new wxButton(this, ID_B_BROWSECHAPTERS);
  siz_chap_l1->Add(b_browse_chapters, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
  siz_chap_l1_l2->Add(siz_chap_l1, 1, wxGROW, 0);

  st_language = new wxStaticText(this, -1, wxEmptyString);
  siz_chap_l1_l2->Add(st_language, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
  cob_chap_language = new wxMTX_COMBOBOX_TYPE(this, ID_CB_CHAPTERLANGUAGE, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
  cob_chap_language->SetMinSize(wxSize(50, -1));
  cob_chap_language->Append(wxEmptyString);
  for (i = 0; i < sorted_iso_codes.Count(); i++)
    cob_chap_language->Append(sorted_iso_codes[i]);
  siz_chap_l2 = new wxBoxSizer(wxHORIZONTAL);
  siz_chap_l2->Add(cob_chap_language, 1, wxALIGN_CENTER_VERTICAL | wxGROW | wxRIGHT, 5);

  st_charset = new wxStaticText(this, -1, wxEmptyString);
  siz_chap_l2->Add(st_charset, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 10);

  cob_chap_charset = new wxMTX_COMBOBOX_TYPE(this, ID_CB_CHAPTERCHARSET, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
  cob_chap_charset->SetMinSize(wxSize(50, -1));
  cob_chap_charset->Append(wxEmptyString);
  for (i = 0; i < sorted_charsets.Count(); i++)
    cob_chap_charset->Append(sorted_charsets[i]);
  siz_chap_l2->Add(cob_chap_charset, 1, wxALIGN_CENTER_VERTICAL | wxGROW | wxRIGHT, 5);
  siz_chap_l1_l2->Add(siz_chap_l2, 1, wxGROW | wxTOP | wxBOTTOM, 2);
  siz_chapters->Add(siz_chap_l1_l2, 0, wxGROW, 0);

  siz_chap_l3        = new wxBoxSizer(wxHORIZONTAL);
  st_cue_name_format = new wxStaticText(this, -1, wxEmptyString);
  siz_chap_l3->Add(st_cue_name_format, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
  tc_cue_name_format = new wxTextCtrl(this, ID_TC_CUENAMEFORMAT, wxEmptyString, wxDefaultPosition, wxSize(150, -1));
  siz_chap_l3->Add(tc_cue_name_format, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
  siz_chapters->Add(siz_chap_l3, 0, wxTOP, 2);

  siz_fg = new wxFlexGridSizer(3);
  siz_fg->AddGrowableCol(1);

  siz_fg->Add(st_tag_file,          0, wxALIGN_CENTER_VERTICAL | wxLEFT,           5);
  siz_fg->Add(tc_global_tags,       1, wxALIGN_CENTER_VERTICAL | wxLEFT | wxGROW,  5);
  siz_fg->Add(b_browse_global_tags, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
  siz_fg->Add(st_segmentinfo_file,  0, wxALIGN_CENTER_VERTICAL | wxLEFT,           5);
  siz_fg->Add(tc_segmentinfo,       1, wxALIGN_CENTER_VERTICAL | wxLEFT | wxGROW,  5);
  siz_fg->Add(b_browse_segmentinfo, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);

  siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->Add(siz_fs_title,    0, wxGROW | wxALL,                       5);
  siz_all->Add(siz_split,       0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);
  siz_all->Add(siz_linking_box, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);
  siz_all->Add(siz_chapters,    0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);
  SetSizer(siz_all);
}

void
tab_global::translate_split_args() {
  auto mode  = cob_split_mode->GetSelection();
  auto value = cob_split_args->GetValue();
  wxString new_tool_tip;

  cob_split_args->Clear();

  if (0 == mode)
    st_split_args->SetLabel(Z("Size:"));

  else if (1 == mode) {
    st_split_args->SetLabel(Z("Size:"));
    new_tool_tip = TIP("The size after which a new output file is started. The letters 'G', 'M' and 'K' can be used to indicate giga/mega/kilo bytes respectively. "
                       "All units are based on 1024 (G = 1024^3, M = 1024^2, K = 1024).");
    cob_split_args->Append(wxEmptyString);
    cob_split_args->Append(wxT("350M"));
    cob_split_args->Append(wxT("650M"));
    cob_split_args->Append(wxT("700M"));
    cob_split_args->Append(wxT("703M"));
    cob_split_args->Append(wxT("800M"));
    cob_split_args->Append(wxT("1000M"));
    cob_split_args->Append(wxT("4483M"));
    cob_split_args->Append(wxT("8142M"));

  } else if (2 == mode) {
    st_split_args->SetLabel(Z("Duration:"));
    new_tool_tip = TIP("The duration after which a new output file is started. The time can be given either in the form HH:MM:SS.nnnnnnnnn "
                       "or as the number of seconds followed by 's'. You may omit the number of hours 'HH' and the number of nanoseconds "
                       "'nnnnnnnnn'. If given then you may use up to nine digits after the decimal point. "
                       "Examples: 01:00:00 (after one hour) or 1800s (after 1800 seconds).");

    cob_split_args->Append(wxEmptyString);
    cob_split_args->Append(wxT("01:00:00"));
    cob_split_args->Append(wxT("1800s"));

  } else if (3 == mode) {
    st_split_args->SetLabel(Z("Timecodes:"));
    new_tool_tip = TIP("The timecodes after which a new output file is started. "
                       "The timecodes refer to the whole stream and not to each individual output file. "
                       "The timecodes can be given either in the form HH:MM:SS.nnnnnnnnn or as the number of seconds followed by 's'. "
                       "You may omit the number of hours 'HH'. "
                       "You can specify up to nine digits for the number of nanoseconds 'nnnnnnnnn' or none at all. "
                       "If given then you may use up to nine digits after the decimal point. "
                       "If two or more timecodes are used then you have to separate them with commas. "
                       "The formats can be mixed, too. "
                       "Examples: 01:00:00,01:30:00 (after one hour and after one hour and thirty minutes) or 1800s,3000s,00:10:00 "
                       "(after three, five and ten minutes).");

  } else if (4 == mode) {
    st_split_args->SetLabel(Z("Parts:"));
    std::vector<std::string> help = {
      Y("A comma-separated list of timecode ranges of content to keep."),
      Y("Each range consists of a start and end timecode with a '-' in the middle, e.g. '00:01:15-00:03:20'."),
      Y("If a start timecode is left out then the previous range's end timecode is used, or the start of the file if there was no previous range."),
      Y("The timecodes can be given either in the form HH:MM:SS.nnnnnnnnn or as the number of seconds followed by 's'."),
      Y("If a range's start timecode is prefixed with '+' then its content will be written to the same file as the previous range. Otherwise a new file will be created for this range."),
    };
    new_tool_tip = format_tooltip(wxU(join(" ", help)));

  } else if (5 == mode) {
    st_split_args->SetLabel(Z("Parts:"));
    std::vector<std::string> help = {
      Y("A comma-separated list of frame/field number ranges of content to keep."),
      Y("Each range consists of a start and end frame/field number with a '-' in the middle, e.g. '157-238'."),
      Y("The numbering starts at 1."),
      Y("This mode considers only the first video track that is output."),
      Y("If no video track is output no splitting will occur."),
      Y("The numbers given with this argument are interpreted based on the number of Matroska blocks that are output."),
      Y("A single Matroska block contains either a full frame (for progressive content) or a single field (for interlaced content)."),
      Y("mkvmerge does not distinguish between those two and simply counts the number of blocks."),
      Y("If a start number is left out then the previous range's end number is used, or the start of the file if there was no previous range."),
      Y("If a range's start number is prefixed with '+' then its content will be written to the same file as the previous range. Otherwise a new file will be created for this range."),
    };
    new_tool_tip = format_tooltip(wxU(join(" ", help)));

  } else if (6 == mode) {
    st_split_args->SetLabel(Z("Frames/fields:"));
    std::vector<std::string> help = {
      Y("A comma-separated list of frame/field numbers after which to split."),
      Y("The numbering starts at 1."),
      Y("This mode considers only the first video track that is output."),
      Y("If no video track is output no splitting will occur."),
      Y("The numbers given with this argument are interpreted based on the number of Matroska blocks that are output."),
      Y("A single Matroska block contains either a full frame (for progressive content) or a single field (for interlaced content)."),
      Y("mkvmerge does not distinguish between those two and simply counts the number of blocks."),
    };
    new_tool_tip = format_tooltip(wxU(join(" ", help)));

  } else if (7 == mode) {
    st_split_args->SetLabel(Z("Chapter numbers:"));
    std::vector<std::string> help = {
      Y("Either the word 'all' which selects all chapters or a comma-separated list of chapter numbers after which to split."),
      Y("The numbering starts at 1."),
      Y("Splitting will occur right before the first key frame whose timecode is equal to or bigger than the start timecode for the chapters whose numbers are listed."),
      Y("A chapter starting at 0s is never considered for splitting and discarded silently."),
      Y("This mode only considers the top-most level of chapters across all edition entries."),
    };
    new_tool_tip = format_tooltip(wxU(join(" ", help)));
  }

  auto tool_tip = cob_split_args->GetToolTip();
  if (tool_tip)
    tool_tip->SetTip(new_tool_tip);
  else
    cob_split_args->SetToolTip(new_tool_tip);

  cob_split_args->SetValue(value);
}

void
tab_global::translate_ui() {
  sb_file_segment_title->SetLabel(Z("Global options"));
  st_file_segment_title->SetLabel(Z("File/segment title:"));
  tc_title->SetToolTip(TIP("This is the title that players may show as the 'main title' for this movie."));

  sb_split->SetLabel(Z("Splitting"));
  st_split_mode->SetLabel(Z("Split mode:"));
  st_split_mode->SetToolTip(TIP("Enables splitting of the output into more than one file. You can split after a given size, "
                                "after a given amount of time has passed in each file or after a list of timecodes."));

  auto split_mode = std::max(cob_split_mode->GetSelection(), 0);
  cob_split_mode->Clear();
  cob_split_mode->Append(Z("no splitting"));
  cob_split_mode->Append(Z("split after size"));
  cob_split_mode->Append(Z("split after duration"));
  cob_split_mode->Append(Z("split after timecodes"));
  cob_split_mode->Append(Z("split by parts based on timecodes"));
  cob_split_mode->Append(Z("split by parts based on frame/field numbers"));
  cob_split_mode->Append(Z("split after frame/field numbers"));
  cob_split_mode->Append(Z("split before chapters"));
  cob_split_mode->SetSelection(split_mode);

  cb_link->SetLabel(Z("link files"));
  cb_link->SetToolTip(TIP("Use 'segment linking' for the resulting files. For an in-depth explanation of this feature consult the mkvmerge documentation."));
  st_split_max_files->SetLabel(Z("Max. number of files:"));
  tc_split_max_files->SetToolTip(TIP("The maximum number of files that will be created even if the last file might "
                                     "contain more bytes/time than wanted. Useful e.g. when you want exactly two "
                                     "files. If you leave this empty then there is no limit for the number of files mkvmerge might create."));
  sb_file_segment_linking->SetLabel(Z("File/segment linking"));
  st_segment_uid->SetLabel(Z("Segment UIDs:"));
  tc_segment_uid->SetToolTip(TIP("Sets the segment UIDs to use. "
                                 "This is a comma-separated list of 128bit segment UIDs in the usual UID form: "
                                 "hex numbers with or without the \"0x\" prefix, with or without spaces, exactly 32 digits.\n\n"
                                 "Each file created contains one segment, and each segment has one segment UID. "
                                 "If more segment UIDs are specified than segments are created then the surplus UIDs are ignored. "
                                 "If fewer UIDs are specified than segments are created then random UIDs will be created for them."));
  st_previous_segment_uid->SetLabel(Z("Previous segment UID:"));
  tc_previous_segment_uid->SetToolTip(TIP("For an in-depth explanantion of file/segment linking and this feature please read mkvmerge's documentation."));
  st_next_segment_uid->SetLabel(Z("Next segment UID:"));
  tc_next_segment_uid->SetToolTip(TIP("For an in-depth explanantion of file/segment linking and this feature please read mkvmerge's documentation."));
  sb_chapters->SetLabel(Z("Chapters"));
  st_chapter_file->SetLabel(Z("Chapter file:"));
  tc_chapters->SetToolTip(TIP("mkvmerge supports two chapter formats: The OGM like text format and the full featured XML format."));
  b_browse_chapters->SetLabel(Z("Browse"));
  b_browse_chapters->SetToolTip(TIP("mkvmerge supports two chapter formats: The OGM like text format and the full featured XML format."));
  st_language->SetLabel(Z("Language:"));
  cob_chap_language->SetToolTip(TIP("mkvmerge supports two chapter formats: The OGM like text format and the full featured XML format. "
                                    "This option specifies the language to be associated with chapters if the OGM chapter format is used. "
                                    "It is ignored for XML chapter files."));
  st_charset->SetLabel(Z("Charset:"));
  cob_chap_charset->SetToolTip(TIP("mkvmerge supports two chapter formats: The OGM like text format and the full featured XML format. "
                                   "If the OGM format is used and the file's charset is not recognized correctly then this option "
                                   "can be used to correct that. This option is ignored for XML chapter files."));
  st_cue_name_format->SetLabel(Z("Cue name format:"));
  tc_cue_name_format->SetToolTip(TIP("mkvmerge can read CUE sheets for audio CDs and automatically convert them to chapters. "
                                     "This option controls how the chapter names are created. The sequence '%p' is replaced by the track's "
                                     "PERFORMER, the sequence '%t' by the track's TITLE, '%n' by the track's number and '%N' by the track's number "
                                     "padded with a leading 0 for track numbers < 10. "
                                     "The rest is copied as is. If nothing is entered then '%p - %t' will be used."));
  st_tag_file->SetLabel(Z("Tag file:"));
  b_browse_global_tags->SetLabel(Z("Browse"));
  tc_global_tags->SetToolTip(TIP("The difference between tags associated with a track and global tags is explained in mkvmerge's documentation. "
                                 "In short: global tags apply to the complete file while the tags you can add on the 'input' tab apply to only one track."));
  st_segmentinfo_file->SetLabel(Z("Segment info file:"));
  b_browse_segmentinfo->SetLabel(Z("Browse"));
  tc_segmentinfo->SetToolTip(TIP("The difference between tags associated with a track and global tags is explained in mkvmerge's documentation. "
                                 "In short: global tags apply to the complete file while the tags you can add on the 'input' tab apply to only one track."));
  cb_webm_mode->SetLabel(Z("Create WebM compliant file"));
  cb_webm_mode->SetToolTip(TIP("Create a WebM compliant file. mkvmerge also turns this on if the output file name's extension is \"webm\". This mode "
                               "enforces several restrictions. The only allowed codecs are VP8 video and Vorbis audio tracks. Neither chapters nor tags are "
                               "allowed. The DocType header item is changed to \"webm\"."));

  translate_split_args();
}

void
tab_global::on_browse_global_tags(wxCommandEvent &) {
  wxFileDialog dlg(nullptr, Z("Choose the tags file"), last_open_dir, wxEmptyString, wxString::Format(Z("Tag files (*.xml)|*.xml|%s"), ALLFILES.c_str()), wxFD_OPEN);
  if(dlg.ShowModal() != wxID_OK)
    return;

  last_open_dir = dlg.GetDirectory();
  tc_global_tags->SetValue(dlg.GetPath());
}

void
tab_global::on_browse_segmentinfo(wxCommandEvent &) {
  wxFileDialog dlg(nullptr, Z("Choose the segment info file"), last_open_dir, wxEmptyString, wxString::Format(Z("Segment info files (*.xml)|*.xml|%s"), ALLFILES.c_str()), wxFD_OPEN);
  if(dlg.ShowModal() != wxID_OK)
    return;

  last_open_dir = dlg.GetDirectory();
  tc_segmentinfo->SetValue(dlg.GetPath());
}

void
tab_global::on_browse_chapters(wxCommandEvent &) {
  wxFileDialog dlg(nullptr, Z("Choose the chapter file"), last_open_dir, wxEmptyString,
                   wxString::Format(Z("Chapter files (*.xml;*.txt;*.cue)|*.xml;*.txt;*.cue|%s"), ALLFILES.c_str()), wxFD_OPEN);
  if(dlg.ShowModal() != wxID_OK)
    return;

  last_open_dir = dlg.GetDirectory();
  tc_chapters->SetValue(dlg.GetPath());
}

void
tab_global::on_split_mode_selected(wxCommandEvent &) {
  enable_split_controls();
  translate_split_args();
}

void
tab_global::on_webm_mode_clicked(wxCommandEvent &) {
  mdlg->handle_webm_mode(cb_webm_mode->IsChecked());
}

void
tab_global::enable_split_controls() {
  bool enable = cob_split_mode->GetSelection() > 0;

  st_split_args->Enable(enable);
  cob_split_args->Enable(enable);

  cb_link->Enable(enable);
  st_split_max_files->Enable(enable);
  tc_split_max_files->Enable(enable);
}

void
tab_global::load(wxConfigBase *cfg,
                 int version) {
  wxString s;
  bool b, ec, er;

  cfg->SetPath(wxT("/global"));
  cfg->Read(wxT("segment_title"), &s, wxEmptyString);
  tc_title->SetValue(s);

  int split_mode_idx = 0;

  if (1 == version) {
    // Version 1 only know two split methods: by time and by size.
    // It stored that in the boolean "split_by_size".
    cfg->Read(wxT("enable_splitting"), &ec, false);
    cfg->Read(wxT("split_by_size"),    &er, true);
    split_mode_idx = !ec ? 0 : er ? 1 : 2;

  } else {
    // Version 4 folds "enable_splitting" into "split_mode".
    bool old_enabled_value = false;
    bool old_enabled_read  = cfg->Read(wxT("enable_splitting"), &old_enabled_value);

    if (!old_enabled_read || old_enabled_value) {
      wxString split_mode;
      cfg->Read(wxT("split_mode"), &split_mode, wxT("none"));

      split_mode_idx = split_mode == wxT("size")         ? 1
                     : split_mode == wxT("duration")     ? 2
                     : split_mode == wxT("timecodes")    ? 3
                     : split_mode == wxT("parts")        ? 4
                     : split_mode == wxT("parts-frames") ? 5
                     : split_mode == wxT("frames")       ? 6
                     : split_mode == wxT("chapters")     ? 7
                     :                                     0;
    }
  }

  cob_split_mode->SetSelection(split_mode_idx);

  // Version 4 uses "split_args" for all splitting options, earlier use separate options.
  if (3 < version)
    cfg->Read(wxT("split_args"), &s, wxEmptyString);
  else {
    wxString key = 1 == split_mode_idx ? wxT("split_after_bytes")
                 : 2 == split_mode_idx ? wxT("split_after_time")
                 : 3 == split_mode_idx ? wxT("split_after_timecodes")
                 :                       wxT("split_by_parts");
    cfg->Read(key, &s, wxEmptyString);
  }
  cob_split_args->SetValue(s);

  enable_split_controls();
  translate_split_args();

  cfg->Read(wxT("split_max_files"), &s, wxEmptyString);
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

  cfg->Read(wxT("segment_uid"), &s);
  tc_segment_uid->SetValue(s);
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
  cfg->Read(wxT("segmentinfo"), &s);
  tc_segmentinfo->SetValue(s);

  cfg->Read(wxT("webm_mode"), &b, false);
  cb_webm_mode->SetValue(b);

  cfg->Read(wxT("title_was_present"), &title_was_present, false);
}

void
tab_global::save(wxConfigBase *cfg) {
  cfg->SetPath(wxT("/global"));
  cfg->Write(wxT("segment_title"), tc_title->GetValue());

  auto split_mode_idx = cob_split_mode->GetSelection();
  auto value          = 1 == split_mode_idx ? wxT("size")
                      : 2 == split_mode_idx ? wxT("duration")
                      : 3 == split_mode_idx ? wxT("timecodes")
                      : 4 == split_mode_idx ? wxT("parts")
                      : 5 == split_mode_idx ? wxT("parts-frames")
                      : 6 == split_mode_idx ? wxT("frames")
                      : 7 == split_mode_idx ? wxT("chapters")
                      :                       wxT("none");
  cfg->Write(wxT("split_mode"), value);
  cfg->Write(wxT("split_args"), cob_split_args->GetValue());
  cfg->Write(wxT("link"), cb_link->IsChecked());
  cfg->Write(wxT("split_max_files"), tc_split_max_files->GetValue());

  cfg->Write(wxT("segment_uid"), tc_segment_uid->GetValue());
  cfg->Write(wxT("previous_segment_uid"), tc_previous_segment_uid->GetValue());
  cfg->Write(wxT("next_segment_uid"), tc_next_segment_uid->GetValue());

  cfg->Write(wxT("chapters"), tc_chapters->GetValue());
  cfg->Write(wxT("chapter_language"), cob_chap_language->GetValue());
  cfg->Write(wxT("chapter_charset"), cob_chap_charset->GetValue());
  cfg->Write(wxT("cue_name_format"), tc_cue_name_format->GetValue());

  cfg->Write(wxT("global_tags"), tc_global_tags->GetValue());
  cfg->Write(wxT("segmentinfo"), tc_segmentinfo->GetValue());
  cfg->Write(wxT("webm_mode"), cb_webm_mode->IsChecked());

  cfg->Write(wxT("title_was_present"), title_was_present);
}

bool
tab_global::is_valid_split_size() {
  int64_t dummy_i, mod;
  char c;
  std::string s = wxMB(cob_split_args->GetValue());

  strip(s);
  if (s.empty()) {
    wxMessageBox(Z("Splitting by size was selected, but no size has been given."), Z("mkvmerge GUI error"), wxOK | wxCENTER | wxICON_ERROR);
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
    wxMessageBox(Z("The format of the split size is invalid."), Z("mkvmerge GUI error"), wxOK | wxCENTER | wxICON_ERROR);
    return false;
  }
  if ((s.empty()) || !parse_number(s, dummy_i)) {
    wxMessageBox(Z("The format of the split size is invalid."), Z("mkvmerge GUI error"), wxOK | wxCENTER | wxICON_ERROR);
    return false;
  }
  if ((dummy_i * mod) < 1024 * 1024) {
    wxMessageBox(Z("The format of the split size is invalid (size too small)."), Z("mkvmerge GUI error"), wxOK | wxCENTER | wxICON_ERROR);
    return false;
  }

  return true;
}

bool
tab_global::is_valid_split_timecode(wxString s) {
  strip(s);
  if (s.empty()) {
    wxMessageBox(Z("Splitting by timecode/duration was selected, but nothing was entered."), Z("mkvmerge GUI error"), wxOK | wxCENTER | wxICON_ERROR);
    return false;
  }

  wxRegEx re_seconds(wxT("^[[:digit:]]+s$"));
  wxRegEx re_timecodes(wxT("^([[:digit:]]{2}:)?[[:digit:]]{2}:[[:digit:]]{2}(\\.[[:digit:]]{1,9})?$"));

  if (!re_seconds.Matches(s) && !re_timecodes.Matches(s)) {
    wxMessageBox(Z("The format of the split timecode/duration is invalid."), Z("mkvmerge GUI error"), wxOK | wxCENTER | wxICON_ERROR);
    return false;
  }

  return true;
}

bool
tab_global::is_valid_split_timecode_list() {
  std::vector<wxString> parts = split(cob_split_args->GetValue(), wxString(wxT(",")));
  for (auto &part : parts)
    if (!is_valid_split_timecode(part))
      return false;

  return true;
}

bool
tab_global::is_valid_split_frame_list() {
  auto show_error = [&]() -> bool {
    wxMessageBox(Z("The format of the split argument is invalid."), Z("mkvmerge GUI error"), wxOK | wxCENTER | wxICON_ERROR);
    return false;
  };

  if (cob_split_args->GetValue().IsEmpty())
    return show_error();

  uint64_t value;
  std::vector<wxString> parts = split(cob_split_args->GetValue(), wxString(wxT(",")));
  for (auto &part : parts)
    if (!parse_number(wxMB(part), value) || !value)
      return show_error();

  return true;
}

bool
tab_global::is_valid_split_chapters_list() {
  auto value = cob_split_args->GetValue();
  if ((value == wxT("all")) || wxRegEx{wxT("^[[:digit:]]+(,[[:digit:]])*$")}.Matches(value))
    return true;

  wxMessageBox(Z("The format of the split argument is invalid."), Z("mkvmerge GUI error"), wxOK | wxCENTER | wxICON_ERROR);
  return false;
}

bool
tab_global::validate_settings() {
  auto idx = cob_split_mode->GetSelection();

  if (   ((1 == idx) && !is_valid_split_size())
      || ((2 == idx) && !is_valid_split_timecode(cob_split_args->GetValue()))
      || ((3 == idx) && !is_valid_split_timecode_list())
      || ((6 == idx) && !is_valid_split_frame_list())
      || ((7 == idx) && !is_valid_split_chapters_list()))
    return false;

  int64_t dummy_i;
  std::string s = wxMB(tc_split_max_files->GetValue());
  strip(s);
  if (!s.empty() && (!parse_number(s, dummy_i) || (1 >= dummy_i))) {
    wxMessageBox(Z("Invalid number of max. split files given."), Z("mkvmerge GUI error"), wxOK | wxCENTER | wxICON_ERROR);
    return false;
  }

  return true;
}

void
tab_global::handle_webm_mode(bool enabled) {
  if (enabled) {
    tc_chapters->SetValue(wxEmptyString);
    tc_global_tags->SetValue(wxEmptyString);
  }

  b_browse_chapters->Enable(!enabled);
  cob_chap_charset->Enable(!enabled);
  cob_chap_language->Enable(!enabled);
  sb_chapters->Enable(!enabled);
  st_chapter_file->Enable(!enabled);
  st_charset->Enable(!enabled);
  st_cue_name_format->Enable(!enabled);
  st_language->Enable(!enabled);
  tc_chapters->Enable(!enabled);
  tc_cue_name_format->Enable(!enabled);

  b_browse_global_tags->Enable(!enabled);
  st_tag_file->Enable(!enabled);
  tc_global_tags->Enable(!enabled);
}

IMPLEMENT_CLASS(tab_global, wxPanel);
BEGIN_EVENT_TABLE(tab_global, wxPanel)
  EVT_BUTTON(ID_B_BROWSEGLOBALTAGS,          tab_global::on_browse_global_tags)
  EVT_BUTTON(ID_B_BROWSESEGMENTINFO,         tab_global::on_browse_segmentinfo)
  EVT_BUTTON(ID_B_BROWSECHAPTERS,            tab_global::on_browse_chapters)
  EVT_COMBOBOX(ID_COB_SPLIT_MODE,            tab_global::on_split_mode_selected)
  EVT_CHECKBOX(ID_CB_WEBM_MODE,              tab_global::on_webm_mode_clicked)
END_EVENT_TABLE();
