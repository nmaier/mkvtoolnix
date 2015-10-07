/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   main dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#if defined(SYS_WINDOWS)

# include <wx/wx.h>
# include <wx/config.h>

# include "mmg/mmg_dialog.h"
# include "mmg/taskbar_progress.h"

static UINT gs_msg_taskbar_button_created = 0;
size_t mmg_dialog::ms_dpi_x               = 96;
size_t mmg_dialog::ms_dpi_y               = 96;

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

void
mmg_dialog::init_dpi_settings() {
  HDC hdc = GetDC(nullptr);
  if (!hdc)
    return;

  ms_dpi_x = GetDeviceCaps(hdc, LOGPIXELSX);
  ms_dpi_y = GetDeviceCaps(hdc, LOGPIXELSY);
  ReleaseDC(nullptr, hdc);
}

size_t
mmg_dialog::scale_with_dpi(size_t width_or_height,
                           bool is_width) {
  return width_or_height * (is_width ? ms_dpi_x : ms_dpi_y) / 96;
}

bool
mmg_dialog::is_higher_dpi() {
  return (96 != ms_dpi_x) || (96 != ms_dpi_y);
}

#endif  // SYS_WINDOWS
