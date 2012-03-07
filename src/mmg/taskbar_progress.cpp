#include "common/os.h"

#if defined(SYS_WINDOWS)

# include <wx/wx.h>
# include <wx/statusbr.h>

# include "mmg/mmg_dialog.h"
# include "mmg/taskbar_progress.h"

taskbar_progress_c::taskbar_progress_c(wxWindow *window)
  : m_window(window)
  , m_state(TBPF_NOPROGRESS)
  , m_completed(0)
  , m_total(0)
  , m_interface(nullptr)
{
}

taskbar_progress_c::~taskbar_progress_c() {
  release_interface();
}

void
taskbar_progress_c::set_value(ULONGLONG completed,
                              ULONGLONG total) {
  m_completed = completed;
  m_total     = total;

  if (nullptr != get_interface())
    m_interface->lpVtbl->SetProgressValue(m_interface, GetHwndOf(m_window), m_completed, m_total);
}

void
taskbar_progress_c::set_state(TBPFLAG state) {
  m_state = state;

  if (nullptr != get_interface())
    m_interface->lpVtbl->SetProgressState(m_interface, GetHwndOf(m_window), m_state);
}

void
taskbar_progress_c::release_interface() {
  if (nullptr != m_interface)
    m_interface->lpVtbl->Release(m_interface);

  m_interface = nullptr;
}

ITaskbarList3 *
taskbar_progress_c::get_interface() {
  if (!mdlg->m_taskbar_msg_received || (nullptr != m_interface))
    return m_interface;

  ITaskbarList3 *iface;
  HRESULT hr = CoCreateInstance(MTX_CLSID_TaskbarList, nullptr, CLSCTX_INPROC_SERVER, IID_ITaskbarList3, reinterpret_cast<void **>(&iface));

  if (SUCCEEDED(hr)) {
    m_interface = iface;

    set_state(m_state);
    set_value(m_completed, m_total);
  }

  return m_interface;
}

IMPLEMENT_DYNAMIC_CLASS(taskbar_progress_c, wxEvtHandler)

#endif  // SYS_WINDOWS
