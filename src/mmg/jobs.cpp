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
#define JOB_RUN_DIALOG_WIDTH 400

job_run_dialog::job_run_dialog(wxWindow *parent,
                               vector<int> &jobs_to_start):
  wxDialog(parent, -1, "mkvmerge is running", wxDefaultPosition,
#ifdef SYS_WINDOWS
           wxSize(JOB_RUN_DIALOG_WIDTH, 360),
#else
           wxSize(JOB_RUN_DIALOG_WIDTH, 155),
#endif
           wxCAPTION) {
  wxStaticText *st_current;
  char c, *arg_utf8;
  long value;
  wxString line, tmp;
  wxInputStream *out;
  mm_io_c *opt_file;
  uint32_t i, job, ndx;
  wxArrayString *arg_list;

  c = 0;
  new wxStaticBox(this, -1, wxS("Status and progress"), wxPoint(10, 10),
                  wxSize(JOB_RUN_DIALOG_WIDTH - 20, 70));
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
  cb_abort_after_current =
    new wxCheckBox(this, -1, wxS("Abort after current job"),
                   wxPoint(20, 90), wxSize(200, -1));

  b_ok = new wxButton(this, wxID_OK, wxS("&Ok"),
                      wxPoint(JOB_RUN_DIALOG_WIDTH / 2 - 40 - 80, 125),
                      wxSize(80, -1));
  b_abort = new wxButton(this, ID_JOBS_B_ABORT, wxS("&Abort"),
                         wxPoint(JOB_RUN_DIALOG_WIDTH / 2 + 40, 125));
  b_ok->Enable(false);
  abort = false;

  Show(true);

  for (job = 0; job < jobs_to_start.size(); job++) {
    ndx = jobs_to_start[job];
    st_jobs->SetLabel(wxString::Format(wxS("Processing job %d/%d"), job + 1,
                                       jobs_to_start.size()));
    st_current->SetLabel(wxString::Format(wxS("Current job ID %d:"),
                                          jobs[ndx].id));
    while (app->Pending())
      app->Dispatch();
    wxSleep(1);
    g_progress->SetValue(50);
    while (app->Pending())
      app->Dispatch();
    wxSleep(1);
    g_progress->SetValue(100);
    while (app->Pending())
      app->Dispatch();

    g_jobs->SetValue(job + 1);
    if (cb_abort_after_current->IsChecked() || abort)
      break;
  }

  b_ok->Enable(true);
  b_ok->SetFocus();
  ShowModal();
}

//   update_window("Muxing in progress.");
//   Show(true);

//   process = new mux_process(this);

// #if defined(SYS_WINDOWS)
//   opt_file_name.Printf("mmg-mkvmerge-options-%d-%d",
//                        (int)GetCurrentProcessId(), (int)time(NULL));
// #else
//   opt_file_name.Printf("mmg-mkvmerge-options-%d-%d", getpid(),
//                        (int)time(NULL));
// #endif
//   try {
//     opt_file = new mm_io_c(opt_file_name.c_str(), MODE_CREATE);
//     opt_file->write_bom("UTF-8");
//   } catch (...) {
//     wxString error;
//     error.Printf("Could not create a temporary file for mkvmerge's command "
//                  "line option called '%s' (error code %d, %s).",
//                  opt_file_name.c_str(), errno, strerror(errno));
//     wxMessageBox(error, "File creation failed", wxOK | wxCENTER |
//                  wxICON_ERROR);
//     throw 0;
//   }
//   arg_list = &static_cast<mmg_dialog *>(parent)->get_command_line_args();
//   for (i = 1; i < arg_list->Count(); i++) {
//     if ((*arg_list)[i].Length() == 0)
//       opt_file->puts_unl("#EMPTY#");
//     else {
//       arg_utf8 = to_utf8(cc_local_utf8, (*arg_list)[i].c_str());
//       opt_file->puts_unl(arg_utf8);
//       safefree(arg_utf8);
//     }
//     opt_file->puts_unl("\n");
//   }
//   delete opt_file;

//   pid = wxExecute((*arg_list)[0] + " @" + opt_file_name, wxEXEC_ASYNC,
//                   process);
//   out = process->GetInputStream();

//   line = "";
//   log = "";
//   while (1) {
//     if (!out->Eof()) {
//       c = out->GetC();
//       if ((unsigned char)c != 0xff)
//         log.Append(c);
//     }
//     while (app->Pending())
//       app->Dispatch();

