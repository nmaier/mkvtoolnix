/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   declarations for the options dialog -- abstract tab base class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MMG_OPTIONS_TAB_BASE_H
#define __MMG_OPTIONS_TAB_BASE_H

#include "common/common_pch.h"

#include <wx/log.h>
#include <wx/panel.h>

class optdlg_base_tab: public wxPanel {
  DECLARE_CLASS(optdlg_base_tab);
protected:
  mmg_options_t &m_options;

public:
  optdlg_base_tab(wxWindow *parent, mmg_options_t &options);

  virtual void save_options() = 0;
  virtual bool validate_choices();
  virtual wxString get_title() = 0;
};

#endif // __MMG_OPTIONS_TAB_BASE_H
