/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Matroska file analyzer (wxWidgets interface)

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __WX_KAX_ANALYZER_H
#define __WX_KAX_ANALYZER_H

#include "common/os.h"

#include <wx/progdlg.h>
#include <wx/window.h>

#include "common/common.h"
#include "common/kax_analyzer.h"

#define ID_B_ANALYZER_ABORT 11000

class wx_kax_analyzer_c: public kax_analyzer_c {
private:
  wxWindow *m_parent;
  wxProgressDialog *m_prog_dlg;

public:
  wx_kax_analyzer_c(wxWindow *parent, std::string file_name);
  virtual ~wx_kax_analyzer_c();

  virtual void show_progress_start(int64_t size);
  virtual bool show_progress_running(int percentage);
  virtual void show_progress_done();
};

#endif // __WX_KAX_ANALYZER_H
