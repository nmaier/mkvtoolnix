/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   declaration for the job queue

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __JOBS_H
#define __JOBS_H

#include "common/common_pch.h"

#include <wx/dialog.h>
#include <wx/listctrl.h>
#include <wx/process.h>

#include "mmg/taskbar_progress.h"

#define ID_JOBS_B_UP                      17001
#define ID_JOBS_B_DOWN                    17002
#define ID_JOBS_B_DELETE                  17003
#define ID_JOBS_B_START                   17004
#define ID_JOBS_B_ABORT                   17005
#define ID_JOBS_B_ABORT_AFTER_CURRENT     17006
#define ID_JOBS_B_REENABLE                17007
#define ID_JOBS_LV_JOBS                   17008
#define ID_JOBS_B_START_SELECTED          17009
#define ID_JOBS_B_VIEW_LOG                17010
#define ID_JOBS_B_SAVE_LOG                17011
#define ID_JOBS_B_DISABLE                 17012

enum job_status_e {
  JOBS_PENDING,
  JOBS_DONE,
  JOBS_DONE_WARNINGS,
  JOBS_ABORTED,
  JOBS_FAILED
};

struct job_t {
  int id;
  job_status_e status;
  int added_on, started_on, finished_on;
  wxString *description, *log;
};

extern std::vector<job_t> jobs;

class job_log_dialog: public wxDialog {
  DECLARE_CLASS(job_log_dialog);
  DECLARE_EVENT_TABLE();
protected:
  wxString text;

public:
  job_log_dialog(wxWindow *parent, wxString &log);

  void on_save(wxCommandEvent &evt);
};

class job_run_dialog: public wxDialog {
  DECLARE_CLASS(job_run_dialog);
  DECLARE_EVENT_TABLE();
protected:
  wxGauge *g_jobs, *g_progress;
  wxStaticText *st_jobs, *st_current, *st_remaining_time_total, *st_remaining_time;
  wxButton *b_ok, *b_abort;
  wxCheckBox *cb_abort_after_current;
  wxTextCtrl *tc_log;

  wxTimer *t_update;
  wxInputStream *out;
  std::string line;
  wxProcess *process;
  wxString opt_file_name;
  bool abort;
  long pid;
  std::vector<int> jobs_to_start;
  int current_job;

  int m_progress;
  int64_t m_next_remaining_time_update, m_next_remaining_time_update_total, m_start_time, m_start_time_total;

#if defined(SYS_WINDOWS)
  taskbar_progress_c *m_taskbar_progress;
#endif

public:
  job_run_dialog(wxWindow *parent, std::vector<int> &n_jobs_to_start);

  void on_abort(wxCommandEvent &evt);
  void on_end_process(wxProcessEvent &evt);
  void on_timer(wxTimerEvent &evt);
  void on_idle(wxIdleEvent &evt);

  void start_next_job();
  void process_input();
  void add_to_log(wxString text);
  void set_progress_value(long value);
  void update_remaining_time();
};

class jobdlg_list_view: public wxListView {
  DECLARE_CLASS(jobdlg_list_view);
  DECLARE_EVENT_TABLE();
public:
  jobdlg_list_view(wxWindow *parent, wxWindowID id);

  void on_key_pressed(wxKeyEvent &evt);
};

class job_dialog: public wxDialog {
  DECLARE_CLASS(job_dialog);
  DECLARE_EVENT_TABLE();
protected:
  jobdlg_list_view *lv_jobs;
  wxButton *b_ok, *b_up, *b_down, *b_abort, *b_abort_after_current, *b_delete;
  wxButton *b_start, *b_start_selected, *b_reenable, *b_view_log, *b_disable;

public:
  job_dialog(wxWindow *parent);

  void on_start(wxCommandEvent &evt);
  void on_start_selected(wxCommandEvent &evt);
  void on_reenable(wxCommandEvent &evt);
  void on_disable(wxCommandEvent &evt);
  void on_up(wxCommandEvent &evt);
  void on_down(wxCommandEvent &evt);
  void on_delete(wxCommandEvent &evt);
  void on_view_log(wxCommandEvent &evt);
  void on_item_selected(wxListEvent &evt);
  void on_key_pressed(wxKeyEvent &evt);

  void enable_buttons(bool enable, bool enable_up_down = true);
  void swap_rows(unsigned int lower, unsigned int higher, bool up);
  void create_list_item(int i);
  void start_jobs(std::vector<int> &jobs_to_start);
};

#endif // __JOBS_H
