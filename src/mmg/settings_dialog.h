/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   declarations for the settings dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __SETTINGS_DIALOG_H
#define __SETTINGS_DIALOG_H

#include "wx/log.h"

#define ID_TC_MKVMERGE                     15000
#define ID_B_BROWSEMKVMERGE                15001
#define ID_COB_PRIORITY                    15002
#define ID_CB_AUTOSET_OUTPUT_FILENAME      15003
#define ID_CB_ASK_BEFORE_OVERWRITING       15004
#define ID_CB_ON_TOP                       15005
#define ID_CB_NEW_AFTER_ADD_TO_JOBQUEUE    15006
#define ID_CB_WARN_USAGE                   15007
#define ID_CB_GUI_DEBUGGING                15008
#define ID_CB_ALWAYS_USE_SIMPLEBLOCK       15009
#define ID_CB_SET_DELAY_FROM_FILENAME      15010
#define ID_TC_OUTPUT_DIRECTORY             15011
#define ID_B_BROWSE_OUTPUT_DIRECTORY       15012

class settings_dialog: public wxDialog {
  DECLARE_CLASS(settings_dialog);
  DECLARE_EVENT_TABLE();
public:
  wxTextCtrl *tc_mkvmerge, *tc_output_directory;
  wxCheckBox *cb_autoset_output_filename;
  wxCheckBox *cb_ask_before_overwriting, *cb_on_top;
  wxCheckBox *cb_filenew_after_add_to_jobqueue;
  wxCheckBox *cb_warn_usage, *cb_gui_debugging;
  wxCheckBox *cb_always_use_simpleblock, *cb_set_delay_from_filename;
  wxComboBox *cob_priority;
  wxStaticText *st_output_directory;
  wxButton *b_browse_output_directory;

  mmg_settings_t &m_settings;

public:
  settings_dialog(wxWindow *parent, mmg_settings_t &settings);

  void on_browse_mkvmerge(wxCommandEvent &evt);
  void on_browse_output_directory(wxCommandEvent &evt);
  void on_autoset_output_filename_selected(wxCommandEvent &evt);
  void on_ok(wxCommandEvent &evt);

  void enable_output_filename_controls(bool enable);
};

#endif // __SETTINGS_DIALOG_H
