/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   declarations for the options dialog -- chapters tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MMG_OPTIONS_CHAPTERS_H
#define __MMG_OPTIONS_CHAPTERS_H

#include "common/common_pch.h"

#include <wx/log.h>
#include <wx/panel.h>

#include "common/wx.h"
#include "mmg/options/tab_base.h"

class optdlg_chapters_tab: public optdlg_base_tab {
  DECLARE_CLASS(optdlg_chapters_tab);
public:
  wxMTX_COMBOBOX_TYPE *cob_language, *cob_country;

public:
  optdlg_chapters_tab(wxWindow *parent, mmg_options_t &options);

  virtual void save_options();
  virtual bool validate_choices();
  virtual wxString get_title();
};

#endif // __MMG_OPTIONS_CHAPTERS_H
