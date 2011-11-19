/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   job queue management dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <wx/wxprec.h>

#include <wx/wx.h>
#include <wx/clipbrd.h>
#include <wx/confbase.h>
#include <wx/file.h>
#include <wx/fileconf.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/statusbr.h>
#include <wx/statline.h>

#if defined(SYS_WINDOWS)
# include <windows.h>
#endif

#include "common/common_pch.h"
#include "common/fs_sys_helpers.h"
#include "common/strings/formatting.h"
#include "mmg/jobs.h"
#include "mmg/mmg.h"
#include "mmg/mmg_dialog.h"
#include "mmg/taskbar_progress.h"

#define JOB_LOG_DIALOG_WIDTH 600
#define JOB_RUN_DIALOG_WIDTH 500

job_run_dialog::job_run_dialog(wxWindow *,
                               std::vector<int> &n_jobs_to_start)
  : wxDialog(NULL, -1, Z("mkvmerge is running"), wxDefaultPosition, wxSize(400, 700), wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX | wxMAXIMIZE_BOX)
  , t_update(new wxTimer(this, 1))
  , out(NULL)
  , process(NULL)
  , abort(false)
  , jobs_to_start(n_jobs_to_start)
  , current_job(-1)
  , m_progress(0)
#if defined(SYS_WINDOWS)
  , m_taskbar_progress(NULL)
#endif
{
  wxBoxSizer *siz_all      = new wxBoxSizer(wxVERTICAL);
  wxStaticBoxSizer *siz_sb = new wxStaticBoxSizer(new wxStaticBox(this, -1, Z("Status and progress")), wxVERTICAL);

  wxFlexGridSizer *siz_fg  = new wxFlexGridSizer(2, 0, 10);
  siz_fg->AddGrowableCol(1);
  st_jobs = new wxStaticText(this, -1, Z("Processing 1000/1000"));
  siz_fg->Add(st_jobs, 0, wxALIGN_CENTER_VERTICAL, 0);
  g_jobs = new wxGauge(this, -1, jobs_to_start.size() * 100);
  siz_fg->Add(g_jobs, 1, wxALIGN_CENTER_VERTICAL | wxGROW, 0);

  wxStaticText *st_remaining_time_label       = new wxStaticText(this, -1, Z("Remaining time:"));
  st_remaining_time                           = new wxStaticText(this, -1, Z("is being estimated"));
  wxStaticText *st_remaining_time_label_total = new wxStaticText(this, -1, Z("Remaining time:"));
  st_remaining_time_total                     = new wxStaticText(this, -1, Z("is being estimated"));

  siz_fg->Add(st_remaining_time_label_total, 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM,          10);
  siz_fg->Add(st_remaining_time_total,       1, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxGROW, 10);

  st_current = new wxStaticText(this, -1, Z("Current job ID 1000:"));
  siz_fg->Add(st_current, 0, wxALIGN_CENTER_VERTICAL, 10);
  g_progress = new wxGauge(this, -1, 100);
  siz_fg->Add(g_progress, 1, wxALIGN_CENTER_VERTICAL | wxGROW, 0);

  siz_fg->Add(st_remaining_time_label, 0, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM,          10);
  siz_fg->Add(st_remaining_time,       1, wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxGROW, 10);

  siz_sb->Add(siz_fg, 0, wxGROW | wxLEFT | wxRIGHT | wxTOP, 10);
  siz_sb->AddSpacer(10);

  siz_sb->Add(new wxStaticText(this, -1, Z("Log output:")), wxALIGN_LEFT, wxLEFT, 10);
  siz_sb->AddSpacer(5);
  tc_log = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(450, 150), wxTE_MULTILINE | wxTE_READONLY);
  siz_sb->Add(tc_log, 1, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 10);
  siz_all->Add(siz_sb, 1, wxGROW | wxALL, 10);

  cb_abort_after_current = new wxCheckBox(this, -1, Z("Abort after current job"));
  cb_abort_after_current->SetToolTip(TIP("Abort processing after the current job"));
  siz_all->Add(cb_abort_after_current, 0, wxALIGN_LEFT | wxLEFT, 10);

  b_ok = new wxButton(this, wxID_OK, Z("&Ok"));
  b_ok->Enable(false);
  b_abort = new wxButton(this, ID_JOBS_B_ABORT, Z("&Abort"));
  b_abort->SetToolTip(TIP("Abort the muxing process right now"));

  wxBoxSizer *siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(1,       0, 1, wxGROW, 0);
  siz_line->Add(b_ok,    0, 0, 0);
  siz_line->Add(1,       0, 1, wxGROW, 0);
  siz_line->Add(b_abort, 0, 0, 0);
  siz_line->Add(1,       0, 1, wxGROW, 0);
  siz_all->Add(siz_line, 0, wxGROW | wxTOP | wxBOTTOM, 10);
  siz_all->SetSizeHints(this);

  SetSizer(siz_all);

