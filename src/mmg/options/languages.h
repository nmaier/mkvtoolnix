/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   declarations for the options dialog -- languages tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MMG_OPTIONS_LANGUAGES_H
#define __MMG_OPTIONS_LANGUAGES_H

#include "common/os.h"

#include <wx/log.h>

#include <vector>

class optdlg_languages_tab: public wxPanel {
  DECLARE_CLASS(optdlg_languages_tab);
  DECLARE_EVENT_TABLE();
public:
  wxListBox *lb_popular_languages;

  mmg_options_t &m_options;

public:
  static wxArrayString s_languages;

public:
  optdlg_languages_tab(wxWindow *parent, mmg_options_t &options);

  void save_options();
};

#endif // __MMG_OPTIONS_LANGUAGES_H