//     if ((c == '\n') || (c == '\r') || out->Eof()) {
//       if (line.Find("Warning:") == 0)
//         tc_warnings->AppendText(line + "\n");
//       else if (line.Find("Error:") == 0)
//         tc_errors->AppendText(line + "\n");
//       else if (line.Find("progress") == 0) {
//         if (line.Find("%)") != 0) {
//           line.Remove(line.Find("%)"));
//           tmp = line.AfterLast('(');
//           tmp.ToLong(&value);
//           if ((value >= 0) && (value <= 100))
//             update_gauge(value);
//         }
//       } else if (line.Length() > 0)
//         tc_output->AppendText(line + "\n");
//       line = "";
//     } else if ((unsigned char)c != 0xff)
//       line.Append(c);

//     if (out->Eof())
//       break;
//   }

//   b_ok->Enable(true);
//   b_abort->Enable(false);
//   b_ok->SetFocus();
//   ShowModal();
// }

// mux_dialog::~mux_dialog() {
//   delete process;
//   unlink(opt_file_name.c_str());
// }

// void mux_dialog::update_window(wxString text) {
//   st_jobs->SetLabel(text);
// }

// void mux_dialog::update_gauge(long value) {
//   g_progress->SetValue(value);
// }

// void mux_dialog::on_ok(wxCommandEvent &evt) {
//   Close(true);
// }

// void mux_dialog::on_save_log(wxCommandEvent &evt) {
//   wxFile *file;
//   wxString s;
//   wxFileDialog dlg(NULL, "Choose an output file", last_open_dir, "",
//                    _T("Log files (*.txt)|*.txt|" ALLFILES),
//                    wxSAVE | wxOVERWRITE_PROMPT);
//   if(dlg.ShowModal() == wxID_OK) {
//     last_open_dir = dlg.GetDirectory();
//     file = new wxFile(dlg.GetPath(), wxFile::write);
//     s = log + "\n";
//     file->Write(s);
//     delete file;
//   }
// }

void
job_run_dialog::on_abort(wxCommandEvent &evt) {
  abort = true;
// #if defined(SYS_WINDOWS)
//   wxKill(pid, wxSIGKILL);
// #else
//   wxKill(pid, wxSIGTERM);
// #endif
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
  b_down = new wxButton(this, ID_JOBS_B_DOWN, wxS("&Down"), wxPoint(700, 70),
                        wxSize(80, -1));
  b_reenable = new wxButton(this, ID_JOBS_B_REENABLE, wxS("&Re-enable"),
                            wxPoint(700, 115), wxSize(80, -1));
  b_delete = new wxButton(this, ID_JOBS_B_DELETE, wxS("D&elete"),
                          wxPoint(700, 150), wxSize(80, -1));
  b_view_log = new wxButton(this, ID_JOBS_B_VIEW_LOG, wxS("&View log"),
                            wxPoint(700, 195), wxSize(80, -1));

  b_ok = new wxButton(this, wxID_OK, wxS("&Ok"), wxPoint(20, 355),
                      wxSize(100, -1));
  b_ok->SetDefault();
  b_start = new wxButton(this, ID_JOBS_B_START, wxS("&Start"),
                         wxPoint(460, 355), wxSize(100, -1));
  b_start_selected = new wxButton(this, ID_JOBS_B_START_SELECTED,
                                  wxS("S&tart selected"),
                                  wxPoint(580, 355), wxSize(100, -1));

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
           jobs[i].status == jobs_done_warnings ? wxS("done_warnings") :
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
job_dialog::on_view_log(wxCommandEvent &evt) {
  job_log_dialog *dialog;
  wxString log;
  int i;

  for (i = 0; i < jobs.size(); i++) {
    if (!lv_jobs->IsSelected(i))
      continue;
    log +=
      wxString::Format(wxS("--- BEGIN job %d (%s), added on %s\n"), jobs[i].id,
                       jobs[i].description->c_str(),
                       format_date_time(jobs[i].added_on).c_str());
    if (jobs[i].log->size() == 0)
      log += wxS("--- No job output found.\n");
    else
      log += *jobs[i].log;
    if (log.Last() != wxC('\n'))
      log += wxS("\n");
    log += wxString::Format(wxS("--- END job %d\n\n"), jobs[i].id);
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
}

IMPLEMENT_CLASS(job_run_dialog, wxDialog);
BEGIN_EVENT_TABLE(job_run_dialog, wxDialog)
  EVT_BUTTON(ID_JOBS_B_ABORT, job_run_dialog::on_abort)
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
  EVT_BUTTON(ID_JOBS_B_VIEW_LOG, job_dialog::on_view_log)
  EVT_LIST_ITEM_SELECTED(ID_JOBS_LV_JOBS, job_dialog::on_item_selected)
  EVT_LIST_ITEM_DESELECTED(ID_JOBS_LV_JOBS, job_dialog::on_item_selected)
END_EVENT_TABLE();