#if defined(SYS_WINDOWS)
  if (get_windows_version() >= WINDOWS_VERSION_7)
    m_taskbar_progress = new taskbar_progress_c(this);
#endif

  m_start_time_total                 = get_current_time_millis();
  m_next_remaining_time_update_total = m_start_time_total + 8000;

  start_next_job();

  ShowModal();
}

void
job_run_dialog::start_next_job() {
  t_update->Stop();

  ++current_job;

  if ((static_cast<int>(jobs_to_start.size()) <= current_job) || cb_abort_after_current->IsChecked() || abort) {
    if (   abort
        || (   cb_abort_after_current->IsChecked()
            && (current_job < static_cast<int>(jobs_to_start.size()))))
      add_to_log(wxString::Format(Z("Aborted processing on %s"), format_date_time(wxGetUTCTime()).c_str()));
    else
      add_to_log(wxString::Format(Z("Finished processing on %s"), format_date_time(wxGetUTCTime()).c_str()));

    b_abort->Enable(false);
    cb_abort_after_current->Enable(false);
    b_ok->Enable(true);
    b_ok->SetFocus();
    SetTitle(Z("mkvmerge has finished"));

    st_remaining_time->SetLabel(wxT("---"));
    st_remaining_time_total->SetLabel(wxT("---"));

#if defined(SYS_WINDOWS)
    if (NULL != m_taskbar_progress)
      m_taskbar_progress->set_state(TBPF_NOPROGRESS);
#endif

    return;
  }

  m_start_time                 = get_current_time_millis();
  m_next_remaining_time_update = m_start_time + 8000;
  st_remaining_time->SetLabel(Z("is being estimated"));

#if defined(SYS_WINDOWS)
  if (NULL != m_taskbar_progress) {
    m_taskbar_progress->set_state(TBPF_NORMAL);
    m_taskbar_progress->set_value(current_job * 100, jobs_to_start.size() * 100);
  }
#endif

  int ndx = jobs_to_start[current_job];
  st_jobs->SetLabel(wxString::Format(Z("Processing job %d/%d"), current_job + 1, (int)jobs_to_start.size()));
  st_current->SetLabel(wxString::Format(Z("Current job ID %d:"), jobs[ndx].id));

  mdlg->load(wxString::Format(wxT("%s/%d.mmg"), app->get_jobs_folder().c_str(), jobs[ndx].id), true);

  opt_file_name.Printf(wxT("%smmg-mkvmerge-options-%d-%d"), get_temp_dir().c_str(), (int)wxGetProcessId(), (int)wxGetUTCTime());

  wxFile *opt_file;
  try {
    opt_file = new wxFile(opt_file_name, wxFile::write);
  } catch (...) {
    jobs[ndx].log->Printf(Z("Could not create a temporary file for mkvmerge's command line option called '%s' (error code %d, %s)."),
                          opt_file_name.c_str(), errno, wxUCS(strerror(errno)));
    jobs[ndx].status = JOBS_FAILED;
    mdlg->save_job_queue();
    if (NULL != process) {
      delete process;
      process = NULL;
    }
    start_next_job();
    return;
  }

  static const unsigned char utf8_bom[3] = {0xef, 0xbb, 0xbf};
  opt_file->Write(utf8_bom, 3);

  mdlg->update_command_line();
  wxArrayString *arg_list = &mdlg->get_command_line_args();

  size_t i;
  for (i = 1; i < arg_list->Count(); i++) {
    if ((*arg_list)[i].Length() == 0)
      opt_file->Write(wxT("#EMPTY#"));
    else {
      std::string arg_utf8 = escape(wxMB((*arg_list)[i]));
      opt_file->Write(arg_utf8.c_str(), arg_utf8.length());
    }
    opt_file->Write(wxT("\n"));
  }
  delete opt_file;

  process = new wxProcess(this, 1);
  process->Redirect();
  wxString command_line = wxString::Format(wxT("\"%s\" \"@%s\""), (*arg_list)[0].c_str(), opt_file_name.c_str());
  pid = wxExecute(command_line, wxEXEC_ASYNC, process);
  if (0 == pid) {
    wxLogError(wxT("Execution of '%s' failed."), command_line.c_str());
    return;
  }
  out = process->GetInputStream();

  *jobs[ndx].log        = wxEmptyString;
  jobs[ndx].started_on  = wxGetUTCTime();
  jobs[ndx].finished_on = -1;

  add_to_log(wxString::Format(Z("Starting job ID %d (%s) on %s"), jobs[ndx].id, jobs[ndx].description->c_str(), format_date_time(jobs[ndx].started_on).c_str()));

  t_update->Start(100);
}

