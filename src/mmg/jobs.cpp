/*
 * mkvmerge GUI -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * job queue management dialog
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include "os.h"

#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

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
#include "mmg.h"
#include "mmg_dialog.h"
#include "mux_dialog.h"

#define JOB_LOG_DIALOG_WIDTH 600
#define JOB_RUN_DIALOG_WIDTH 500

job_run_dialog::job_run_dialog(wxWindow *parent,
                               vector<int> &njobs_to_start):
  wxDialog(parent, -1, wxT("mkvmerge is running"), wxDefaultPosition,
           wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER |
           wxMINIMIZE_BOX | wxMAXIMIZE_BOX) {
  wxStaticBoxSizer *siz_sb;
  wxBoxSizer *siz_line, *siz_all;
  wxFlexGridSizer *siz_fg;
  jobs_to_start = njobs_to_start;
  abort = false;
  current_job = -1;
  t_update = new wxTimer(this, 1);
  process = NULL;

  siz_all = new wxBoxSizer(wxVERTICAL);
  siz_sb = new wxStaticBoxSizer(new wxStaticBox(this, -1,
                                                wxT("Status and progress")),
                                wxVERTICAL);

  siz_fg = new wxFlexGridSizer(2);
  siz_fg->AddGrowableCol(1);
  st_jobs = new wxStaticText(this, -1, wxT("Processing 1000/1000"));
  siz_fg->Add(st_jobs, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT | wxBOTTOM, 10);
  g_jobs = new wxGauge(this, -1, jobs_to_start.size());
  siz_fg->Add(g_jobs, 1, wxALIGN_CENTER_VERTICAL | wxGROW | wxBOTTOM, 10);

  st_current = new wxStaticText(this, -1, wxT("Current job ID 1000:"));
  siz_fg->Add(st_current, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 10);
  g_progress = new wxGauge(this, -1, 100);
  siz_fg->Add(g_progress, 1, wxALIGN_CENTER_VERTICAL | wxGROW, 0);
  siz_sb->Add(siz_fg, 0, wxGROW | wxLEFT | wxRIGHT | wxTOP, 10);
  siz_sb->Add(0, 10, 0, 0, 0);

  siz_sb->Add(new wxStaticText(this, -1, wxT("Log output:")),
              wxALIGN_LEFT, wxLEFT, 10);
  siz_sb->Add(0, 5, 0, 0, 0);
  tc_log = new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition,
                          wxSize(450, 150), wxTE_MULTILINE | wxTE_READONLY);
  siz_sb->Add(tc_log, 1, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 10);
  siz_all->Add(siz_sb, 1, wxGROW | wxALL, 10);

  cb_abort_after_current =
    new wxCheckBox(this, -1, wxT("Abort after current job"));
  cb_abort_after_current->SetToolTip(TIP("Abort processing after the current "
                                         "job"));
  siz_all->Add(cb_abort_after_current, 0, wxALIGN_LEFT | wxLEFT, 10);

  b_ok = new wxButton(this, wxID_OK, wxT("&Ok"));
  b_ok->Enable(false);
  b_abort = new wxButton(this, ID_JOBS_B_ABORT, wxT("&Abort"));
  b_abort->SetToolTip(TIP("Abort the muxing process right now"));

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(1, 0, 1, wxGROW, 0);
  siz_line->Add(b_ok, 0, 0, 0);
  siz_line->Add(1, 0, 1, wxGROW, 0);
  siz_line->Add(b_abort, 0, 0, 0);
  siz_line->Add(1, 0, 1, wxGROW, 0);
  siz_all->Add(siz_line, 0, wxGROW | wxTOP | wxBOTTOM, 10);
  siz_all->SetSizeHints(this);

  SetSizer(siz_all);

  start_next_job();

  ShowModal();
}

void
job_run_dialog::start_next_job() {
  wxString tmp;
  wxArrayString *arg_list;
  wxFile *opt_file;
  string arg_utf8;
  uint32_t i, ndx;
  const unsigned char utf8_bom[3] = {0xef, 0xbb, 0xbf};

  t_update->Stop();

  current_job++;

  if ((current_job >= jobs_to_start.size()) ||
      cb_abort_after_current->IsChecked() || abort) {
    if (abort ||
        (cb_abort_after_current->IsChecked() &&
         (current_job < jobs_to_start.size())))
      add_to_log(wxString::Format(wxT("Aborted processing on %s"),
                                  format_date_time(time(NULL)).c_str()));
    else
      add_to_log(wxString::Format(wxT("Finished processing on %s"),
                                  format_date_time(time(NULL)).c_str()));
    b_abort->Enable(false);
    cb_abort_after_current->Enable(false);
    b_ok->Enable(true);
    b_ok->SetFocus();
    SetTitle(wxT("mkvmerge has finished"));

    return;
  }

  ndx = jobs_to_start[current_job];
  st_jobs->SetLabel(wxString::Format(wxT("Processing job %d/%d"),
                                     current_job + 1,
                                     jobs_to_start.size()));
  st_current->SetLabel(wxString::Format(wxT("Current job ID %d:"),
                                        jobs[ndx].id));

  mdlg->load(wxString::Format(wxT("%s/jobs/%d.mmg"), wxGetCwd().c_str(),
                              jobs[ndx].id));

#if defined(SYS_WINDOWS)
  opt_file_name.Printf(wxT("mmg-mkvmerge-options-%d-%d"),
                       (int)GetCurrentProcessId(), (int)time(NULL));
#else
  opt_file_name.Printf(wxT("mmg-mkvmerge-options-%d-%d"), getpid(),
                       (int)time(NULL));
#endif
  try {
    opt_file = new wxFile(opt_file_name, wxFile::write);
  } catch (...) {
    jobs[ndx].log->Printf(wxT("Could not create a temporary file for "
                              "mkvmerge's command line option called '%s"
                              "' (error code %d, %s)."), opt_file_name.c_str(),
                          errno, wxUCS(strerror(errno)));
    jobs[ndx].status = jobs_failed;
    mdlg->save_job_queue();
    if (process != NULL) {
      delete process;
      process = NULL;
    }
    start_next_job();
    return;
  }

  opt_file->Write(utf8_bom, 3);

  mdlg->update_command_line();
  arg_list = &mdlg->get_command_line_args();
  for (i = 1; i < arg_list->Count(); i++) {
    if ((*arg_list)[i].Length() == 0)
      opt_file->Write(wxT("#EMPTY#"));
    else {
      arg_utf8 = to_utf8((*arg_list)[i]);
      opt_file->Write(arg_utf8.c_str(), arg_utf8.length());
    }
    opt_file->Write(wxT("\n"));
  }
  delete opt_file;

  process = new wxProcess(this, 1);
  process->Redirect();
  pid = wxExecute((*arg_list)[0] + wxT(" @") + opt_file_name, wxEXEC_ASYNC,
                  process);
  out = process->GetInputStream();

  *jobs[ndx].log = wxT("");
  jobs[ndx].started_on = time(NULL);
  jobs[ndx].finished_on = -1;

  add_to_log(wxString::Format(wxT("Starting job ID %d (%s) on %s"),
                              jobs[ndx].id, jobs[ndx].description->c_str(),
                              format_date_time(jobs[ndx].started_on).c_str()));

  line = wxT("");
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

  c = wxT('\0');

  while (process->IsInputAvailable()) {
    if (!out->Eof()) {
      c = out->GetC();
      got_char = true;
    } else
      got_char = false;

    if (got_char && ((c == wxT('\n')) || (c == wxT('\r')) || out->Eof())) {
      if (line.Find(wxT("progress")) == 0) {
        if (line.Find(wxT("%)")) != 0) {
          line.Remove(line.Find(wxT("%)")));
          tmp = line.AfterLast(wxT('('));
          tmp.ToLong(&value);
          if ((value >= 0) && (value <= 100))
            g_progress->SetValue(value);
        }
      } else if (line.Length() > 0)
        *jobs[jobs_to_start[current_job]].log += line + wxT("\n");
      line = wxT("");
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

  process_input();

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

  add_to_log(wxString::Format(wxT("Finished job ID %d on %s: status '%s'"),
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
    tc_log->AppendText(wxT("\n"));
  tc_log->AppendText(text);
  tc_log->ShowPosition(tc_log->GetValue().length());
}

// ---------------------------------------------------

job_log_dialog::job_log_dialog(wxWindow *parent,
                               wxString &log):
  wxDialog(parent, -1, wxT("Job output"), wxDefaultPosition,
           wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER) {
  wxBoxSizer *siz_buttons, *siz_all;
  wxButton *b_ok;

  siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->Add(new wxStaticText(this, -1, wxT("Output of the selected jobs:")),
               0, wxALIGN_LEFT | wxLEFT | wxTOP, 10);
  siz_all->Add(0, 5, 0, 0, 0);
  siz_all->Add(new wxTextCtrl(this, -1, log, wxDefaultPosition,
                              wxSize(550, 150), wxTE_MULTILINE |
                              wxTE_READONLY),
               1, wxGROW | wxLEFT | wxRIGHT, 10);
  siz_buttons = new wxBoxSizer(wxHORIZONTAL);
  b_ok = new wxButton(this, wxID_OK, wxT("&Ok"));
  b_ok->SetDefault();
  siz_buttons->Add(1, 0, 1, wxGROW, 0);
  siz_buttons->Add(b_ok, 0, 0, 0);
  siz_buttons->Add(1, 0, 1, wxGROW, 0);
  siz_buttons->Add(new wxButton(this, ID_JOBS_B_SAVE_LOG, wxT("&Save")),
                   0, 0, 0);
  siz_buttons->Add(1, 0, 1, wxGROW, 0);
  siz_all->Add(siz_buttons, 0, wxGROW | wxTOP | wxBOTTOM, 10);
  siz_all->SetSizeHints(this);
  SetSizer(siz_all);

  text = log;

  ShowModal();
}

void
job_log_dialog::on_save(wxCommandEvent &evt) {
  wxFileDialog dialog(NULL, wxT("Choose an output file"), last_open_dir,
                      wxT(""), wxT("Text files (*.txt)|*.txt|" ALLFILES),
                      wxSAVE | wxOVERWRITE_PROMPT);
  if(dialog.ShowModal() == wxID_OK) {
    wxFile file(dialog.GetPath(), wxFile::write);
    file.Write(text);
    last_open_dir = dialog.GetDirectory();
  }
}

// ---------------------------------------------------

job_dialog::job_dialog(wxWindow *parent):
  wxDialog(parent, -1, wxT("Job queue management"), wxDefaultPosition,
           wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER |
           wxMINIMIZE_BOX | wxMAXIMIZE_BOX) {
  wxBoxSizer *siz_b_right, *siz_b_bottom, *siz_all, *siz_line;
  wxListItem item;
  int i;
  long dummy;

  siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->Add(new wxStaticText(this, -1, wxT("Current and past jobs:")),
               0, wxALIGN_LEFT | wxALL, 10);
  lv_jobs =
    new wxListView(this, ID_JOBS_LV_JOBS, wxDefaultPosition, wxSize(400, -1),
                   wxLC_REPORT | wxSUNKEN_BORDER);

  item.m_mask = wxLIST_MASK_TEXT;
  item.m_text = wxT("ID");
  item.m_image = -1;
  lv_jobs->InsertColumn(0, item);
  item.m_text = wxT("Status");
  lv_jobs->InsertColumn(1, item);
  item.m_text = wxT("Description");
  lv_jobs->InsertColumn(2, item);
  item.m_text = wxT("Added on");
  lv_jobs->InsertColumn(3, item);
  item.m_text = wxT("Started on");
  lv_jobs->InsertColumn(4, item);
  item.m_text = wxT("Finished on");
  lv_jobs->InsertColumn(5, item);

  if (jobs.size() == 0) {
    dummy = lv_jobs->InsertItem(0, wxT("12345"));
    lv_jobs->SetItem(dummy, 1, wxT("aborted"));
    lv_jobs->SetItem(dummy, 2, wxT("2004-05-06 07:08:09"));
    lv_jobs->SetItem(dummy, 3, wxT("2004-05-06 07:08:09"));
    lv_jobs->SetItem(dummy, 4, wxT("2004-05-06 07:08:09"));
    lv_jobs->SetItem(dummy, 5, wxT("2004-05-06 07:08:09"));

  } else
    for (i = 0; i < jobs.size(); i++)
      create_list_item(i);

  for (i = 0; i < 6; i++)
    lv_jobs->SetColumnWidth(i, wxLIST_AUTOSIZE);

  if (jobs.size() == 0)
    lv_jobs->DeleteItem(0);
  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(lv_jobs, 1, wxGROW | wxRIGHT, 10);

  siz_b_right = new wxBoxSizer(wxVERTICAL);
  b_up = new wxButton(this, ID_JOBS_B_UP, wxT("&Up"));
  b_up->SetToolTip(TIP("Move the selected job(s) up"));
  siz_b_right->Add(b_up, 0, wxLEFT | wxBOTTOM, 10);
  b_down = new wxButton(this, ID_JOBS_B_DOWN, wxT("&Down"));
  b_down->SetToolTip(TIP("Move the selected job(s) down"));
  siz_b_right->Add(b_down, 0, wxLEFT | wxBOTTOM, 10);
  siz_b_right->Add(0, 15, 0, 0, 0);

  b_reenable = new wxButton(this, ID_JOBS_B_REENABLE, wxT("&Re-enable"));
  b_reenable->SetToolTip(TIP("Re-enable the selected job(s)"));
  siz_b_right->Add(b_reenable, 0, wxLEFT | wxBOTTOM, 10);
  b_disable = new wxButton(this, ID_JOBS_B_DISABLE, wxT("&Disable"));
  b_disable->SetToolTip(TIP("Disable the selected job(s) and sets their "
                            "status to 'done'"));
  siz_b_right->Add(b_disable, 0, wxLEFT | wxBOTTOM, 10);
  siz_b_right->Add(0, 15, 0, 0, 0);

  b_delete = new wxButton(this, ID_JOBS_B_DELETE, wxT("D&elete"));
  b_delete->SetToolTip(TIP("Delete the selected job(s) from the job queue"));
  siz_b_right->Add(b_delete, 0, wxLEFT | wxBOTTOM, 10);
  siz_b_right->Add(0, 15, 0, 0, 0);

  b_view_log = new wxButton(this, ID_JOBS_B_VIEW_LOG, wxT("&View log"));
  b_view_log->SetToolTip(TIP("View the output that mkvmerge generated during "
                             "the muxing process for the selected job(s)"));
  siz_b_right->Add(b_view_log, 0, wxLEFT, 10);
  siz_line->Add(siz_b_right, 0, 0, 0);
  siz_all->Add(siz_line, 1, wxGROW | wxLEFT | wxRIGHT, 10);

  siz_all->Add(new wxStaticLine(this, -1), 0, wxGROW | wxALL, 10);

  siz_b_bottom = new wxBoxSizer(wxHORIZONTAL);
  b_ok = new wxButton(this, wxID_OK, wxT("&Ok"));
  b_ok->SetDefault();
  siz_b_bottom->Add(b_ok, 0, 0, 0);
  siz_b_bottom->Add(1, 0, 1, wxGROW, 0);

  b_start = new wxButton(this, ID_JOBS_B_START, wxT("&Start"));
  b_start->SetToolTip(TIP("Start the jobs whose status is 'pending'"));
  siz_b_bottom->Add(b_start, 0, wxRIGHT, 10);
  siz_b_bottom->Add(10, 0, 0, 0, 0);
  b_start_selected = new wxButton(this, ID_JOBS_B_START_SELECTED,
                                  wxT("S&tart selected"), wxDefaultPosition,
                                  wxSize(100, -1));
  b_start_selected->SetToolTip(TIP("Start the selected job(s) regardless of "
                                   "their status"));
  siz_b_bottom->Add(b_start_selected, 0, wxLEFT, 10);
  siz_all->Add(siz_b_bottom, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 10);
  siz_all->SetSizeHints(this);
  SetSizer(siz_all);

  enable_buttons(false);

  b_start->Enable(jobs.size() > 0);

  ShowModal();
}

void
job_dialog::create_list_item(int i) {
  wxString s;
  long dummy;

  s.Printf(wxT("%d"), jobs[i].id);
  dummy = lv_jobs->InsertItem(i, s);

  s.Printf(wxT("%s"),
           jobs[i].status == jobs_pending ? wxT("pending") :
           jobs[i].status == jobs_done ? wxT("done") :
           jobs[i].status == jobs_done_warnings ? wxT("done/warnings") :
           jobs[i].status == jobs_aborted ? wxT("aborted") :
           wxT("failed"));
  lv_jobs->SetItem(dummy, 1, s);

  s = *jobs[i].description;
  while (s.length() < 15)
    s += wxT(" ");
  lv_jobs->SetItem(dummy, 2, s);

  lv_jobs->SetItem(dummy, 3, format_date_time(jobs[i].added_on));

  if (jobs[i].started_on != -1)
    s = format_date_time(jobs[i].started_on);
  else
    s = wxT("                   ");
  lv_jobs->SetItem(dummy, 4, s);

  if (jobs[i].finished_on != -1)
    s = format_date_time(jobs[i].finished_on);
  else
    s = wxT("                   ");
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
  int i, j;
  vector<job_t>::iterator dit;
  vector<bool> selected;

  for (i = 0; i < lv_jobs->GetItemCount(); i++)
    selected.push_back(lv_jobs->IsSelected(i));
  dit = jobs.begin();
  i = 0;
  j =0;
  while (i < jobs.size()) {
    if (selected[j]) {
      wxRemoveFile(wxString::Format(wxT("jobs/%d.mmg"), dit->id));
      jobs.erase(dit);
      lv_jobs->DeleteItem(i);
    } else {
      dit++;
      i++;
    }
    j++;
  }

  mdlg->save_job_queue();
}

void
job_dialog::swap_rows(int lower,
                      int higher,
                      bool up) {
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

  if (up) {
    lv_jobs->DeleteItem(lower);
    create_list_item(higher);
  } else {
    lv_jobs->DeleteItem(higher);
    create_list_item(lower);
  }
}

void
job_dialog::on_up(wxCommandEvent &evt) {
  vector<bool> selected;
  bool first;
  int i, j;

  i = 0;
  first = true;
  for (j = 0; j < lv_jobs->GetItemCount(); j++) {
    selected.push_back(lv_jobs->IsSelected(j));
    if (lv_jobs->IsSelected(j) && first)
      i++;
    else
      first = false;
  }

  for (; i < jobs.size(); i++)
    if (selected[i]) {
      swap_rows(i - 1, i, true);
      selected[i - 1] = true;
      selected[i] = false;
    }
  
  mdlg->save_job_queue();
}

void
job_dialog::on_down(wxCommandEvent &evt) {
  vector<bool> selected;
  bool first;
  int i, j;

  first = true;
  i = lv_jobs->GetItemCount() - 1;
  for (j = i; j >= 0; j--) {
    selected.insert(selected.begin(), lv_jobs->IsSelected(j));
    if (lv_jobs->IsSelected(j) && first)
      i--;
    else
      first = false;
  }

  for (; i >= 0; i--)
    if (selected[i]) {
      swap_rows(i + 1, i, false);
      selected[i + 1] =  true;
      selected[i] = false;
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
    lv_jobs->SetItem(item, 1, wxT("pending"));
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
    lv_jobs->SetItem(item, 1, wxT("done"));
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
      wxString::Format(wxT("--- BEGIN job %d (%s, added on %s)"),
                       jobs[i].id, jobs[i].description->c_str(),
                       format_date_time(jobs[i].added_on).c_str());
    if (jobs[i].started_on != -1)
      log += wxString::Format(wxT(", started on %s"),
                              format_date_time(jobs[i].started_on).c_str());
    log += wxT("\n");
    if (jobs[i].log->size() == 0)
      log += wxT("--- No job output found.\n");
    else
      log += *jobs[i].log;
    if (log.Last() != wxT('\n'))
      log += wxT("\n");
    log += wxString::Format(wxT("--- END job %d"), jobs[i].id);
    if (jobs[i].finished_on != -1)
      log += wxString::Format(wxT(", finished on %s"),
                              format_date_time(jobs[i].finished_on).c_str());
    log += wxT("\n\n");
  }

  if (log.length() == 0)
    return;
  dialog = new job_log_dialog(this, log);
  delete dialog;
}

void
job_dialog::on_item_selected(wxListEvent &evt) {
  enable_buttons(lv_jobs->GetSelectedItemCount() > 0);
}

void
job_dialog::start_jobs(vector<int> &jobs_to_start) {
  wxString temp_settings;
  job_run_dialog *dlg;
  int i;

  temp_settings.Printf(wxT("%s/jobs/temp.mmg"), wxGetCwd().c_str());
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
