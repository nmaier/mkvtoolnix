/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   declarations for the settings tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __TAB_SETTINGS_H
#define __TAB_SETTINGS_H

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

class tab_settings: public wxPanel {
  DECLARE_CLASS(tab_settings);
  DECLARE_EVENT_TABLE();
public:
  wxTextCtrl *tc_mkvmerge;
  wxCheckBox *cb_show_commandline, *cb_autoset_output_filename;
  wxCheckBox *cb_ask_before_overwriting, *cb_on_top;
  wxCheckBox *cb_filenew_after_add_to_jobqueue;
  wxCheckBox *cb_warn_usage, *cb_gui_debugging;
  wxComboBox *cob_priority;

public:
  tab_settings(wxWindow *parent);
  virtual ~tab_settings();

  void on_browse(wxCommandEvent &evt);
  void on_xyz_selected(wxCommandEvent &evt);
  void on_on_top_selected(wxCommandEvent &evt);
  void on_gui_debugging_selected(wxCommandEvent &evt);

  void load_preferences();
  void save_preferences();

  void save(wxConfigBase *cfg);
  void load(wxConfigBase *cfg);
  bool validate_settings();

  void query_mkvmerge_capabilities();
};

#endif // __TAB_SETTINGS_H