void
job_run_dialog::process_input() {
  if (NULL == process)
    return;

  while (process->IsInputAvailable()) {
    bool got_char = false;
    char c        = 0;

    if (!out->Eof()) {
      c = out->GetC();
      got_char = true;
    }

    if (got_char && ((c == '\n') || (c == '\r') || out->Eof())) {
      wxString wx_line = wxU(line);
      if (wx_line.Find(Z("Progress")) == 0) {
        int percent_pos = wx_line.Find(wxT("%"));
        if (0 < percent_pos) {
          wx_line.Remove(percent_pos);
          wxString tmp = wx_line.AfterLast(wxT(' '));

          long value;
          tmp.ToLong(&value);
          if ((value >= 0) && (value <= 100))
            set_progress_value(value);
        }
      } else if (wx_line.Length() > 0)
        *jobs[jobs_to_start[current_job]].log += wx_line + wxT("\n");
      line = "";
    } else if ((unsigned char)c != 0xff)
      line += c;

    update_remaining_time();

    if (out->Eof())
      break;
  }
}

void
job_run_dialog::set_progress_value(long value) {
  m_progress = current_job * 100 + value;
  g_progress->SetValue(value);
  g_jobs->SetValue(m_progress);

#if defined(SYS_WINDOWS)
  if (NULL != m_taskbar_progress)
    m_taskbar_progress->set_value(current_job * 100 + value, jobs_to_start.size() * 100);
#endif
}

void
job_run_dialog::update_remaining_time() {
  if (0 == m_progress)
    return;

  int64_t now = get_current_time_millis();

  if ((0 != (m_progress % 100)) && (now >= m_next_remaining_time_update)) {
    int64_t total_time           = (now - m_start_time) * 100 / (m_progress % 100);
    int64_t remaining_time       = total_time - now + m_start_time;
    m_next_remaining_time_update = now + 1000;

    st_remaining_time->SetLabel(wxU(create_minutes_seconds_time_string(static_cast<unsigned int>(remaining_time / 1000))));
  }

  if (now >= m_next_remaining_time_update_total) {
    int64_t total_percentage = m_progress / jobs_to_start.size();
    if (0 != total_percentage) {
      int64_t total_time                 = (now - m_start_time_total) * 100 / total_percentage;
      int64_t remaining_time             = total_time - now + m_start_time_total;
      m_next_remaining_time_update_total = now + 1000;

      st_remaining_time_total->SetLabel(wxU(create_minutes_seconds_time_string(static_cast<unsigned int>(remaining_time / 1000))));
    }
  }
}

