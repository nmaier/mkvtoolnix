/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   muxing dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/wx.h>
#include <wx/clipbrd.h>
#include <wx/file.h>
#include <wx/confbase.h>
#include <wx/fileconf.h>
#include <wx/notebook.h>
#include <wx/listctrl.h>
#include <wx/statusbr.h>
#include <wx/statline.h>

#include "common/at_scope_exit.h"
#include "common/fs_sys_helpers.h"
#include "common/strings/editing.h"
#include "common/strings/formatting.h"
#include "mmg/mmg.h"
#include "mmg/mmg_dialog.h"
#include "mmg/mux_dialog.h"

mux_dialog::mux_dialog(wxWindow *parent):
  wxDialog(parent, -1, Z("mkvmerge is running"), wxDefaultPosition,
#ifdef SYS_WINDOWS
           wxSize(700, 560),
#else
           wxSize(720, 520),
#endif
           wxDEFAULT_FRAME_STYLE)
  , m_thread{nullptr}
#if defined(SYS_WINDOWS)
  , m_taskbar_progress(nullptr)
#endif  // SYS_WINDOWS
  , m_abort_button_changed(false)
  , m_exit_code(0)
  , m_progress(0)
{
  wxBoxSizer *siz_all, *siz_buttons, *siz_line;
  wxStaticBoxSizer *siz_status, *siz_output;

  siz_status = new wxStaticBoxSizer(new wxStaticBox(this, -1, Z("Status and progress")), wxVERTICAL);
  st_label = new wxStaticText(this, -1, wxEmptyString);
  st_remaining_time_label = new wxStaticText(this, -1, Z("Remaining time:"));
  st_remaining_time       = new wxStaticText(this, -1, Z("is being estimated"));
  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(st_label);
  siz_line->AddSpacer(5);
  siz_line->Add(st_remaining_time_label);
  siz_line->AddSpacer(5);
  siz_line->Add(st_remaining_time);
  siz_status->Add(siz_line, 0, wxGROW | wxALIGN_LEFT | wxALL, 5);
  g_progress = new wxGauge(this, -1, 100, wxDefaultPosition, wxSize(250, 15));
  siz_status->Add(g_progress, 1, wxALL | wxGROW, 5);

  siz_output = new wxStaticBoxSizer(new wxStaticBox(this, -1, Z("Output")), wxVERTICAL);
  siz_output->Add(new wxStaticText(this, -1, Z("mkvmerge output:")), 0, wxALIGN_LEFT | wxALL, 5);
  tc_output = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(-1, 100), wxTE_READONLY | wxTE_BESTWRAP | wxTE_MULTILINE);
  siz_output->Add(tc_output, 2, wxGROW | wxALL, 5);
  siz_output->Add(new wxStaticText(this, -1, Z("Warnings:")), 0, wxALIGN_LEFT | wxALL, 5);
  tc_warnings = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(-1, 50), wxTE_READONLY | wxTE_BESTWRAP | wxTE_MULTILINE);
  siz_output->Add(tc_warnings, 1, wxGROW | wxALL, 5);
  siz_output->Add(new wxStaticText(this, -1, Z("Errors:")), 0, wxALIGN_LEFT | wxALL, 5);
  tc_errors = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxSize(-1, 50), wxTE_READONLY | wxTE_BESTWRAP | wxTE_MULTILINE);
  siz_output->Add(tc_errors, 1, wxGROW | wxALL, 5);

  siz_buttons = new wxBoxSizer(wxHORIZONTAL);
  siz_buttons->AddStretchSpacer();
  b_ok = new wxButton(this, ID_B_MUX_OK, Z("Ok"));
  b_ok->Enable(false);
  siz_buttons->Add(b_ok, 0, wxGROW);
  siz_buttons->AddStretchSpacer();
  b_abort = new wxButton(this, ID_B_MUX_ABORT, Z("Abort"));
  siz_buttons->Add(b_abort, 0, wxGROW);
  siz_buttons->AddStretchSpacer();
  b_save_log = new wxButton(this, ID_B_MUX_SAVELOG, Z("Save log"));
  siz_buttons->Add(b_save_log, 0, wxGROW);
  siz_buttons->AddStretchSpacer();

  siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->Add(siz_status, 0, wxGROW | wxALL, 5);
  siz_all->Add(siz_output, 1, wxGROW | wxALL, 5);
  siz_all->Add(siz_buttons, 0, wxGROW | wxALL, 10);
  SetSizer(siz_all);
}

