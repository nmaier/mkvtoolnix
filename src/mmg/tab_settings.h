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

class tab_settings: public wxPanel {
  DECLARE_CLASS(tab_settings);
  DECLARE_EVENT_TABLE();
protected:
  wxTextCtrl *tc_mkvmerge;
  wxCheckBox *cb_show_commandline;

public:
  tab_settings(wxWindow *parent);
  virtual ~tab_settings();

  void on_browse(wxCommandEvent &evt);

  void load_preferences();
  void save_preferences();

  void save(wxConfigBase *cfg);
  void load(wxConfigBase *cfg);
  bool validate_settings();
};

#endif // __TAB_SETTINGS_H
