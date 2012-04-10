/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   muxing dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MUX_DIALOG_H
#define __MUX_DIALOG_H

#include "common/common_pch.h"

#include <wx/dialog.h>
#include <wx/process.h>

#if defined(SYS_WINDOWS)
# include "common/fs_sys_helpers.h"
# include "mmg/taskbar_progress.h"
#endif  // SYS_WINDOWS

#define ID_B_MUX_OK                       18000
#define ID_B_MUX_SAVELOG                  18001
#define ID_B_MUX_ABORT                    18002

extern const wxEventType mux_thread_event;

class mux_thread;
class mux_process;

class mux_dialog: public wxDialog {
  DECLARE_CLASS(mux_dialog);
  DECLARE_EVENT_TABLE();
protected:
  friend class mux_thread;

  long pid;
  wxStaticText *st_label, *st_remaining_time_label, *st_remaining_time;
  wxGauge *g_progress;
  wxCriticalSection m_cs_thread;
  mux_thread *m_thread;
  mux_process *m_process;
  int m_pid;
  wxString log, opt_file_name;
  wxButton *b_ok, *b_save_log, *b_abort;
  wxTextCtrl *tc_output, *tc_warnings, *tc_errors;
#if defined(SYS_WINDOWS)
  taskbar_progress_c *m_taskbar_progress;
#endif  // SYS_WINDOWS
  bool m_abort_button_changed;

  int m_exit_code, m_progress;
  int64_t m_next_remaining_time_update, m_start_time;
public:

  mux_dialog(wxWindow *parent);
  ~mux_dialog();

  void run();

  void update_label(wxString const &text);
  void update_gauge(long value);
  void update_remaining_time();

  void on_ok(wxCommandEvent &evt);
  void on_save_log(wxCommandEvent &evt);
  void on_abort(wxCommandEvent &evt);
  void on_close(wxCloseEvent &evt);
  void on_output_available(wxCommandEvent &evt);
  void on_process_terminated(wxCommandEvent &evt);

#if defined(SYS_WINDOWS)
  void change_abort_button();
#endif  // SYS_WINDOWS
};

class mux_process: public wxProcess {
  friend class mux_dialog;

private:
  mux_dialog *m_dialog;
  wxCriticalSection m_cs_dialog;

public:
  mux_process(mux_dialog *dialog);
  virtual void OnTerminate(int terminated_pid, int status);
};

class mux_thread: public wxThread {
public:
  static wxEventType const event;
  enum event_type_e {
    output_available = 1,
    process_terminated,
  };

private:
  mux_dialog *m_dialog;
  wxProcess *m_process;

public:
  mux_thread(mux_dialog *dialog, wxProcess *process);
  virtual void *Entry();
};

#endif // __MUX_DIALOG_H
