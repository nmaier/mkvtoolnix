/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  jobs.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <root@sirelvis.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief job queue management dialog
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include "os.h"

#include <errno.h>
#include <unistd.h>

#include "wx/wxprec.h"

#include "wx/wx.h"
#include "wx/clipbrd.h"
#include "wx/confbase.h"
#include "wx/file.h"
#include "wx/fileconf.h"
#include "wx/listctrl.h"
#include "wx/notebook.h"
#include "wx/statusbr.h"
#include "wx/statline.h"

#if defined(SYS_WINDOWS)
#include <windef.h>
#include <winbase.h>
#include <io.h>
#endif

#include "common.h"
#include "jobs.h"
#include "mm_io.h"
#include "mmg.h"
#include "mmg_dialog.h"
#include "mux_dialog.h"

#define JOB_LOG_DIALOG_WIDTH 600
#define JOB_RUN_DIALOG_WIDTH 500

job_run_dialog::job_run_dialog(wxWindow *parent,
                               vector<int> &njobs_to_start):
  wxDialog(parent, -1, "mkvmerge is running", wxDefaultPosition,
#ifdef SYS_WINDOWS
           wxSize(JOB_RUN_DIALOG_WIDTH, 360),
#else
           wxSize(JOB_RUN_DIALOG_WIDTH, 285),
#endif
           wxCAPTION) {
  jobs_to_start = njobs_to_start;
  abort = false;
  current_job = -1;
  t_update = new wxTimer(this, 1);
  process = NULL;

  new wxStaticBox(this, -1, wxS("Status and progress"), wxPoint(10, 10),
                  wxSize(JOB_RUN_DIALOG_WIDTH - 20, 205));
  st_jobs = new wxStaticText(this, -1, wxS(""), wxPoint(20, 27),
                              wxSize(200, -1));
  g_jobs = new wxGauge(this, -1, jobs_to_start.size(),
                       wxPoint(JOB_RUN_DIALOG_WIDTH / 2 - 50, 27),
                       wxSize(JOB_RUN_DIALOG_WIDTH / 2 - 20 + 50, 15));
  st_current = new wxStaticText(this, -1, wxS(""), wxPoint(20, 55),
                                wxSize(100, -1));
  g_progress = new wxGauge(this, -1, 100, 
                           wxPoint(JOB_RUN_DIALOG_WIDTH / 2 - 50, 55),
                           wxSize(JOB_RUN_DIALOG_WIDTH / 2 - 20 + 50, 15));
  new wxStaticText(this, -1, wxS("Log output:"), wxPoint(20, 83));
  tc_log = new wxTextCtrl(this, -1, wxS(""), wxPoint(20, 105),
                          wxSize(JOB_RUN_DIALOG_WIDTH - 40, 100),
                          wxTE_MULTILINE | wxTE_READONLY);
  cb_abort_after_current =
    new wxCheckBox(this, -1, wxS("Abort after current job"),
                   wxPoint(20, 220), wxSize(200, -1));
  cb_abort_after_current->SetToolTip(wxS("Abort processing after the current "
                                         "job"));

  b_ok = new wxButton(this, wxID_OK, wxS("&Ok"),
                      wxPoint(JOB_RUN_DIALOG_WIDTH / 2 - 40 - 80, 250),
                      wxSize(80, -1));
  b_abort = new wxButton(this, ID_JOBS_B_ABORT, wxS("&Abort"),
                         wxPoint(JOB_RUN_DIALOG_WIDTH / 2 + 40, 250));
  b_abort->SetToolTip(wxS("Abort the muxing process right now"));
  b_ok->Enable(false);

  start_next_job();

  ShowModal();
}

