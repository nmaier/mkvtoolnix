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

#include "common/common_pch.h"

#include <wx/log.h>

#include "mmg/options/tab_base.h"

class optdlg_languages_tab: public optdlg_base_tab {
  DECLARE_CLASS(optdlg_languages_tab);
protected:
  wxListBox *lb_popular_languages;

public:
  static wxArrayString s_languages;

public:
  optdlg_languages_tab(wxWindow *parent, mmg_options_t &options);

  virtual void save_options();
  virtual wxString get_title();
};

#endif // __MMG_OPTIONS_LANGUAGES_H
