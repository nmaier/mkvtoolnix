#ifndef __MMG_TASKBAR_PROGRESS_H
#define __MMG_TASKBAR_PROGRESS_H

#include "common/os.h"

#if defined(SYS_WINDOWS)

# include <wx/window.h>
# include <wx/frame.h>
# include <wx/utils.h>
# include <wx/menu.h>

# include <wx/msw/private.h>
# include <wx/msw/winundef.h>

# include <string.h>

# include "common/win_itaskbarlist3.h"

class taskbar_progress_c: public wxEvtHandler {
  DECLARE_DYNAMIC_CLASS_NO_COPY(taskbar_progress_c);
protected:
  wxWindow *m_window;
  TBPFLAG m_state;
  ULONGLONG m_completed, m_total;
  ITaskbarList3 *m_interface;

public:
  taskbar_progress_c(wxWindow *window = nullptr);
  virtual ~taskbar_progress_c();
  virtual void set_value(ULONGLONG completed, ULONGLONG total);
  virtual void set_state(TBPFLAG state);

protected:
  ITaskbarList3 *get_interface();
  void release_interface();
};

#endif  // defined(SYS_WINDOWS)

#endif  // __MMG_TASKBAR_PROGRESS_H
