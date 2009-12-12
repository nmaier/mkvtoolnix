/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   main dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#if defined(SYS_WINDOWS)

# include <wx/wx.h>
# include <wx/config.h>

# include "mmg/mmg_dialog.h"
# include "mmg/taskbar_progress.h"

static UINT gs_msg_taskbar_button_created = 0;

WXLRESULT
mmg_dialog::MSWWindowProc(WXUINT msg,
                          WXWPARAM wParam,
                          WXLPARAM lParam) {
  if (gs_msg_taskbar_button_created == msg)
    m_taskbar_msg_received = true;

  return wxFrame::MSWWindowProc(msg, wParam, lParam);
}

void
mmg_dialog::RegisterWindowMessages() {
  static bool s_registered = false;

  if (s_registered)
    return;

  gs_msg_taskbar_button_created = RegisterWindowMessage(wxT("TaskbarButtonCreated"));
  s_registered                  = true;
}

#endif  // SYS_WINDOWS