void
mux_dialog::run() {
  auto &arg_list = static_cast<mmg_dialog *>(GetParent())->get_command_line_args();

  opt_file_name.Printf(wxT("%smmg-mkvmerge-options-%d-%d"), get_temp_dir().c_str(), (int)wxGetProcessId(), (int)wxGetUTCTime());
  try {
    const unsigned char utf8_bom[3] = {0xef, 0xbb, 0xbf};
    wxFile opt_file{opt_file_name, wxFile::write};
    opt_file.Write(utf8_bom, 3);

    for (size_t i = 1; i < arg_list.Count(); i++) {
      if (arg_list[i].IsEmpty())
        opt_file.Write(wxT("#EMPTY#"));
      else {
        auto arg_utf8 = escape(to_utf8(arg_list[i]));
        opt_file.Write(arg_utf8.c_str(), arg_utf8.length());
      }
      opt_file.Write(wxT("\n"));
    }
  } catch (...) {
    wxString error;
    error.Printf(Z("Could not create a temporary file for mkvmerge's command line option called '%s' (error code %d, %s)."), opt_file_name.c_str(), errno, wxUCS(strerror(errno)));
    wxMessageBox(error, Z("File creation failed"), wxOK | wxCENTER | wxICON_ERROR);
    throw 0;
  }

#if defined(SYS_WINDOWS)
  if (get_windows_version() >= WINDOWS_VERSION_7) {
    m_taskbar_progress = new taskbar_progress_c(mdlg);
    m_taskbar_progress->set_state(TBPF_NORMAL);
    m_taskbar_progress->set_value(0, 100);
  }
#endif  // SYS_WINDOWS

  update_label(Z("Muxing in progress."));

  m_start_time                 = get_current_time_millis();
  m_next_remaining_time_update = m_start_time + 8000;

  auto command_line = wxString::Format(wxT("\"%s\" \"@%s\""), arg_list[0].c_str(), opt_file_name.c_str());
  m_thread          = new mux_thread{this, command_line};
  m_thread->Create();
  m_thread->execute();

  ShowModal();
}

mux_dialog::~mux_dialog() {
#if defined(SYS_WINDOWS)
  if (nullptr != m_taskbar_progress)
    m_taskbar_progress->set_state(TBPF_NOPROGRESS);
  delete m_taskbar_progress;
#endif  // SYS_WINDOWS

  wxRemoveFile(opt_file_name);
}

void
mux_dialog::update_label(wxString const &text) {
  st_label->SetLabel(text);
  Layout();
}

void
mux_dialog::update_gauge(long value) {
  m_progress = value;
  g_progress->SetValue(value);
#if defined(SYS_WINDOWS)
  if (nullptr != m_taskbar_progress)
    m_taskbar_progress->set_value(value, 100);
#endif  // SYS_WINDOWS
}

void
mux_dialog::update_remaining_time() {
  int64_t now = get_current_time_millis();

  if ((0 == m_progress) || (now < m_next_remaining_time_update))
    return;

  int64_t total_time     = (now - m_start_time) * 100 / m_progress;
  int64_t remaining_time = total_time - now + m_start_time;
  st_remaining_time->SetLabel(wxU(create_minutes_seconds_time_string(static_cast<unsigned int>(remaining_time / 1000))));

  m_next_remaining_time_update = now + 1000;
}

void
mux_dialog::on_ok(wxCommandEvent &) {
  Close(true);
}

void
mux_dialog::on_save_log(wxCommandEvent &) {
  wxFile *file;
  wxString s;
  wxFileDialog dlg(nullptr, Z("Choose an output file"), last_open_dir, wxEmptyString, wxString::Format(Z("Log files (*.txt)|*.txt|%s"), ALLFILES.c_str()), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    file = new wxFile(dlg.GetPath(), wxFile::write);
    s = log + wxT("\n");
#if defined(SYS_WINDOWS)
    s.Replace(wxT("\n"), wxT("\r\n"));
#endif
    file->Write(s);
    delete file;
  }
}

void
mux_dialog::on_abort(wxCommandEvent &) {
#if defined(SYS_WINDOWS)
  if (m_abort_button_changed) {
    wxFileName output_file_name(static_cast<mmg_dialog *>(GetParent())->tc_output->GetValue());
    wxExecute(wxString::Format(wxT("explorer \"%s\""), output_file_name.GetPath().c_str()));
    return;
  }

  change_abort_button();
#else
  if (m_abort_button_changed)
    return;

  m_abort_button_changed = true;

  b_abort->Enable(false);
#endif

  b_ok->Enable(true);
  b_ok->SetFocus();

  wxCriticalSectionLocker locker{m_cs_thread};
  if (nullptr != m_thread)
    m_thread->kill_process();
}

void
mux_dialog::on_close(wxCloseEvent &) {
  mdlg->muxing_has_finished(m_exit_code);
  EndModal(0);
}

#if defined(SYS_WINDOWS)
void
mux_dialog::change_abort_button() {
  if (m_abort_button_changed)
    return;

  b_abort->SetLabel(Z("Open folder"));
  m_abort_button_changed = true;

  Layout();
}
#endif  // SYS_WINDOWS