void
job_run_dialog::start_next_job() {
  wxString tmp;
  wxArrayString *arg_list;
  char *arg_utf8;
  mm_io_c *opt_file;
  uint32_t i, ndx;

  t_update->Stop();

  current_job++;

  if ((current_job >= jobs_to_start.size()) ||
      cb_abort_after_current->IsChecked() || abort) {
    if (abort ||
        (cb_abort_after_current->IsChecked() &&
         (current_job < jobs_to_start.size())))
      add_to_log(wxString::Format(wxS("Aborted processing on %s"),
                                  format_date_time(time(NULL)).c_str()));
    else
      add_to_log(wxString::Format(wxS("Finished processing on %s"),
                                  format_date_time(time(NULL)).c_str()));
    b_abort->Enable(false);
    cb_abort_after_current->Enable(false);
    b_ok->Enable(true);
    b_ok->SetFocus();

    return;
  }

  ndx = jobs_to_start[current_job];
  st_jobs->SetLabel(wxString::Format(wxS("Processing job %d/%d"),
                                     current_job + 1,
                                     jobs_to_start.size()));
  st_current->SetLabel(wxString::Format(wxS("Current job ID %d:"),
                                        jobs[ndx].id));

  mdlg->load(wxString::Format(wxS("%s/jobs/%d.mmg"), wxGetCwd().c_str(),
                              jobs[ndx].id));

#if defined(SYS_WINDOWS)
  opt_file_name.Printf("mmg-mkvmerge-options-%d-%d",
                       (int)GetCurrentProcessId(), (int)time(NULL));
#else
  opt_file_name.Printf("mmg-mkvmerge-options-%d-%d", getpid(),
                       (int)time(NULL));
#endif
  try {
    opt_file = new mm_io_c(opt_file_name.c_str(), MODE_CREATE);
  } catch (...) {
    jobs[ndx].log->Printf("Could not create a temporary file for mkvmerge's "
                          "command line option called '%s' (error code %d, "
                          "%s).", opt_file_name.c_str(), errno,
                          strerror(errno));
    jobs[ndx].status = jobs_failed;
    mdlg->save_job_queue();
    if (process != NULL) {
      delete process;
      process = NULL;
    }
    start_next_job();
    return;
  }

  opt_file->write_bom("UTF-8");

  mdlg->update_command_line();
  arg_list = &mdlg->get_command_line_args();
  for (i = 1; i < arg_list->Count(); i++) {
    if ((*arg_list)[i].Length() == 0)
      opt_file->puts_unl("#EMPTY#");
    else {
      arg_utf8 = to_utf8(cc_local_utf8, (*arg_list)[i].c_str());
      opt_file->puts_unl(arg_utf8);
      safefree(arg_utf8);
    }
    opt_file->puts_unl("\n");
  }
  delete opt_file;

  process = new wxProcess(this, 1);
  process->Redirect();
  pid = wxExecute((*arg_list)[0] + " @" + opt_file_name, wxEXEC_ASYNC,
                  process);
  out = process->GetInputStream();

  *jobs[ndx].log = wxS("");
  jobs[ndx].started_on = time(NULL);
  jobs[ndx].finished_on = -1;

  add_to_log(wxString::Format(wxS("Starting job ID %d (%s) on %s"),
                              jobs[ndx].id, jobs[ndx].description->c_str(),
                              format_date_time(jobs[ndx].started_on).c_str()));

  line = wxS("");
  t_update->Start(100);
}

void
job_run_dialog::process_input() {
  wxString tmp;
  bool got_char;
  long value;
  wxChar c;

  if (process == NULL)
    return;

  while (process->IsInputAvailable()) {
    if (!out->Eof()) {
      c = out->GetC();
      got_char = true;
    } else
      got_char = false;

    if (got_char && ((c == wxC('\n')) || (c == wxC('\r')) || out->Eof())) {
      if (line.Find(wxS("progress")) == 0) {
        if (line.Find(wxS("%)")) != 0) {
          line.Remove(line.Find(wxS("%)")));
          tmp = line.AfterLast(wxC('('));
          tmp.ToLong(&value);
          if ((value >= 0) && (value <= 100))
            g_progress->SetValue(value);
        }
      } else if (line.Length() > 0)
        *jobs[jobs_to_start[current_job]].log += line + wxS("\n");
      line = wxS("");
    } else if ((unsigned char)c != 0xff)
      line.Append(c);

    if (out->Eof())
      break;
  }
}

void
job_run_dialog::on_timer(wxTimerEvent &evt) {
  process_input();
}

void
job_run_dialog::on_idle(wxIdleEvent &evt) {
  process_input();
}