void
job_run_dialog::on_timer(wxTimerEvent &) {
  process_input();
}

void
job_run_dialog::on_idle(wxIdleEvent &) {
  process_input();
}

void
job_run_dialog::on_abort(wxCommandEvent &) {
  abort = true;
#if defined(SYS_WINDOWS)
  wxKill(pid, wxSIGKILL);
  if (NULL != m_taskbar_progress)
    m_taskbar_progress->set_state(TBPF_ERROR);
#else
  wxKill(pid, wxSIGTERM);
#endif
}

void
job_run_dialog::on_end_process(wxProcessEvent &evt) {
  process_input();

  int ndx       = jobs_to_start[current_job];
  int exit_code = evt.GetExitCode();
  wxString status;

  if (abort) {
    jobs[ndx].status = JOBS_ABORTED;
    status           = Z("aborted");
  } else if (0 == exit_code) {
    jobs[ndx].status = JOBS_DONE;
    status           = Z("completed OK");
  } else if (1 == exit_code) {
    jobs[ndx].status = JOBS_DONE_WARNINGS;
    status           = Z("completed with warnings");
  } else {
    jobs[ndx].status = JOBS_FAILED;
    status           = Z("failed");
  }

  jobs[ndx].finished_on = wxGetUTCTime();

  add_to_log(wxString::Format(Z("Finished job ID %d on %s: status '%s'"), jobs[ndx].id, format_date_time(jobs[ndx].finished_on).c_str(), status.c_str()));

  mdlg->save_job_queue();
  delete process;
  process = NULL;
  out     = NULL;

  wxRemoveFile(opt_file_name);

  if (!abort)
    g_jobs->SetValue((current_job + 1) * 100);

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
                               wxString &log)
  : wxDialog(parent, -1, Z("Job output"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
  , text(log)
{
  wxBoxSizer *siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->Add(new wxStaticText(this, -1, Z("Output of the selected jobs:")), 0, wxALIGN_LEFT | wxLEFT | wxTOP, 10);
  siz_all->AddSpacer(5);
  siz_all->Add(new wxTextCtrl(this, -1, log, wxDefaultPosition, wxSize(550, 150), wxTE_MULTILINE | wxTE_READONLY), 1, wxGROW | wxLEFT | wxRIGHT, 10);

  wxBoxSizer *siz_buttons = new wxBoxSizer(wxHORIZONTAL);
  wxButton *b_ok          = new wxButton(this, wxID_OK, Z("&Ok"));
  b_ok->SetDefault();
  siz_buttons->Add(1,    0, 1, wxGROW, 0);
  siz_buttons->Add(b_ok, 0, 0, 0);
  siz_buttons->Add(1,    0, 1, wxGROW, 0);
  siz_buttons->Add(new wxButton(this, ID_JOBS_B_SAVE_LOG, Z("&Save")), 0, 0, 0);
  siz_buttons->Add(1, 0, 1, wxGROW, 0);
  siz_all->Add(siz_buttons, 0, wxGROW | wxTOP | wxBOTTOM, 10);
  siz_all->SetSizeHints(this);
  SetSizer(siz_all);

  ShowModal();
}

void
job_log_dialog::on_save(wxCommandEvent &) {
  wxFileDialog dialog(NULL, Z("Choose an output file"), last_open_dir, wxEmptyString, wxString::Format(Z("Text files (*.txt)|*.txt|%s"), ALLFILES.c_str()), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if(dialog.ShowModal() != wxID_OK)
    return;

  wxString copy = text;
#if defined(SYS_WINDOWS)
  copy.Replace(wxT("\n"), wxT("\r\n"));
#endif
  wxFile file(dialog.GetPath(), wxFile::write);
  file.Write(copy);
  last_open_dir = dialog.GetDirectory();
}

// ---------------------------------------------------

job_dialog::job_dialog(wxWindow *parent):
  wxDialog(parent, -1, Z("Job queue management"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX | wxMAXIMIZE_BOX) {

  wxBoxSizer *siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->Add(new wxStaticText(this, -1, Z("Current and past jobs:")), 0, wxALIGN_LEFT | wxALL, 10);
  lv_jobs = new jobdlg_list_view(this, ID_JOBS_LV_JOBS);

  wxListItem item;
  item.m_mask  = wxLIST_MASK_TEXT;
  item.m_text  = Z("ID");
  item.m_image = -1;
  lv_jobs->InsertColumn(0, item);
  item.m_text  = Z("Status");
  lv_jobs->InsertColumn(1, item);
  item.m_text  = Z("Description");
  lv_jobs->InsertColumn(2, item);
  item.m_text  = Z("Added on");
  lv_jobs->InsertColumn(3, item);
  item.m_text  = Z("Started on");
  lv_jobs->InsertColumn(4, item);
  item.m_text  = Z("Finished on");
  lv_jobs->InsertColumn(5, item);

  size_t i;
  for (i = 0; i < jobs.size(); i++)
    create_list_item(i);

  long dummy = lv_jobs->InsertItem(0, wxT("12345"));
  lv_jobs->SetItem(dummy, 1, Z("aborted"));
  lv_jobs->SetItem(dummy, 2, wxT("2004-05-06 07:08:09"));
  lv_jobs->SetItem(dummy, 3, wxT("2004-05-06 07:08:09"));
  lv_jobs->SetItem(dummy, 4, wxT("2004-05-06 07:08:09"));
  lv_jobs->SetItem(dummy, 5, wxT("2004-05-06 07:08:09"));

  for (i = 0; i < 6; i++)
    lv_jobs->SetColumnWidth(i, wxLIST_AUTOSIZE);

  lv_jobs->DeleteItem(0);

  wxBoxSizer *siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(lv_jobs, 1, wxGROW | wxRIGHT, 10);

  wxBoxSizer *siz_b_right = new wxBoxSizer(wxVERTICAL);
  b_up = new wxButton(this, ID_JOBS_B_UP, Z("&Up"));
  b_up->SetToolTip(TIP("Move the selected job(s) up"));
  siz_b_right->Add(b_up, 0, wxGROW | wxLEFT | wxBOTTOM, 10);
  b_down = new wxButton(this, ID_JOBS_B_DOWN, Z("&Down"));
  b_down->SetToolTip(TIP("Move the selected job(s) down"));
  siz_b_right->Add(b_down, 0, wxGROW | wxLEFT | wxBOTTOM, 10);
  siz_b_right->AddSpacer(15);

  b_reenable = new wxButton(this, ID_JOBS_B_REENABLE, Z("&Re-enable"));
  b_reenable->SetToolTip(TIP("Re-enable the selected job(s)"));
  siz_b_right->Add(b_reenable, 0, wxGROW | wxLEFT | wxBOTTOM, 10);
  b_disable = new wxButton(this, ID_JOBS_B_DISABLE, Z("&Disable"));
  b_disable->SetToolTip(TIP("Disable the selected job(s) and sets their status to 'done'"));
  siz_b_right->Add(b_disable, 0, wxGROW | wxLEFT | wxBOTTOM, 10);
  siz_b_right->AddSpacer(15);

  b_delete = new wxButton(this, ID_JOBS_B_DELETE, Z("D&elete"));
  b_delete->SetToolTip(TIP("Delete the selected job(s) from the job queue"));
  siz_b_right->Add(b_delete, 0, wxGROW | wxLEFT | wxBOTTOM, 10);
  siz_b_right->AddSpacer(15);

  b_view_log = new wxButton(this, ID_JOBS_B_VIEW_LOG, Z("&View log"));
  b_view_log->SetToolTip(TIP("View the output that mkvmerge generated during the muxing process for the selected job(s)"));
  siz_b_right->Add(b_view_log, 0, wxGROW | wxLEFT, 10);
  siz_line->Add(siz_b_right, 0, 0, 0);
  siz_all->Add(siz_line, 1, wxGROW | wxLEFT | wxRIGHT, 10);

  siz_all->Add(new wxStaticLine(this, -1), 0, wxGROW | wxALL, 10);

  wxBoxSizer *siz_b_bottom = new wxBoxSizer(wxHORIZONTAL);
  b_ok = new wxButton(this, wxID_OK, Z("&Ok"));
  b_ok->SetDefault();
  siz_b_bottom->Add(b_ok, 0, 0, 0);
  siz_b_bottom->Add(1, 0, 1, wxGROW, 0);

  b_start = new wxButton(this, ID_JOBS_B_START, Z("&Start"));
  b_start->SetToolTip(TIP("Start the jobs whose status is 'pending'"));
  siz_b_bottom->Add(b_start, 0, wxGROW | wxRIGHT, 10);
  siz_b_bottom->Add(10, 0, 0, 0, 0);
  b_start_selected = new wxButton(this, ID_JOBS_B_START_SELECTED, Z("S&tart selected"));
  b_start_selected->SetToolTip(TIP("Start the selected job(s) regardless of their status"));
  siz_b_bottom->Add(b_start_selected, 0, wxGROW | wxLEFT, 10);
  siz_all->Add(siz_b_bottom, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 10);
  siz_all->SetSizeHints(this);
  SetSizer(siz_all);

  enable_buttons(false);

  b_start->Enable(!jobs.empty());

  ShowModal();
}

void
job_dialog::create_list_item(int i) {
  wxString s;
  s.Printf(wxT("%d"), jobs[i].id);

  long dummy = lv_jobs->InsertItem(i, s);

  s = JOBS_PENDING       == jobs[i].status ? Z("pending")
    : JOBS_DONE          == jobs[i].status ? Z("done")
    : JOBS_DONE_WARNINGS == jobs[i].status ? Z("done/warnings")
    : JOBS_ABORTED       == jobs[i].status ? Z("aborted")
    :                                        Z("failed");
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
job_dialog::enable_buttons(bool enable,
                           bool enable_up_down) {
  b_up->Enable(enable && enable_up_down);
  b_down->Enable(enable && enable_up_down);
  b_reenable->Enable(enable);
  b_disable->Enable(enable);
  b_delete->Enable(enable);
  b_start_selected->Enable(enable);
  b_view_log->Enable(enable);
}

void
job_dialog::on_start(wxCommandEvent &) {
  size_t i;
  std::vector<int> jobs_to_start;

  for (i = 0; jobs.size() > i; ++i)
    if (JOBS_PENDING == jobs[i].status)
      jobs_to_start.push_back(i);

  if (!jobs_to_start.empty())
    start_jobs(jobs_to_start);
}

void
job_dialog::on_start_selected(wxCommandEvent &) {
  size_t i;
  std::vector<int> jobs_to_start;

  for (i = 0; jobs.size() > i; ++i)
    if (lv_jobs->IsSelected(i))
      jobs_to_start.push_back(i);

  if (!jobs_to_start.empty())
    start_jobs(jobs_to_start);
}

void
job_dialog::on_delete(wxCommandEvent &) {
  size_t i;
  std::vector<bool> selected;

  for (i = 0; static_cast<size_t>(lv_jobs->GetItemCount()) > i; ++i)
    selected.push_back(lv_jobs->IsSelected(i));

  i        = 0;
  size_t k = 0;
  while (jobs.size() > i) {
    if (selected[k]) {
      wxRemoveFile(wxString::Format(wxT("%s/%d.mmg"), app->get_jobs_folder().c_str(), jobs[i].id));
      jobs.erase(jobs.begin() + i);
      lv_jobs->DeleteItem(i);
    } else
      ++i;

    ++k;
  }

  mdlg->save_job_queue();
}

void
job_dialog::swap_rows(unsigned int lower,
                      unsigned int higher,
                      bool up) {
  if ((lower == higher) || (jobs.size() <= lower) || (jobs.size() <= higher))
    return;

  if (lower > higher) {
    int tmp_i = lower;
    lower     = higher;
    higher    = tmp_i;
  }
  job_t tmp_job = jobs[lower];
  jobs[lower]   = jobs[higher];
  jobs[higher]  = tmp_job;

  if (up) {
    lv_jobs->DeleteItem(lower);
    create_list_item(higher);

  } else {
    lv_jobs->DeleteItem(higher);
    create_list_item(lower);
  }
}

void
job_dialog::on_up(wxCommandEvent &) {
  std::vector<bool> selected;

  bool first = true;
  size_t i   = 0;
  size_t k;
  for (k = 0; static_cast<size_t>(lv_jobs->GetItemCount()) > k; ++k) {
    selected.push_back(lv_jobs->IsSelected(k));
    if (lv_jobs->IsSelected(k) && first)
      ++i;
    else
      first = false;
  }

  for (; jobs.size() > i; i++)
    if (selected[i]) {
      swap_rows(i - 1, i, true);
      selected[i - 1] = true;
      selected[i]     = false;
    }

  mdlg->save_job_queue();
}

void
job_dialog::on_down(wxCommandEvent &) {
  std::vector<bool> selected;

  bool first = true;
  int i      = lv_jobs->GetItemCount() - 1;
  int k;
  for (k = i; 0 <= k; --k) {
    selected.insert(selected.begin(), lv_jobs->IsSelected(k));
    if (lv_jobs->IsSelected(k) && first)
      --i;
    else
      first = false;
  }

  for (; 0 <= i; --i)
    if (selected[i]) {
      swap_rows(i + 1, i, false);
      selected[i + 1] = true;
      selected[i]     = false;
    }

  mdlg->save_job_queue();
}

void
job_dialog::on_reenable(wxCommandEvent &) {
  long item = -1;
  while (true) {
    item = lv_jobs->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (-1 == item)
      break;

    jobs[item].status = JOBS_PENDING;
    lv_jobs->SetItem(item, 1, Z("pending"));
  }

  mdlg->save_job_queue();
}

void
job_dialog::on_disable(wxCommandEvent &) {
  long item = -1;
  while (true) {
    item = lv_jobs->GetNextItem(item, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
    if (-1 == item)
      break;

    jobs[item].status = JOBS_DONE;
    lv_jobs->SetItem(item, 1, Z("done"));
  }

  mdlg->save_job_queue();
}

void
job_dialog::on_view_log(wxCommandEvent &) {
  wxString log;
  size_t i;

  for (i = 0; i < jobs.size(); i++) {
    if (!lv_jobs->IsSelected(i))
      continue;
    log += wxString::Format(Z("--- BEGIN job %d (%s, added on %s)"), jobs[i].id, jobs[i].description->c_str(), format_date_time(jobs[i].added_on).c_str());
    if (-1 != jobs[i].started_on)
      log += wxString::Format(Z(", started on %s"), format_date_time(jobs[i].started_on).c_str());
    log += wxT("\n");
    if (jobs[i].log->empty())
      log += Z("--- No job output found.\n");
    else
      log += *jobs[i].log;
    if (log.Last() != wxT('\n'))
      log += wxT("\n");
    log += wxString::Format(Z("--- END job %d"), jobs[i].id);
    if (-1 != jobs[i].finished_on)
      log += wxString::Format(Z(", finished on %s"), format_date_time(jobs[i].finished_on).c_str());
    log += wxT("\n\n");
  }

  if (log.empty())
    return;

  job_log_dialog dlg(this, log);
}

void
job_dialog::on_item_selected(wxListEvent &) {
  enable_buttons(lv_jobs->GetSelectedItemCount() > 0,
                 lv_jobs->GetSelectedItemCount() < lv_jobs->GetItemCount());
}

void
job_dialog::start_jobs(std::vector<int> &jobs_to_start) {
  wxString temp_settings = app->get_jobs_folder() + wxT("/temp.mmg");
  mdlg->save(temp_settings, true);

  mdlg->Show(false);
  Show(false);
  job_run_dialog *jrdlg = new job_run_dialog(this, jobs_to_start);
  jrdlg->Destroy();
  Show(true);
  mdlg->Show(true);

  mdlg->load(temp_settings, true);
  wxRemoveFile(temp_settings);

  size_t i;
  for (i = 0; i < jobs_to_start.size(); i++) {
    lv_jobs->DeleteItem(jobs_to_start[i]);
    create_list_item(jobs_to_start[i]);
  }
}

void
job_dialog::on_key_pressed(wxKeyEvent &evt) {
  if (WXK_RETURN == evt.GetKeyCode()) {
    EndModal(0);
    return;
  }

  if (1 != evt.GetKeyCode()) {  // Ctrl-A
    evt.Skip(true);
    return;
  }

  int i;
  for (i = 0; lv_jobs->GetItemCount() > i; ++i)
    lv_jobs->Select(i, true);
  enable_buttons(lv_jobs->GetSelectedItemCount() > 0, false);
}

jobdlg_list_view::jobdlg_list_view(wxWindow *parent,
                                   wxWindowID id)
  : wxListView(parent, id, wxDefaultPosition, wxSize(400, -1), wxLC_REPORT | wxSUNKEN_BORDER)
{
}

void
jobdlg_list_view::on_key_pressed(wxKeyEvent &evt) {
  int key_code = evt.GetKeyCode();
  if (   (1 == key_code)        // Ctrl-A
      || (WXK_RETURN == key_code))
    ((job_dialog *)GetParent())->on_key_pressed(evt);
  else
    evt.Skip(true);
}

IMPLEMENT_CLASS(job_run_dialog, wxDialog);
BEGIN_EVENT_TABLE(job_run_dialog, wxDialog)
  EVT_BUTTON(ID_JOBS_B_ABORT, job_run_dialog::on_abort)
  EVT_END_PROCESS(1,          job_run_dialog::on_end_process)
  EVT_TIMER(1,                job_run_dialog::on_timer)
  EVT_IDLE(job_run_dialog::on_idle)
END_EVENT_TABLE();

IMPLEMENT_CLASS(job_log_dialog, wxDialog);
BEGIN_EVENT_TABLE(job_log_dialog, wxDialog)
  EVT_BUTTON(ID_JOBS_B_SAVE_LOG, job_log_dialog::on_save)
END_EVENT_TABLE();

IMPLEMENT_CLASS(job_dialog, wxDialog);
BEGIN_EVENT_TABLE(job_dialog, wxDialog)
  EVT_BUTTON(ID_JOBS_B_START,               job_dialog::on_start)
  EVT_BUTTON(ID_JOBS_B_START_SELECTED,      job_dialog::on_start_selected)
  EVT_BUTTON(ID_JOBS_B_UP,                  job_dialog::on_up)
  EVT_BUTTON(ID_JOBS_B_DOWN,                job_dialog::on_down)
  EVT_BUTTON(ID_JOBS_B_DELETE,              job_dialog::on_delete)
  EVT_BUTTON(ID_JOBS_B_REENABLE,            job_dialog::on_reenable)
  EVT_BUTTON(ID_JOBS_B_DISABLE,             job_dialog::on_disable)
  EVT_BUTTON(ID_JOBS_B_VIEW_LOG,            job_dialog::on_view_log)
  EVT_LIST_ITEM_SELECTED(ID_JOBS_LV_JOBS,   job_dialog::on_item_selected)
  EVT_LIST_ITEM_DESELECTED(ID_JOBS_LV_JOBS, job_dialog::on_item_selected)
END_EVENT_TABLE();

IMPLEMENT_CLASS(jobdlg_list_view, wxListView);
BEGIN_EVENT_TABLE(jobdlg_list_view, wxListView)
  EVT_CHAR(jobdlg_list_view::on_key_pressed)
END_EVENT_TABLE();
