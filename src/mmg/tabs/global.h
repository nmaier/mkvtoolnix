/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   declarations for the global tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_TAB_GLOBAL_H
#define MTX_TAB_GLOBAL_H

#include "common/common_pch.h"

#include "common/wx.h"

#define ID_B_BROWSEGLOBALTAGS             13000
#define ID_B_BROWSECHAPTERS               13001
#define ID_COB_SPLIT_MODE                 13002
#define ID_COB_SPLIT_ARG                  13003
#define ID_CB_LINK                        13007
#define ID_TC_SPLITMAXFILES               13008
#define ID_TC_PREVIOUSSEGMENTUID          13009
#define ID_TC_NEXTSEGMENTUID              13010
#define ID_TC_CHAPTERS                    13011
#define ID_CB_CHAPTERLANGUAGE             13012
#define ID_CB_CHAPTERCHARSET              13013
#define ID_TC_GLOBALTAGS                  13014
#define ID_TC_SEGMENTTITLE                13015
#define ID_TC_CUENAMEFORMAT               13016
#define ID_TC_SEGMENTUID                  13022
#define ID_TC_SEGMENTINFO                 13023
#define ID_B_BROWSESEGMENTINFO            13024
#define ID_CB_WEBM_MODE                   13025

class tab_global: public wxPanel {
  DECLARE_CLASS(tab_global);
  DECLARE_EVENT_TABLE();
public:
  wxTextCtrl *tc_chapters, *tc_global_tags, *tc_split_max_files, *tc_title;
  wxTextCtrl *tc_next_segment_uid, *tc_previous_segment_uid, *tc_segment_uid;
  wxTextCtrl *tc_cue_name_format, *tc_segmentinfo;
  wxCheckBox *cb_link;
  wxMTX_COMBOBOX_TYPE *cob_split_mode, *cob_split_args, *cob_chap_language, *cob_chap_charset;
  wxStaticText *st_split_mode, *st_split_args, *st_split_max_files, *st_file_segment_title, *st_segment_uid, *st_previous_segment_uid, *st_next_segment_uid, *st_chapter_file, *st_language, *st_charset;
  wxStaticText *st_cue_name_format, *st_tag_file, *st_segmentinfo_file;
  wxStaticBox *sb_split, *sb_file_segment_title, *sb_file_segment_linking, *sb_chapters;
  wxButton *b_browse_chapters, *b_browse_global_tags, *b_browse_segmentinfo;
  wxCheckBox *cb_webm_mode;

public:
  tab_global(wxWindow *parent);

  void on_browse_chapters(wxCommandEvent &evt);
  void on_browse_global_tags(wxCommandEvent &evt);
  void on_browse_segmentinfo(wxCommandEvent &evt);
  void on_split_mode_selected(wxCommandEvent &evt);
  void on_webm_mode_clicked(wxCommandEvent &evt);

  void save(wxConfigBase *cfg);
  void load(wxConfigBase *cfg, int version);
  bool validate_settings();

  bool is_valid_split_size();
  bool is_valid_split_timecode(wxString s);
  bool is_valid_split_timecode_list();
  void enable_split_controls();

  void translate_ui();
  void translate_split_args();
  void handle_webm_mode(bool enabled);
};

#endif // MTX_TAB_GLOBAL_H