void
job_run_dialog::on_abort(wxCommandEvent &evt) {
  abort = true;
#if defined(SYS_WINDOWS)
  wxKill(pid, wxSIGKILL);
#else
  wxKill(pid, wxSIGTERM);
#endif
}

void
job_run_dialog::on_end_process(wxProcessEvent &evt) {
  int exit_code, ndx;
  wxString s;
  const char *status;

  ndx = jobs_to_start[current_job];
  exit_code = evt.GetExitCode();
  if (abort) {
    jobs[ndx].status = jobs_aborted;
    status = "aborted";
  } else if (exit_code == 0) {
    jobs[ndx].status = jobs_done;
    status = "completed OK";
  } else if (exit_code == 1) {
    jobs[ndx].status = jobs_done_warnings;
    status = "completed with warnings";
  } else {
    jobs[ndx].status = jobs_failed;
    status = "failed";
  }
  jobs[ndx].finished_on = time(NULL);

  add_to_log(wxString::Format(wxS("Finished job ID %d on %s: status '%s'"),
                              jobs[ndx].id,
                              format_date_time(jobs[ndx].finished_on).c_str(),
                              status));

  mdlg->save_job_queue();
  delete process;
  process = NULL;
  out = NULL;
  wxRemoveFile(opt_file_name);

  if (!abort)
    g_jobs->SetValue(current_job + 1);

  start_next_job();
}

void
job_run_dialog::add_to_log(wxString text) {
  if (tc_log->GetValue().length() > 0)
    tc_log->AppendText(wxS("\n"));
  tc_log->AppendText(text);
  tc_log->ShowPosition(tc_log->GetValue().length());
}

// ---------------------------------------------------

job_log_dialog::job_log_dialog(wxWindow *parent,
                               wxString &log):
  wxDialog(parent, -1, "Job output", wxDefaultPosition,
           wxSize(JOB_LOG_DIALOG_WIDTH, 400), wxCAPTION) {
  wxButton *b_ok;

  new wxStaticText(this, -1, wxS("Output of the selected jobs:"),
                   wxPoint(20, 20));
  new wxTextCtrl(this, -1, log, wxPoint(20, 40),
                 wxSize(JOB_LOG_DIALOG_WIDTH - 40, 300),
                 wxTE_MULTILINE | wxTE_READONLY);
  b_ok = new wxButton(this, wxID_OK, wxS("&Ok"),
                      wxPoint(JOB_LOG_DIALOG_WIDTH / 2 - 40 - 80, 360),
                      wxSize(80, -1));
  b_ok->SetDefault();
  new wxButton(this, ID_JOBS_B_SAVE_LOG, wxS("&Save"),
               wxPoint(JOB_LOG_DIALOG_WIDTH / 2 + 40, 360),
               wxSize(80, -1));
  text = log;

  ShowModal();
}

void
job_log_dialog::on_save(wxCommandEvent &evt) {
  wxFileDialog dialog(NULL, "Choose an output file", last_open_dir, "",
                   wxS("Text files (*.txt)|*.txt|" ALLFILES),
                   wxSAVE | wxOVERWRITE_PROMPT);
  if(dialog.ShowModal() == wxID_OK) {
    wxFile file(dialog.GetPath(), wxFile::write);
    file.Write(text);
    last_open_dir = dialog.GetDirectory();
  }
}

// ---------------------------------------------------

