/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   declarations for the input tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_TAB_INPUT_H
#define MTX_TAB_INPUT_H

#include "common/common_pch.h"

#include <wx/wx.h>

#define ID_LB_INPUTFILES                  11000
#define ID_B_ADDFILE                      11001
#define ID_B_REMOVEFILE                   11002
#define ID_CLB_TRACKS                     11003
#define ID_CB_LANGUAGE                    11004
#define ID_TC_TRACKNAME                   11005
#define ID_CB_CUES                        11006
#define ID_CB_MAKEDEFAULT                 11007
#define ID_TC_DELAY                       11008
#define ID_TC_STRETCH                     11009
#define ID_CB_NOCHAPTERS                  11010
#define ID_CB_SUBTITLECHARSET             11011
#define ID_CB_AACISSBR                    11012
#define ID_TC_TAGS                        11013
#define ID_B_BROWSETAGS                   11014
#define ID_CB_ASPECTRATIO                 11015
#define ID_CB_FOURCC                      11016
#define ID_CB_NOATTACHMENTS               11017
#define ID_CB_NOTAGS                      11018
#define ID_CB_COMPRESSION                 11019
#define ID_TC_TIMECODES                   11020
#define ID_B_BROWSE_TIMECODES             11021
#define ID_RB_ASPECTRATIO                 11022
#define ID_RB_DISPLAYDIMENSIONS           11023
#define ID_TC_DISPLAYWIDTH                11024
#define ID_TC_DISPLAYHEIGHT               11025
#define ID_B_TRACKUP                      11028
#define ID_B_TRACKDOWN                    11029
#define ID_T_INPUTVALUES                  11030
#define ID_B_APPENDFILE                   11031
#define ID_CB_STEREO_MODE                 11032
#define ID_CB_FPS                         11033
#define ID_CB_NALU_SIZE_LENGTH            11034
#define ID_TC_USER_DEFINED                11035
#define ID_B_REMOVE_ALL_FILES             11036
#define ID_CB_FORCED_TRACK                11037
#define ID_TC_CROPPING                    11038
#define ID_B_ADDITIONAL_PARTS             11039
#define ID_NB_OPTIONS                     11999

extern const wxChar *predefined_aspect_ratios[];
extern const wxChar *predefined_fourccs[];

class tab_input;

class tab_input_general: public wxPanel {
  DECLARE_CLASS(tab_input_general);
  DECLARE_EVENT_TABLE();
public:
  wxMTX_COMBOBOX_TYPE *cob_default, *cob_language, *cob_forced;
  wxTextCtrl *tc_track_name, *tc_tags, *tc_timecodes;
  wxButton *b_browse_tags, *b_browse_timecodes;

  wxStaticText *st_language, *st_track_name, *st_default, *st_forced;
  wxStaticText *st_tags, *st_timecodes;

  tab_input *input;

  translation_table_c cob_default_translations, cob_forced_translations;

public:
  tab_input_general(wxWindow *parent, tab_input *ti);

  void setup_languages();
  void setup_default_track();
  void setup_forced_track();
  void set_track_mode(mmg_track_t *t);

  void on_default_track_selected(wxCommandEvent &evt);
  void on_forced_track_selected(wxCommandEvent &evt);
  void on_language_selected(wxCommandEvent &evt);
  void on_browse_tags(wxCommandEvent &evt);
  void on_tags_changed(wxCommandEvent &evt);
  void on_track_name_changed(wxCommandEvent &evt);
  void on_timecodes_changed(wxCommandEvent &evt);
  void on_browse_timecodes_clicked(wxCommandEvent &evt);

  void translate_ui();
};

class tab_input_format: public wxPanel {
  DECLARE_CLASS(tab_input_format);
  DECLARE_EVENT_TABLE();
public:
  wxCheckBox *cb_aac_is_sbr;
  wxMTX_COMBOBOX_TYPE *cob_sub_charset, *cob_aspect_ratio, *cob_fourcc, *cob_stereo_mode, *cob_fps, *cob_nalu_size_length;
  wxTextCtrl *tc_delay, *tc_stretch, *tc_cropping;
  wxRadioButton *rb_aspect_ratio, *rb_display_dimensions;
  wxTextCtrl *tc_display_width, *tc_display_height;

  wxStaticText *st_delay, *st_stretch, *st_stereo_mode, *st_cropping;
  wxStaticText *st_x, *st_sub_charset, *st_fourcc;
  wxStaticText *st_fps, *st_nalu_size_length;

  tab_input *input;

  translation_table_c cob_sub_charset_translations;

public:
  tab_input_format(wxWindow *parent, tab_input *ti);

