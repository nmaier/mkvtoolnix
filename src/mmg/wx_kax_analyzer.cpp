/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Matroska file analyzer (wxWidgets interface)

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <wx/app.h>
#include <wx/log.h>
#include <wx/progdlg.h>

#include "common/strings/editing.h"
#include "mmg/mmg.h"
#include "mmg/wx_kax_analyzer.h"

wx_kax_analyzer_c::wx_kax_analyzer_c(wxWindow *parent,
                                     std::string file_name)
  : kax_analyzer_c(file_name)
  , m_parent(parent)
{
}

wx_kax_analyzer_c::~wx_kax_analyzer_c() {
}

void
wx_kax_analyzer_c::show_progress_start(int64_t) {
  m_prog_dlg = new wxProgressDialog(Z("Analysis is running"), Z("The Matroska file is analyzed."), 100, m_parent,
                                    wxPD_AUTO_HIDE | wxPD_APP_MODAL | wxPD_CAN_ABORT | wxPD_REMAINING_TIME);

  while (app->Pending())
    app->Dispatch();
}

bool
wx_kax_analyzer_c::show_progress_running(int percentage) {
  bool aborted = !m_prog_dlg->Update(percentage);
  while (app->Pending())
    app->Dispatch();

  return !aborted;
}

void
wx_kax_analyzer_c::show_progress_done() {
  delete m_prog_dlg;
  m_prog_dlg = NULL;
}

void
wx_kax_analyzer_c::_log_debug_message(const std::string &message) {
  kax_analyzer_c::_log_debug_message(message);

  std::string msg_no_nl = message;
  strip_back(msg_no_nl, true);
  wxLogMessage(wxU(msg_no_nl));
}

void
wx_kax_analyzer_c::debug_abort_process() {
  throw uer_error_unknown;
}