void
mux_dialog::on_output_available(wxCommandEvent &evt) {
  wxString line = evt.GetString();

  log += line;
  if (line.Right(1) != wxT("\r"))
    log += wxT("\n");

  if (line.Find(Z("Warning:")) == 0)
    tc_warnings->AppendText(line + wxT("\n"));

  else if (line.Find(Z("Error:")) == 0)
    tc_errors->AppendText(line + wxT("\n"));

  else if (line.Find(Z("Progress")) == 0) {
    if (line.Find(wxT("%")) != 0) {
      line.Remove(line.Find(wxT("%")));
      auto tmp   = line.AfterLast(wxT(' '));
      long value = 0;
      tmp.ToLong(&value);
      if ((value >= 0) && (value <= 100))
        update_gauge(value);
    }

  } else if (line.Length() > 0)
    tc_output->AppendText(line + wxT("\n"));

  update_remaining_time();
}

void
mux_dialog::on_process_terminated(wxCommandEvent &evt) {
  m_exit_code = evt.GetInt();

  wxString format;
  if ((0 != m_exit_code) && (1 != m_exit_code))
    format = Z("mkvmerge FAILED with a return code of %d. %s");
  else
    format = Z("mkvmerge finished with a return code of %d. %s");

#if defined(SYS_WINDOWS)
  wxString status_1 = Z("There were warnings, or the process was terminated.");
#else
  wxString status_1 = Z("There were warnings");
#endif

  auto status_str = 0 == m_exit_code ? wxString(Z("Everything went fine."))
                  : 1 == m_exit_code ? status_1
                  : 2 == m_exit_code ? wxString(Z("There were ERRORs."))
                  :                    wxString(wxT(""));

  update_label(wxString::Format(format, m_exit_code, status_str.c_str()));

  SetTitle(Z("mkvmerge has finished"));
  st_remaining_time_label->SetLabel(wxEmptyString);
  st_remaining_time->SetLabel(wxEmptyString);

  b_ok->Enable(true);
  b_ok->SetFocus();

#if defined(SYS_WINDOWS)
  change_abort_button();
  if (nullptr != m_taskbar_progress)
    m_taskbar_progress->set_state(0 != m_exit_code ? TBPF_ERROR : TBPF_NOPROGRESS);
#else  // SYS_WINDOWS
  b_abort->Enable(false);
#endif  // SYS_WINDOWS
}

// ------------------------------------------------------------

wxEventType const mux_thread::event = wxNewEventType();

mux_thread::mux_thread(mux_dialog *dlg,
                       wxString const &command_line)
  : m_dlg{dlg}
  , m_command_line{command_line}
  , m_process{new mux_process{this}}
  , m_pid{0}
{
}

void
mux_thread::execute() {
  m_pid = wxExecute(m_command_line, wxEXEC_ASYNC, m_process);
  if (0 == m_pid) {
    wxCommandEvent evt(mux_thread::event, mux_thread::process_terminated);
    evt.SetInt(2);
    wxPostEvent(m_dlg, evt);

  } else
    Run();
}

void *
mux_thread::Entry() {
  at_scope_exit_c unlink_thread{[&]() {
      wxCriticalSectionLocker locker{m_dlg->m_cs_thread};
      m_dlg->m_thread = nullptr;
    }};

  auto out = m_process->GetInputStream();

  std::string line;

  while (1) {
    if (!out->CanRead() && !out->Eof()) {
      wxMilliSleep(5);
      continue;
    }

    char c = !out->Eof() ? out->GetC() : '\n';

    if ((c == '\n') || (c == '\r') || out->Eof()) {
      if (nullptr != m_dlg) {
        // send line to dialog
        wxCommandEvent evt(mux_thread::event, mux_thread::output_available);
        evt.SetString(wxU(line));
        wxPostEvent(m_dlg, evt);
      }

      line.clear();

    } else if ((unsigned char)c != 0xff)
      line += c;

    if (out->Eof())
      break;
  }

  return nullptr;
}

void
mux_thread::kill_process() {
#if defined(SYS_WINDOWS)
  wxKill(m_pid, wxSIGKILL);
#else
  wxKill(m_pid, wxSIGTERM);
#endif
}

void
mux_thread::on_terminate(int status) {
  wxCommandEvent evt(mux_thread::event, mux_thread::process_terminated);
  evt.SetInt(status);
  wxPostEvent(m_dlg, evt);
}

mux_process::mux_process(mux_thread *thread)
  : wxProcess(wxPROCESS_REDIRECT)
  , m_thread(thread)
{
}

void
mux_process::OnTerminate(int,
                         int status) {
  if (nullptr != m_thread)
    m_thread->on_terminate(status);
}

IMPLEMENT_CLASS(mux_dialog, wxDialog);
BEGIN_EVENT_TABLE(mux_dialog, wxDialog)
  EVT_BUTTON(ID_B_MUX_OK,      mux_dialog::on_ok)
  EVT_BUTTON(ID_B_MUX_SAVELOG, mux_dialog::on_save_log)
  EVT_BUTTON(ID_B_MUX_ABORT,   mux_dialog::on_abort)
  EVT_COMMAND(mux_thread::output_available,   mux_thread::event, mux_dialog::on_output_available)
  EVT_COMMAND(mux_thread::process_terminated, mux_thread::event, mux_dialog::on_process_terminated)
  EVT_CLOSE(mux_dialog::on_close)
END_EVENT_TABLE();
