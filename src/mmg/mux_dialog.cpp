/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   muxing dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <errno.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include "wx/wxprec.h"

#include "wx/wx.h"
#include "wx/clipbrd.h"
#include "wx/file.h"
#include "wx/confbase.h"
#include "wx/fileconf.h"
#include "wx/notebook.h"
#include "wx/listctrl.h"
#include "wx/statusbr.h"
#include "wx/statline.h"

#if defined(SYS_WINDOWS)
#include <windows.h>
#endif

#include "common.h"
#include "mmg.h"
#include "mmg_dialog.h"
#include "mux_dialog.h"

mux_dialog::mux_dialog(wxWindow *parent):
  wxDialog(parent, -1, Z("mkvmerge is running"), wxDefaultPosition,
#ifdef SYS_WINDOWS
           wxSize(700, 560),
#else
           wxSize(700, 520),
#endif
           wxDEFAULT_FRAME_STYLE) {
  char c;
  string arg_utf8, line;
  long value;
  wxString wx_line, tmp;
  wxInputStream *out;
  wxFile *opt_file;
  uint32_t i;
  wxArrayString *arg_list;
  wxBoxSizer *siz_all, *siz_buttons;
  wxStaticBoxSizer *siz_status, *siz_output;

  m_window_disabler = new wxWindowDisabler(this);

  c = 0;
  siz_status = new wxStaticBoxSizer(new wxStaticBox(this, -1, Z("Status and progress")), wxVERTICAL);
  st_label = new wxStaticText(this, -1, wxEmptyString);
  siz_status->Add(st_label, 0, wxGROW | wxALIGN_LEFT | wxALL, 5);
  g_progress = new wxGauge(this, -1, 100, wxDefaultPosition, wxSize(250, 15));
  siz_status->Add(g_progress, 1, wxALL | wxGROW, 5);

  siz_output = new wxStaticBoxSizer(new wxStaticBox(this, -1, Z("Output")), wxVERTICAL);
  siz_output->Add(new wxStaticText(this, -1, Z("mkvmerge output:")), 0, wxALIGN_LEFT | wxALL, 5);
  tc_output = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_BESTWRAP | wxTE_MULTILINE);
  siz_output->Add(tc_output, 2, wxGROW | wxALL, 5);
  siz_output->Add(new wxStaticText(this, -1, Z("Warnings:")), 0, wxALIGN_LEFT | wxALL, 5);
  tc_warnings = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_BESTWRAP | wxTE_MULTILINE);
  siz_output->Add(tc_warnings, 1, wxGROW | wxALL, 5);
  siz_output->Add(new wxStaticText(this, -1, Z("Errors:")), 0, wxALIGN_LEFT | wxALL, 5);
  tc_errors = new wxTextCtrl(this, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY | wxTE_BESTWRAP | wxTE_MULTILINE);
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

  update_window(Z("Muxing in progress."));
  Show(true);

  process = new mux_process(this);

  opt_file_name.Printf(wxT("%smmg-mkvmerge-options-%d-%d"), get_temp_dir().c_str(), (int)wxGetProcessId(), (int)time(NULL));
  try {
    const unsigned char utf8_bom[3] = {0xef, 0xbb, 0xbf};
    opt_file = new wxFile(opt_file_name, wxFile::write);
    opt_file->Write(utf8_bom, 3);
  } catch (...) {
    wxString error;
    error.Printf(Z("Could not create a temporary file for mkvmerge's command line option called '%s' (error code %d, %s)."), opt_file_name.c_str(), errno, wxUCS(strerror(errno)));
    wxMessageBox(error, Z("File creation failed"), wxOK | wxCENTER | wxICON_ERROR);
    throw 0;
  }
  arg_list = &static_cast<mmg_dialog *>(parent)->get_command_line_args();
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

  pid = wxExecute((*arg_list)[0] + wxT(" \"@") + opt_file_name + wxT("\""),
                  wxEXEC_ASYNC, process);
  out = process->GetInputStream();

  line = "";
  log = wxEmptyString;
  while (1) {
    while (app->Pending())
      app->Dispatch();

    if (!out->CanRead() && !out->Eof()) {
      wxMilliSleep(5);
      continue;
    }

    if (!out->Eof())
      c = out->GetC();
    else
      c = '\n';

    if ((c == '\n') || (c == '\r') || out->Eof()) {
      wx_line = wxU(line.c_str());
      log += wx_line;
      if (c != '\r')
        log += wxT("\n");
      if (wx_line.Find(Z("Warning:")) == 0)
        tc_warnings->AppendText(wx_line + wxT("\n"));
      else if (wx_line.Find(Z("Error:")) == 0)
        tc_errors->AppendText(wx_line + wxT("\n"));
      else if (wx_line.Find(Z("Progress")) == 0) {
        if (wx_line.Find(wxT("%")) != 0) {
          wx_line.Remove(wx_line.Find(wxT("%")));
          tmp = wx_line.AfterLast(wxT(' '));
          tmp.ToLong(&value);
          if ((value >= 0) && (value <= 100))
            update_gauge(value);
        }
      } else if (wx_line.Length() > 0)
        tc_output->AppendText(wx_line + wxT("\n"));
      line = "";
    } else if ((unsigned char)c != 0xff)
      line += c;

    if (out->Eof())
      break;
  }
}

