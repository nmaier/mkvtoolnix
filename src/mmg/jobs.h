/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  jobs.h

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <flo.wagner@gmx.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief declaration for the job queue
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __JOBS_H
#define __JOBS_H

#include "wx/dialog.h"
#include "wx/listctrl.h"
#include "wx/process.h"
#include "wx/thread.h"

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

enum job_status_t {
  jobs_pending,
  jobs_done,
  jobs_done_warnings,
  jobs_aborted,
  jobs_failed
};

typedef struct {
  int32_t id;
  job_status_t status;
  int32_t added_on, started_on, finished_on;
  wxString *description, *log;
} job_t;

extern vector<job_t> jobs;

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
  bool abort;
  long pid, exit_code;
  wxSemaphore *lock;

public:
  job_run_dialog(wxWindow *parent, vector<int> &jobs_to_start);

  void on_abort(wxCommandEvent &evt);
  void on_end_process(wxProcessEvent &evt);
};

class job_dialog: public wxDialog {
  DECLARE_CLASS(job_dialog);
  DECLARE_EVENT_TABLE();
protected:
  wxListView *lv_jobs;
  wxButton *b_ok, *b_up, *b_down, *b_abort, *b_abort_after_current, *b_delete;
  wxButton *b_start, *b_start_selected, *b_reenable, *b_view_log;

public:
  job_dialog(wxWindow *parent);

  void on_start(wxCommandEvent &evt);
  void on_start_selected(wxCommandEvent &evt);
  void on_reenable(wxCommandEvent &evt);
  void on_up(wxCommandEvent &evt);
  void on_down(wxCommandEvent &evt);
  void on_delete(wxCommandEvent &evt);
  void on_view_log(wxCommandEvent &evt);
  void on_item_selected(wxCommandEvent &evt);

  void enable_buttons(bool enable);
  void swap_rows(int lower, int higher);
  void create_list_item(int i);
  void start_jobs(vector<int> &jobs_to_start);
}; 

#endif // __JOBS_H
