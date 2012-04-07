/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   declarations for the global tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __TAB_GLOBAL_H
#define __TAB_GLOBAL_H

#include "common/common_pch.h"

#include "common/wx.h"

#define ID_B_BROWSEGLOBALTAGS             13000
#define ID_B_BROWSECHAPTERS               13001
#define ID_CB_SPLIT                       13002
#define ID_RB_SPLITBYSIZE                 13003
#define ID_RB_SPLITBYTIME                 13004
#define ID_CB_SPLITBYSIZE                 13005
#define ID_CB_SPLITBYTIME                 13006
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
#define ID_RB_SPLITAFTERCHAPTERS          13017
#define ID_TC_SPLITAFTERCHAPTERS          13018
#define ID_RB_SPLITAFTEREACHCHAPTER       13019
#define ID_RB_SPLITAFTERTIMECODES         13020
#define ID_TC_SPLITAFTERTIMECODES         13021
#define ID_TC_SEGMENTUID                  13022
#define ID_TC_SEGMENTINFO                 13023
#define ID_B_BROWSESEGMENTINFO            13024
#define ID_CB_WEBM_MODE                   13025
#define ID_RB_SPLITBYPARTS                13026
#define ID_TC_SPLITBYPARTS                13027

class tab_global: public wxPanel {
  DECLARE_CLASS(tab_global);
  DECLARE_EVENT_TABLE();
public:
  wxTextCtrl *tc_chapters, *tc_global_tags, *tc_split_max_files, *tc_title;
  wxTextCtrl *tc_next_segment_uid, *tc_previous_segment_uid, *tc_segment_uid;
  wxTextCtrl *tc_split_bytes, *tc_split_time, *tc_cue_name_format;
  wxTextCtrl *tc_split_chapters, *tc_split_after_timecodes, *tc_split_by_parts, *tc_segmentinfo;
  wxCheckBox *cb_split, *cb_link;
  wxRadioButton *rb_split_by_size, *rb_split_by_time, *rb_split_each_chapter;
  wxRadioButton *rb_split_chapters, *rb_split_after_timecodes, *rb_split_by_parts;
  wxMTX_COMBOBOX_TYPE *cob_split_by_size, *cob_split_by_time, *cob_chap_language, *cob_chap_charset;
  wxStaticText *st_split_max_files, *st_file_segment_title, *st_segment_uid, *st_previous_segment_uid, *st_next_segment_uid, *st_chapter_file, *st_language, *st_charset;
  wxStaticText *st_cue_name_format, *st_tag_file, *st_segmentinfo_file;
  wxStaticBox *sb_splitting, *sb_file_segment_title, *sb_file_segment_linking, *sb_chapters;
  wxButton *b_browse_chapters, *b_browse_global_tags, *b_browse_segmentinfo;
  wxCheckBox *cb_webm_mode;

public:
  tab_global(wxWindow *parent);

  void on_browse_chapters(wxCommandEvent &evt);
  void on_browse_global_tags(wxCommandEvent &evt);
  void on_browse_segmentinfo(wxCommandEvent &evt);
  void on_split_clicked(wxCommandEvent &evt);
  void on_splitby_size_clicked(wxCommandEvent &evt);
  void on_splitby_time_clicked(wxCommandEvent &evt);
  void on_splitafter_timecodes_clicked(wxCommandEvent &evt);
  void on_splitby_parts_clicked(wxCommandEvent &evt);
  void on_webm_mode_clicked(wxCommandEvent &evt);

  void save(wxConfigBase *cfg);
  void load(wxConfigBase *cfg, int version);
  bool validate_settings();

  bool is_valid_split_size();
  bool is_valid_split_timecode(wxString s);
  bool is_valid_split_timecode_list();

  void translate_ui();
  void handle_webm_mode(bool enabled);
};

#endif // __TAB_GLOBAL_H
