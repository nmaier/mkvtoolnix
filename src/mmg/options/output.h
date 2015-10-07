/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   declarations for the options dialog -- output tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_MMG_OPTIONS_OUTPUT_H
#define MTX_MMG_OPTIONS_OUTPUT_H

#include "common/common_pch.h"

#include <vector>
#include <wx/log.h>

#include "mmg/options/tab_base.h"

#define ID_CB_AUTOSET_OUTPUT_FILENAME             15103
#define ID_CB_ASK_BEFORE_OVERWRITING              15104
#define ID_TC_OUTPUT_DIRECTORY                    15111
#define ID_B_BROWSE_OUTPUT_DIRECTORY              15112
#define ID_RB_ODM_INPUT_FILE                      15113
#define ID_RB_ODM_PREVIOUS                        15114
#define ID_RB_ODM_FIXED                           15115
#define ID_RB_ODM_PARENT_OF_INPUT_FILE            15122
#define ID_CB_UNIQUE_OUTPUT_FILE_NAME_SUGGESTIONS 15123

class optdlg_output_tab: public optdlg_base_tab {
  DECLARE_CLASS(optdlg_output_tab);
  DECLARE_EVENT_TABLE();
public:
  wxTextCtrl *tc_output_directory;
  wxCheckBox *cb_ask_before_overwriting, *cb_autoset_output_filename, *cb_unique_output_file_name_suggestions;
  wxRadioButton *rb_odm_input_file, *rb_odm_parent_of_input_file, *rb_odm_previous, *rb_odm_fixed;
  wxButton *b_browse_output_directory;

public:
  optdlg_output_tab(wxWindow *parent, mmg_options_t &options);

  void on_browse_output_directory(wxCommandEvent &evt);
  void on_autoset_output_filename_selected(wxCommandEvent &evt);
  void on_output_directory_mode(wxCommandEvent &evt);

  void enable_output_filename_controls(bool enable);

  virtual void save_options();
  virtual wxString get_title();
};

#endif // MTX_MMG_OPTIONS_OUTPUT_H
