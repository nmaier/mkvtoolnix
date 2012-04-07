/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "show text" dialog definitions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MMG_SHOW_TEXT_DLG_H
#define __MMG_SHOW_TEXT_DLG_H

#include "common/common_pch.h"

#include "mmg/mmg.h"
#include "common/wx.h"

class show_text_dlg: public wxDialog {
  DECLARE_CLASS(show_text_dlg);
  DECLARE_EVENT_TABLE();
public:
  show_text_dlg(wxWindow *parent, const wxString &title, const wxString &text);
};

#endif  // __MMG_SHOW_TEXT_DLG_H
