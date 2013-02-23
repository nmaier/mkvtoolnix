/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   ask_scan_for_playlists_dlg

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_MMG_TABS_ASK_SCAN_FOR_PLAYLISTS_DLG_H
#define MTX_MMG_TABS_ASK_SCAN_FOR_PLAYLISTS_DLG_H

#include "common/common_pch.h"

#include <wx/button.h>
#include <wx/dialog.h>

#include "common/wx.h"

class ask_scan_for_playlists_dlg : public wxDialog {
protected:
  wxMTX_COMBOBOX_TYPE *m_cob_in_the_future;

public:
  ask_scan_for_playlists_dlg(wxWindow *parent, size_t num_other_files);
  ~ask_scan_for_playlists_dlg();
};

#endif // MTX_MMG_TABS_ASK_SCAN_FOR_PLAYLISTS_DLG_H
