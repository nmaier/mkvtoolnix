/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  tab_advanced.h

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <root@sirelvis.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief declarations for the advanced tab
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __TAB_ADVANCED_H
#define __TAB_ADVANCED_H

#define ID_CB_NOCUES                      14000
#define ID_CB_NOCLUSTERSINMETASEEK        14001
#define ID_CB_DISABLELACING               14002
#define ID_CB_ENABLEDURATIONS             14003
#define ID_CB_ENABLETIMESLICES            14004
#define ID_CB_CLCHARSET                   14005

class tab_advanced: public wxPanel {
  DECLARE_CLASS(tab_advanced);
  DECLARE_EVENT_TABLE();
public:
  wxComboBox *cob_cl_charset;
  wxCheckBox *cb_no_cues, *cb_no_clusters, *cb_disable_lacing;
  wxCheckBox *cb_enable_durations, *cb_enable_timeslices;

public:
  tab_advanced(wxWindow *parent);

  void save(wxConfigBase *cfg);
  void load(wxConfigBase *cfg);
  bool validate_settings();
};

#endif // __TAB_ADVANCED_H
