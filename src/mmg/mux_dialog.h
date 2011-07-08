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

#include "common/os.h"

#include <wx/dialog.h>
#include <wx/process.h>

#if defined(SYS_WINDOWS)
# include "common/fs_sys_helpers.h"
# include "mmg/taskbar_progress.h"
#endif  // SYS_WINDOWS

#define ID_B_MUX_OK                       18000
#define ID_B_MUX_SAVELOG                  18001
#define ID_B_MUX_ABORT                    18002

class mux_process;

class mux_dialog: public wxDialog {
  DECLARE_CLASS(mux_dialog);
  DECLARE_EVENT_TABLE();
protected:
  long pid;
  wxStaticText *st_label, *st_remaining_time_label, *st_remaining_time;
  wxGauge *g_progress;
  mux_process *process;
  wxString log, opt_file_name;
  wxButton *b_ok, *b_save_log, *b_abort;
  wxTextCtrl *tc_output, *tc_warnings, *tc_errors;
  wxWindowDisabler *m_window_disabler;
#if defined(SYS_WINDOWS)
  taskbar_progress_c *m_taskbar_progress;
  bool m_abort_button_changed;
#endif  // SYS_WINDOWS

  int m_exit_code, m_progress;
  int64_t m_next_remaining_time_update, m_start_time;
public:

  mux_dialog(wxWindow *parent);
  ~mux_dialog();

  void update_window(wxString text);
  void update_gauge(long value);
  void update_remaining_time();

  void on_ok(wxCommandEvent &evt);
  void on_save_log(wxCommandEvent &evt);
  void on_abort(wxCommandEvent &evt);
  void on_close(wxCloseEvent &evt);
  void done(int status);

  void set_exit_code(int exit_code) {
    m_exit_code = exit_code;
  }

#if defined(SYS_WINDOWS)
  void change_abort_button();
#endif  // SYS_WINDOWS
};

class mux_process: public wxProcess {
public:
  mux_dialog *dlg;

  mux_process(mux_dialog *mdlg);
  virtual void OnTerminate(int terminated_pid, int status);
};

#endif // __MUX_DIALOG_H