job_dialog::job_dialog(wxWindow *parent):
  wxDialog(parent, -1, "Job queue management", wxDefaultPosition,
#ifdef SYS_WINDOWS
           wxSize(800, 455),
#else
           wxSize(800, 395),
#endif
           wxCAPTION) {
  wxListItem item;
  int i;
  long dummy;

  new wxStaticText(this, -1, wxS("Current and past jobs:"), wxPoint(20, 20));
  lv_jobs =
    new wxListView(this, ID_JOBS_LV_JOBS, wxPoint(20, 40), wxSize(660, 300),
                   wxLC_REPORT | wxSUNKEN_BORDER);

  item.m_mask = wxLIST_MASK_TEXT;
  item.m_text = wxS("ID");
  item.m_image = -1;
  lv_jobs->InsertColumn(0, item);
  item.m_text = wxS("Status");
  lv_jobs->InsertColumn(1, item);
  item.m_text = wxS("Added on");
  lv_jobs->InsertColumn(2, item);
  item.m_text = wxS("Started on");
  lv_jobs->InsertColumn(3, item);
  item.m_text = wxS("Finished on");
  lv_jobs->InsertColumn(4, item);
  item.m_text = wxS("Description");
  lv_jobs->InsertColumn(5, item);

  if (jobs.size() == 0) {
    dummy = lv_jobs->InsertItem(0, wxS("12345"));
    lv_jobs->SetItem(dummy, 1, wxS("aborted"));
    lv_jobs->SetItem(dummy, 2, wxS("2004-05-06 07:08:09"));
    lv_jobs->SetItem(dummy, 3, wxS("2004-05-06 07:08:09"));
    lv_jobs->SetItem(dummy, 4, wxS("2004-05-06 07:08:09"));
    lv_jobs->SetItem(dummy, 5, wxS("2004-05-06 07:08:09"));

  } else
    for (i = 0; i < jobs.size(); i++)
      create_list_item(i);

  for (i = 0; i < 6; i++)
    lv_jobs->SetColumnWidth(i, wxLIST_AUTOSIZE);

  if (jobs.size() == 0)
    lv_jobs->DeleteItem(0);

  b_up = new wxButton(this, ID_JOBS_B_UP, wxS("&Up"), wxPoint(700, 40),
                      wxSize(80, -1));
  b_up->SetToolTip(wxS("Move the selected job(s) up"));
  b_down = new wxButton(this, ID_JOBS_B_DOWN, wxS("&Down"), wxPoint(700, 70),
                        wxSize(80, -1));
  b_down->SetToolTip(wxS("Move the selected job(s) down"));
  b_reenable = new wxButton(this, ID_JOBS_B_REENABLE, wxS("&Re-enable"),
                            wxPoint(700, 115), wxSize(80, -1));
  b_reenable->SetToolTip(wxS("Re-enable the selected job(s)"));
  b_disable = new wxButton(this, ID_JOBS_B_DISABLE, wxS("&Disable"),
                            wxPoint(700, 145), wxSize(80, -1));
  b_disable->SetToolTip(wxS("Disable the selected job(s) and sets their "
                            "status to 'done'"));
  b_delete = new wxButton(this, ID_JOBS_B_DELETE, wxS("D&elete"),
                          wxPoint(700, 190), wxSize(80, -1));
  b_delete->SetToolTip(wxS("Delete the selected job(s) from the job queue"));
  b_view_log = new wxButton(this, ID_JOBS_B_VIEW_LOG, wxS("&View log"),
                            wxPoint(700, 235), wxSize(80, -1));
  b_view_log->SetToolTip(wxS("View the output that mkvmerge generated during "
                             "the muxing process for the selected job(s)"));

  b_ok = new wxButton(this, wxID_OK, wxS("&Ok"), wxPoint(20, 355),
                      wxSize(100, -1));
  b_ok->SetDefault();
  b_start = new wxButton(this, ID_JOBS_B_START, wxS("&Start"),
                         wxPoint(460, 355), wxSize(100, -1));
  b_start->SetToolTip(wxS("Start the jobs whose status is 'pending'"));
  b_start_selected = new wxButton(this, ID_JOBS_B_START_SELECTED,
                                  wxS("S&tart selected"),
                                  wxPoint(580, 355), wxSize(100, -1));
  b_start_selected->SetToolTip(wxS("Start the selected job(s) regardless of "
                                   "their status"));

  enable_buttons(false);

  b_start->Enable(jobs.size() > 0);

  ShowModal();
}

