/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  mux_dialog.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <root@sirelvis.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief muxing dialog
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include "os.h"

#include <errno.h>
#include <unistd.h>

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
#include <windef.h>
#include <winbase.h>
#include <io.h>
#endif

#include "common.h"
#include "mm_io.h"
#include "mmg.h"
#include "mmg_dialog.h"
#include "mux_dialog.h"

mux_dialog::mux_dialog(wxWindow *parent):
  wxDialog(parent, -1, "mkvmerge is running", wxDefaultPosition,
#ifdef SYS_WINDOWS
           wxSize(500, 560),
#else
           wxSize(500, 520),
#endif
           wxCAPTION) {
  char c;
  long value;
  wxString line, tmp;
  wxInputStream *out;
  mm_io_c *opt_file;
  uint32_t i;
  wxArrayString *arg_list;

  c = 0;
  new wxStaticBox(this, -1, _("Status and progress"), wxPoint(10, 5),
                  wxSize(480, 70));
  st_label = new wxStaticText(this, -1, _(""), wxPoint(15, 25),
                              wxSize(450, -1));
  g_progress = new wxGauge(this, -1, 100, wxPoint(120, 50), wxSize(250, 15));

  new wxStaticBox(this, -1, _("Output"), wxPoint(10, 80), wxSize(480, 400));
  new wxStaticText(this, -1, _("mkvmerge output:"), wxPoint(15, 100));
  tc_output =
    new wxTextCtrl(this, -1, _(""), wxPoint(15, 120), wxSize(455, 140),
                   wxTE_READONLY | wxTE_LINEWRAP | wxTE_MULTILINE);
  new wxStaticText(this, -1, _("Warnings:"), wxPoint(15, 270));
  tc_warnings =
    new wxTextCtrl(this, -1, _(""), wxPoint(15, 290), wxSize(455, 80),
                   wxTE_READONLY | wxTE_LINEWRAP | wxTE_MULTILINE);
  new wxStaticText(this, -1, _("Errors:"), wxPoint(15, 380));
  tc_errors =
    new wxTextCtrl(this, -1, _(""), wxPoint(15, 400), wxSize(455, 70),
                   wxTE_READONLY | wxTE_LINEWRAP | wxTE_MULTILINE);

  b_ok = new wxButton(this, ID_B_MUX_OK, _("Ok"), wxPoint(1, 500));
  b_save_log =
    new wxButton(this, ID_B_MUX_SAVELOG, _("Save log"), wxPoint(1, 500));
  b_abort =
    new wxButton(this, ID_B_MUX_ABORT, _("Abort"), wxPoint(1, 500));
  b_ok->Move(wxPoint((int)(250 - 2 * b_save_log->GetSize().GetWidth()),
                     500 - b_ok->GetSize().GetHeight() / 2));
  b_abort->Move(wxPoint((int)(250 - 0.5 * b_save_log->GetSize().GetWidth()),
                        500 - b_ok->GetSize().GetHeight() / 2));
  b_save_log->Move(wxPoint((int)(250 + b_save_log->GetSize().GetWidth()),
                           500 - b_ok->GetSize().GetHeight() / 2));
  b_ok->Enable(false);

  update_window("Muxing in progress.");
  Show(true);

  process = new mux_process(this);

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
    wxString error;
    error.Printf("Could not create a temporary file for mkvmerge's command "
                 "line option called '%s' (error code %d, %s).",
                 opt_file_name.c_str(), errno, strerror(errno));
    wxMessageBox(error, "File creation failed", wxOK | wxCENTER |
                 wxICON_ERROR);
    throw 0;
  }
  arg_list = &static_cast<mmg_dialog *>(parent)->get_command_line_args();
  for (i = 1; i < arg_list->Count(); i++) {
    if ((*arg_list)[i].Length() == 0)
      opt_file->puts_unl("#EMPTY#");
    else
      opt_file->puts_unl((*arg_list)[i].c_str());
    opt_file->puts_unl("\n");
  }
  delete opt_file;

  pid = wxExecute((*arg_list)[0] + " @" + opt_file_name, wxEXEC_ASYNC,
                  process);
  out = process->GetInputStream();

  line = "";
  log = "";
  while (1) {
    if (!out->Eof()) {
      c = out->GetC();
      if ((unsigned char)c != 0xff)
        log.Append(c);
    }
    while (app->Pending())
      app->Dispatch();

    if ((c == '\n') || (c == '\r') || out->Eof()) {
      if (line.Find("Warning:") == 0)
        tc_warnings->AppendText(line + "\n");
      else if (line.Find("Error:") == 0)
        tc_errors->AppendText(line + "\n");
      else if (line.Find("progress") == 0) {
        if (line.Find("%)") != 0) {
          line.Remove(line.Find("%)"));
          tmp = line.AfterLast('(');
          tmp.ToLong(&value);
          if ((value >= 0) && (value <= 100))
            update_gauge(value);
        }
      } else if (line.Length() > 0)
        tc_output->AppendText(line + "\n");
      line = "";
    } else if ((unsigned char)c != 0xff)
      line.Append(c);

    if (out->Eof())
      break;
  }

  b_ok->Enable(true);
  b_abort->Enable(false);
  b_ok->SetFocus();
  ShowModal();
}

mux_dialog::~mux_dialog() {
  delete process;
  unlink(opt_file_name.c_str());
}

void mux_dialog::update_window(wxString text) {
  st_label->SetLabel(text);
}

void mux_dialog::update_gauge(long value) {
  g_progress->SetValue(value);
}

void mux_dialog::on_ok(wxCommandEvent &evt) {
  Close(true);
}

void mux_dialog::on_save_log(wxCommandEvent &evt) {
  wxFile *file;
  wxString s;
  wxFileDialog dlg(NULL, "Choose an output file", last_open_dir, "",
                   _T("Log files (*.txt)|*.txt|" ALLFILES),
                   wxSAVE | wxOVERWRITE_PROMPT);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    file = new wxFile(dlg.GetPath(), wxFile::write);
    s = log + "\n";
    file->Write(s);
    delete file;
  }
}

void mux_dialog::on_abort(wxCommandEvent &evt) {
#if defined(SYS_WINDOWS)
  wxKill(pid, wxSIGKILL);
#else
  wxKill(pid, wxSIGTERM);
#endif
}

mux_process::mux_process(mux_dialog *mux_dlg):
  wxProcess(wxPROCESS_REDIRECT), 
  dlg(mux_dlg) {
}

void mux_process::OnTerminate(int pid, int status) {
  wxString s;

  s.Printf("mkvmerge %s with a return code of %d. %s\n",
           (status != 0) && (status != 1) ? "FAILED" : "finished", status,
           status == 0 ? "Everything went fine." :
           status == 1 ? "There were warnings"
#if defined(SYS_WINDOWS)
           ", or the process was terminated."
#else
           "."
#endif
           : status == 2 ? "There were ERRORs." : "");
  dlg->update_window(s);
}

IMPLEMENT_CLASS(mux_dialog, wxDialog);
BEGIN_EVENT_TABLE(mux_dialog, wxDialog)
  EVT_BUTTON(ID_B_MUX_OK, mux_dialog::on_ok)
  EVT_BUTTON(ID_B_MUX_SAVELOG, mux_dialog::on_save_log)
  EVT_BUTTON(ID_B_MUX_ABORT, mux_dialog::on_abort)
END_EVENT_TABLE();
