/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  tab_settings.h

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <root@sirelvis.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief declarations for the settings tab
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __TAB_SETTINGS_H
#define __TAB_SETTINGS_H

#define ID_TC_MKVMERGE                     15000
#define ID_B_BROWSEMKVMERGE                15001
#define ID_COB_PRIORITY                    15002
#define ID_CB_AUTOSET_OUTPUT_FILENAME      15003

class tab_settings: public wxPanel {
  DECLARE_CLASS(tab_settings);
  DECLARE_EVENT_TABLE();
public:
  wxTextCtrl *tc_mkvmerge;
  wxCheckBox *cb_show_commandline, *cb_autoset_output_filename;
  wxComboBox *cob_priority;

public:
  tab_settings(wxWindow *parent);
  virtual ~tab_settings();

  void on_browse(wxCommandEvent &evt);
  void on_xyz_selected(wxCommandEvent &evt);

  void load_preferences();
  void save_preferences();

  void save(wxConfigBase *cfg);
  void load(wxConfigBase *cfg);
  bool validate_settings();

  void query_mkvmerge_capabilities();
};

#endif // __TAB_SETTINGS_H