void
job_dialog::create_list_item(int i) {
  wxString s;
  long dummy;

  s.Printf(wxS("%d"), jobs[i].id);
  dummy = lv_jobs->InsertItem(i, s);

  s.Printf(wxS("%s"),
           jobs[i].status == jobs_pending ? wxS("pending") :
           jobs[i].status == jobs_done ? wxS("done") :
           jobs[i].status == jobs_done_warnings ? wxS("done/warnings") :
           jobs[i].status == jobs_aborted ? wxS("aborted") :
           wxS("failed"));
  lv_jobs->SetItem(dummy, 1, s);

  lv_jobs->SetItem(dummy, 2, format_date_time(jobs[i].added_on));

  if (jobs[i].started_on != -1)
    s = format_date_time(jobs[i].started_on);
  else
    s = wxS("                   ");
  lv_jobs->SetItem(dummy, 3, s);

  if (jobs[i].finished_on != -1)
    s = format_date_time(jobs[i].finished_on);
  else
    s = wxS("                   ");
  lv_jobs->SetItem(dummy, 4, s);

  s = *jobs[i].description;
  while (s.length() < 15)
    s += wxS(" ");
  lv_jobs->SetItem(dummy, 5, s);
}

void
job_dialog::enable_buttons(bool enable) {
  b_up->Enable(enable);
  b_down->Enable(enable);
  b_reenable->Enable(enable);
  b_disable->Enable(enable);
  b_delete->Enable(enable);
  b_start_selected->Enable(enable);
  b_view_log->Enable(enable);
}

void
job_dialog::on_start(wxCommandEvent &evt) {
  int i;
  vector<int> jobs_to_start;

  for (i = 0; i < jobs.size(); i++)
    if (jobs[i].status == jobs_pending)
      jobs_to_start.push_back(i);
  if (jobs_to_start.size() == 0)
    return;

  start_jobs(jobs_to_start);
}

void
job_dialog::on_start_selected(wxCommandEvent &evt) {
  int i;
  vector<int> jobs_to_start;

  for (i = 0; i < jobs.size(); i++)
    if (lv_jobs->IsSelected(i))
      jobs_to_start.push_back(i);
  if (jobs_to_start.size() == 0)
    return;

  start_jobs(jobs_to_start);
}

void
job_dialog::on_delete(wxCommandEvent &evt) {
  int i;
  vector<job_t>::iterator dit;

  dit = jobs.begin();
  i = 0;
  while (i < jobs.size()) {
    if (lv_jobs->IsSelected(i)) {
      jobs.erase(dit);
      lv_jobs->DeleteItem(i);
    } else {
      dit++;
      i++;
    }
  }

  mdlg->save_job_queue();
}

void
job_dialog::swap_rows(int lower, int higher) {
  job_t tmp_job;
  int tmp_i;

  if ((lower == higher) || (lower < 0) || (higher < 0) ||
      (lower >= jobs.size()) || (higher >= jobs.size()))
    return;
  if (lower > higher) {
    tmp_i = lower;
    lower = higher;
    higher = tmp_i;
  }
  tmp_job = jobs[lower];
  jobs[lower] = jobs[higher];
  jobs[higher] = tmp_job;

  lv_jobs->DeleteItem(higher);
  create_list_item(lower);
}

void
job_dialog::on_up(wxCommandEvent &evt) {
  int i;

  i = 0;
  while ((i < jobs.size()) && lv_jobs->IsSelected(i))
    i++;
  for (; i < jobs.size(); i++)
    if (lv_jobs->IsSelected(i)) {
      swap_rows(i - 1, i);
      lv_jobs->Select(i - 1, true);
      lv_jobs->Select(i, false);
    }

  mdlg->save_job_queue();
}

void
job_dialog::on_down(wxCommandEvent &evt) {
  int i;

  i = jobs.size() - 1;
  while ((i >= 0) && lv_jobs->IsSelected(i))
    i--;
  for (; i >= 0; i--)
    if (lv_jobs->IsSelected(i)) {
      swap_rows(i + 1, i);
      lv_jobs->Select(i + 1, true);
      lv_jobs->Select(i, false);
    }

  mdlg->save_job_queue();
}

void
job_dialog::on_reenable(wxCommandEvent &evt) {
  long item;

  item = -1;
  while (true) {
    item = lv_jobs->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item == -1)
      break;
    jobs[item].status = jobs_pending;
    lv_jobs->SetItem(item, 1, wxS("pending"));
  }

  mdlg->save_job_queue();
}

void
job_dialog::on_disable(wxCommandEvent &evt) {
  long item;

  item = -1;
  while (true) {
    item = lv_jobs->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (item == -1)
      break;
    jobs[item].status = jobs_done;
    lv_jobs->SetItem(item, 1, wxS("done"));
  }

  mdlg->save_job_queue();
}