  void setup_control_contents();
  void set_track_mode(mmg_track_t *t);

  void on_aac_is_sbr_clicked(wxCommandEvent &evt);
  void on_subcharset_selected(wxCommandEvent &evt);
  void on_delay_changed(wxCommandEvent &evt);
  void on_stretch_changed(wxCommandEvent &evt);
  void on_aspect_ratio_selected(wxCommandEvent &evt);
  void on_aspect_ratio_changed(wxCommandEvent &evt);
  void on_display_dimensions_selected(wxCommandEvent &evt);
  void on_display_width_changed(wxCommandEvent &evt);
  void on_display_height_changed(wxCommandEvent &evt);
  void on_fourcc_changed(wxCommandEvent &evt);
  void on_fps_changed(wxCommandEvent &evt);
  void on_nalu_size_length_changed(wxCommandEvent &evt);
  void on_stereo_mode_changed(wxCommandEvent &evt);
  void on_cropping_changed(wxCommandEvent &evt);

  void translate_ui();
};

class tab_input_extra: public wxPanel {
  DECLARE_CLASS(tab_input_extra);
  DECLARE_EVENT_TABLE();
public:
  wxMTX_COMBOBOX_TYPE *cob_cues, *cob_compression;
  wxTextCtrl *tc_user_defined;

  wxStaticText *st_cues, *st_user_defined, *st_compression;

  tab_input *input;

  translation_table_c cob_cues_translations, cob_compression_translations;

public:
  tab_input_extra(wxWindow *parent, tab_input *ti);

  void setup_cues();
  void setup_compression();

  void set_track_mode(mmg_track_t *t);

  void on_cues_selected(wxCommandEvent &evt);
  void on_compression_selected(wxCommandEvent &evt);
  void on_user_defined_changed(wxCommandEvent &evt);

  void translate_ui();
};

class tab_input: public wxPanel {
  DECLARE_CLASS(tab_input);
  DECLARE_EVENT_TABLE();
public:
  tab_input_general *ti_general;
  tab_input_format *ti_format;
  tab_input_extra *ti_extra;

  wxListBox *lb_input_files;
  wxButton *b_add_file, *b_remove_file, *b_remove_all_files;
  wxButton *b_track_up, *b_track_down, *b_append_file, *b_additional_parts;
  wxCheckListBox *clb_tracks;
  wxStaticText *st_tracks, *st_input_files;
  wxNotebook *nb_options;

  wxTimer value_copy_timer;
  bool dont_copy_values_now;

  int selected_file, selected_track;

  wxString media_files;

public:
  tab_input(wxWindow *parent);

  void on_add_file(wxCommandEvent &evt);
  void add_file(const wxString &file_name, bool append);
  void select_file(bool append);
  void on_remove_file(wxCommandEvent &evt);
  void on_remove_all_files(wxCommandEvent &evt);
  void on_append_file(wxCommandEvent &evt);
  void on_file_selected(wxCommandEvent &evt);
  void on_move_track_up(wxCommandEvent &evt);
  void on_move_track_down(wxCommandEvent &evt);
  void on_additional_parts(wxCommandEvent &evt);
  void on_track_selected(wxCommandEvent &evt);
  void on_track_enabled(wxCommandEvent &evt);
  void on_value_copy_timer(wxTimerEvent &evt);
  void on_file_new(wxCommandEvent &evt);

  void set_track_mode(mmg_track_t *t);
  void enable_file_controls();

  void save(wxConfigBase *cfg);
  void load(wxConfigBase *cfg, int version);
  bool validate_settings();

  wxString setup_file_type_filter();

  void translate_ui();
  void handle_webm_mode(bool enabled);

  void parse_track_line(mmg_file_cptr file, wxString const &line, wxString const &delay_from_file_name, std::map<char, bool> &default_track_found_for);
  void parse_container_line(mmg_file_cptr file, wxString const &line);
  void parse_attachment_line(mmg_file_cptr file, wxString const &line);
  void parse_chapters_line(mmg_file_cptr file, wxString const &line);
  void parse_global_tags_line(mmg_file_cptr file, wxString const &line);
  void parse_tags_line(mmg_file_cptr file, wxString const &line);
  void parse_identification_output(mmg_file_cptr file, wxArrayString const &output);
  bool run_mkvmerge_identification(wxString const &file_name, wxArrayString &output);
  void insert_file_in_controls(mmg_file_cptr file, bool append);
  wxString check_for_and_handle_playlist_file(wxString const &file_name, wxArrayString &original_output);
  std::map<wxString, wxString> parse_properties(wxString const &wanted_line, wxArrayString const &output) const;
};

#endif // MTX_TAB_INPUT_H