mux_dialog::~mux_dialog() {
  process->dlg = NULL;
  delete process;
  wxRemoveFile(opt_file_name);
}

void
mux_dialog::update_window(wxString text) {
  st_label->SetLabel(text);
}

void
mux_dialog::update_gauge(long value) {
  g_progress->SetValue(value);
}

void
mux_dialog::on_ok(wxCommandEvent &evt) {
  Close(true);
}

void
mux_dialog::on_save_log(wxCommandEvent &evt) {
  wxFile *file;
  wxString s;
  wxFileDialog dlg(NULL, Z("Choose an output file"), last_open_dir, wxEmptyString, wxString::Format(Z("Log files (*.txt)|*.txt|%s"), ALLFILES.c_str()), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
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
mux_dialog::on_abort(wxCommandEvent &evt) {
#if defined(SYS_WINDOWS)
  wxKill(pid, wxSIGKILL);
#else
  wxKill(pid, wxSIGTERM);
#endif
  b_abort->Enable(false);
}

void
mux_dialog::on_close(wxCloseEvent &evt) {
  mdlg->muxing_has_finished();
  delete m_window_disabler;
  Destroy();
}

void
mux_dialog::done() {
  SetTitle(Z("mkvmerge has finished"));

  b_ok->Enable(true);
  b_abort->Enable(false);
  b_ok->SetFocus();
}

mux_process::mux_process(mux_dialog *mux_dlg):
  wxProcess(wxPROCESS_REDIRECT),
  dlg(mux_dlg) {
}

void
mux_process::OnTerminate(int pid,
                         int status) {
  if (NULL == dlg)
    return;

  wxString format;
  if ((status != 0) && (status != 1))
    format = Z("mkvmerge FAILED with a return code of %d. %s\n");
  else
    format = Z("mkvmerge finished with a return code of %d. %s\n");

#if defined(SYS_WINDOWS)
  wxString status_1 = Z("There were warnings, or the process was terminated.");
#else
  wxString status_1 = Z("There were warnings");
#endif

  wxString status_str = 0 == status ? wxString(Z("Everything went fine."))
                      : 1 == status ? status_1
                      : 2 == status ? wxString(Z("There were ERRORs."))
                      :               wxString(wxT(""));

  dlg->update_window(wxString::Format(format, status, status_str.c_str()));
  dlg->done();
}

IMPLEMENT_CLASS(mux_dialog, wxDialog);
BEGIN_EVENT_TABLE(mux_dialog, wxDialog)
  EVT_BUTTON(ID_B_MUX_OK,      mux_dialog::on_ok)
  EVT_BUTTON(ID_B_MUX_SAVELOG, mux_dialog::on_save_log)
  EVT_BUTTON(ID_B_MUX_ABORT,   mux_dialog::on_abort)
  EVT_CLOSE(mux_dialog::on_close)
END_EVENT_TABLE();
