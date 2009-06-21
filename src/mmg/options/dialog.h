/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   declarations for the options dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MMG_OPTIONS_DIALOG_H
#define __MMG_OPTIONS_DIALOG_H

#include "common/os.h"

#include <wx/log.h>

#include <vector>

#define ID_TC_MKVMERGE                     15000
#define ID_B_BROWSEMKVMERGE                15001
#define ID_COB_PRIORITY                    15002
#define ID_CB_AUTOSET_OUTPUT_FILENAME      15003
#define ID_CB_ASK_BEFORE_OVERWRITING       15004
#define ID_CB_ON_TOP                       15005
#define ID_CB_NEW_AFTER_ADD_TO_JOBQUEUE    15006
#define ID_CB_WARN_USAGE                   15007
#define ID_CB_GUI_DEBUGGING                15008
#define ID_CB_SET_DELAY_FROM_FILENAME      15010
#define ID_TC_OUTPUT_DIRECTORY             15011
#define ID_B_BROWSE_OUTPUT_DIRECTORY       15012
#define ID_RB_ODM_INPUT_FILE               15013
#define ID_RB_ODM_PREVIOUS                 15014
#define ID_RB_ODM_FIXED                    15015
#define ID_COB_UI_LANGUAGE                 15016
#define ID_CB_NEW_AFTER_SUCCESSFUL_MUX     15017

class options_dialog: public wxDialog {
  DECLARE_CLASS(options_dialog);
  DECLARE_EVENT_TABLE();
public:
  wxTextCtrl *tc_mkvmerge, *tc_output_directory;
  wxCheckBox *cb_autoset_output_filename;
  wxCheckBox *cb_ask_before_overwriting, *cb_on_top;
  wxCheckBox *cb_filenew_after_add_to_jobqueue;
  wxCheckBox *cb_filenew_after_successful_mux;
  wxCheckBox *cb_warn_usage, *cb_gui_debugging;
  wxCheckBox *cb_set_delay_from_filename;
  wxMTX_COMBOBOX_TYPE *cob_priority;
  wxRadioButton *rb_odm_input_file, *rb_odm_previous, *rb_odm_fixed;
  wxButton *b_browse_output_directory;

  mmg_options_t &m_options;

#if defined(HAVE_LIBINTL_H)
  wxMTX_COMBOBOX_TYPE *cob_ui_language;
  std::vector<std::string> m_sorted_locales;
#endif  // HAVE_LIBINTL_H

public:
  translation_table_c cob_priority_translations;

public:
  options_dialog(wxWindow *parent, mmg_options_t &options);

  void on_browse_mkvmerge(wxCommandEvent &evt);
  void on_browse_output_directory(wxCommandEvent &evt);
  void on_autoset_output_filename_selected(wxCommandEvent &evt);
  void on_ok(wxCommandEvent &evt);
  void on_output_directory_mode(wxCommandEvent &evt);

  void enable_output_filename_controls(bool enable);

  void select_priority(const wxString &priority);
  wxString get_selected_priority();
};

#endif // __MMG_OPTIONS_DIALOG_H
