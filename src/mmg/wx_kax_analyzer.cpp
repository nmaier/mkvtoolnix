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

#include <wx/progdlg.h>

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
wx_kax_analyzer_c::show_progress_start(int64_t file_size) {
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