void
job_dialog::on_view_log(wxCommandEvent &evt) {
  job_log_dialog *dialog;
  wxString log;
  int i;

  for (i = 0; i < jobs.size(); i++) {
    if (!lv_jobs->IsSelected(i))
      continue;
    log +=
      wxString::Format(wxS("--- BEGIN job %d (%s, added on %s)"),
                       jobs[i].id, jobs[i].description->c_str(),
                       format_date_time(jobs[i].added_on).c_str());
    if (jobs[i].started_on != -1)
      log += wxString::Format(wxS(", started on %s"),
                              format_date_time(jobs[i].started_on).c_str());
    log += wxS("\n");
    if (jobs[i].log->size() == 0)
      log += wxS("--- No job output found.\n");
    else
      log += *jobs[i].log;
    if (log.Last() != wxC('\n'))
      log += wxS("\n");
    log += wxString::Format(wxS("--- END job %d"), jobs[i].id);
    if (jobs[i].finished_on != -1)
      log += wxString::Format(wxS(", finished on %s"),
                              format_date_time(jobs[i].finished_on).c_str());
    log += wxS("\n\n");
  }

  if (log.length() == 0)
    return;
  dialog = new job_log_dialog(this, log);
  delete dialog;
}

void
job_dialog::on_item_selected(wxCommandEvent &evt) {
  enable_buttons(lv_jobs->GetSelectedItemCount() > 0);
}

void
job_dialog::start_jobs(vector<int> &jobs_to_start) {
  wxString temp_settings;
  job_run_dialog *dlg;
  int i;

  temp_settings.Printf(wxS("%s/jobs/temp.mmg"), wxGetCwd().c_str());
  mdlg->save(temp_settings, true);

  dlg = new job_run_dialog(this, jobs_to_start);
  delete dlg;

  mdlg->load(temp_settings, true);
  wxRemoveFile(temp_settings);

  for (i = 0; i < jobs_to_start.size(); i++) {
    lv_jobs->DeleteItem(jobs_to_start[i]);
    create_list_item(jobs_to_start[i]);
  }
}

IMPLEMENT_CLASS(job_run_dialog, wxDialog);
BEGIN_EVENT_TABLE(job_run_dialog, wxDialog)
  EVT_BUTTON(ID_JOBS_B_ABORT, job_run_dialog::on_abort)
  EVT_END_PROCESS(1, job_run_dialog::on_end_process)
  EVT_IDLE(job_run_dialog::on_idle)
  EVT_TIMER(1, job_run_dialog::on_timer)
END_EVENT_TABLE();

IMPLEMENT_CLASS(job_log_dialog, wxDialog);
BEGIN_EVENT_TABLE(job_log_dialog, wxDialog)
  EVT_BUTTON(ID_JOBS_B_SAVE_LOG, job_log_dialog::on_save)
END_EVENT_TABLE();

IMPLEMENT_CLASS(job_dialog, wxDialog);
BEGIN_EVENT_TABLE(job_dialog, wxDialog)
  EVT_BUTTON(ID_JOBS_B_START, job_dialog::on_start)
  EVT_BUTTON(ID_JOBS_B_START_SELECTED, job_dialog::on_start_selected)
  EVT_BUTTON(ID_JOBS_B_UP, job_dialog::on_up)
  EVT_BUTTON(ID_JOBS_B_DOWN, job_dialog::on_down)
  EVT_BUTTON(ID_JOBS_B_DELETE, job_dialog::on_delete)
  EVT_BUTTON(ID_JOBS_B_REENABLE, job_dialog::on_reenable) 
  EVT_BUTTON(ID_JOBS_B_DISABLE, job_dialog::on_disable)
  EVT_BUTTON(ID_JOBS_B_VIEW_LOG, job_dialog::on_view_log)
  EVT_LIST_ITEM_SELECTED(ID_JOBS_LV_JOBS, job_dialog::on_item_selected)
  EVT_LIST_ITEM_DESELECTED(ID_JOBS_LV_JOBS, job_dialog::on_item_selected)
END_EVENT_TABLE();
