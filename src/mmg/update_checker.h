/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   update checker thread & dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MMG_UPDATE_CHECKER_H
# define __MMG_UPDATE_CHECKER_H

# include "common/os.h"

# if defined(HAVE_CURL_EASY_H)

#  include <wx/thread.h>
#  include "common/version.h"

class mmg_dialog;

class update_check_thread_c: public wxThread {
private:
  mmg_dialog *m_mdlg;

public:
  update_check_thread_c(mmg_dialog *mdlg);
  virtual void *Entry();
};

DECLARE_EVENT_TYPE(wxEVT_MTX_UPDATE_CHECK_STATE_CHANGED, -1);

#  define UPDATE_CHECK_START               1
#  define UPDATE_CHECK_DONE_NO_NEW_RELEASE 2
#  define UPDATE_CHECK_DONE_NEW_RELEASE    3
#  define UPDATE_CHECK_DONE_ERROR          4

# endif  // defined(HAVE_CURL_EASY_H)
#endif // __MMG_UPDATE_CHECKER_H
