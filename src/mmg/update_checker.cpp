/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   update checker thread & dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#if defined(HAVE_CURL_EASY_H)

# include <wx/wx.h>
# include <wx/thread.h>

# include "common/version.h"
# include "mmg/mmg_dialog.h"
# include "mmg/update_checker.h"

DEFINE_EVENT_TYPE(wxEVT_MTX_UPDATE_CHECK_STATE_CHANGED);
update_check_thread_c::update_check_thread_c(mmg_dialog *mdlg)
  : wxThread(wxTHREAD_DETACHED)
  , m_mdlg(mdlg)
{
}

void *
update_check_thread_c::Entry() {
  wxCommandEvent event(wxEVT_MTX_UPDATE_CHECK_STATE_CHANGED, UPDATE_CHECK_START);
  wxPostEvent(m_mdlg, event);

  mtx_release_version_t release = get_latest_release_version();

  if (!release.valid)
    event.SetId(UPDATE_CHECK_DONE_ERROR);

  else if (release.current_version < release.latest_source) {
    m_mdlg->set_release_version(release);
    event.SetId(UPDATE_CHECK_DONE_NEW_RELEASE);

  } else
    event.SetId(UPDATE_CHECK_DONE_NO_NEW_RELEASE);

  wxPostEvent(m_mdlg, event);

  return NULL;
}

#endif  // defined(HAVE_CURL_EASY_H)
