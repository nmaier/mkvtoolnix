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
 * muxing dialog
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
  wxDialog(parent, -1, wxT("mkvmerge is running"), wxDefaultPosition,
#ifdef SYS_WINDOWS
           wxSize(500, 560),
#else
           wxSize(500, 520),
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

  c = 0;
  siz_status =
    new wxStaticBoxSizer(new wxStaticBox(this, -1, wxT("Status and progress")),
                         wxVERTICAL);
  st_label = new wxStaticText(this, -1, wxT(""));
  siz_status->Add(st_label, 0, wxGROW | wxALIGN_LEFT | wxALL, 5);
  g_progress = new wxGauge(this, -1, 100, wxDefaultPosition, wxSize(250, 15));
  siz_status->Add(g_progress, 1, wxALL | wxGROW, 5);

  siz_output =
    new wxStaticBoxSizer(new wxStaticBox(this, -1, wxT("Output")),
                         wxVERTICAL);
  siz_output->Add(new wxStaticText(this, -1, wxT("mkvmerge output:")),
                  0, wxALIGN_LEFT | wxALL, 5);
  tc_output =
    new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxDefaultSize,
                   wxTE_READONLY | wxTE_LINEWRAP | wxTE_MULTILINE);
  siz_output->Add(tc_output, 2, wxGROW | wxALL, 5);
  siz_output->Add(new wxStaticText(this, -1, wxT("Warnings:")),
                  0, wxALIGN_LEFT | wxALL, 5);
  tc_warnings =
    new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxDefaultSize,
                   wxTE_READONLY | wxTE_LINEWRAP | wxTE_MULTILINE);
  siz_output->Add(tc_warnings, 1, wxGROW | wxALL, 5);
  siz_output->Add(new wxStaticText(this, -1, wxT("Errors:")),
                  0, wxALIGN_LEFT | wxALL, 5);
  tc_errors =
    new wxTextCtrl(this, -1, wxT(""), wxDefaultPosition, wxDefaultSize,
                   wxTE_READONLY | wxTE_LINEWRAP | wxTE_MULTILINE);
  siz_output->Add(tc_errors, 1, wxGROW | wxALL, 5);

  siz_buttons = new wxBoxSizer(wxHORIZONTAL);
  siz_buttons->Add(0, 0, 1, wxGROW, 0);
  b_ok = new wxButton(this, ID_B_MUX_OK, wxT("Ok"));
  b_ok->Enable(false);
  siz_buttons->Add(b_ok);
  siz_buttons->Add(0, 0, 1, wxGROW, 0);
  b_abort = new wxButton(this, ID_B_MUX_ABORT, wxT("Abort"));
  siz_buttons->Add(b_abort);
  siz_buttons->Add(0, 0, 1, wxGROW, 0);
  b_save_log = new wxButton(this, ID_B_MUX_SAVELOG, wxT("Save log"));
  siz_buttons->Add(b_save_log);
  siz_buttons->Add(0, 0, 1, wxGROW, 0);

  siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->Add(siz_status, 0, wxGROW | wxALL, 5);
  siz_all->Add(siz_output, 1, wxGROW | wxALL, 5);
  siz_all->Add(siz_buttons, 0, wxGROW | wxALL, 10);
  SetSizer(siz_all);

  update_window(wxT("Muxing in progress."));
  MakeModal(true);
  Show(true);

  process = new mux_process(this);

  opt_file_name.Printf(wxT("mmg-mkvmerge-options-%d-%d"),
                       (int)wxGetProcessId(), (int)time(NULL));
  try {
    const unsigned char utf8_bom[3] = {0xef, 0xbb, 0xbf};
    opt_file = new wxFile(opt_file_name, wxFile::write);
    opt_file->Write(utf8_bom, 3);
  } catch (...) {
    wxString error;
    error.Printf(wxT("Could not create a temporary file for mkvmerge's "
                     "command line option called '%s' (error code %d, "
                     "%s)."), opt_file_name.c_str(), errno,
                 wxUCS(strerror(errno)));
    wxMessageBox(error, wxT("File creation failed"), wxOK | wxCENTER |
                 wxICON_ERROR);
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

  pid = wxExecute((*arg_list)[0] + wxT(" @") + opt_file_name, wxEXEC_ASYNC,
                  process);
  out = process->GetInputStream();

  line = "";
  log = wxT("");
  while (1) {
    if (!out->Eof())
      c = out->GetC();
    while (app->Pending())
      app->Dispatch();

    if ((c == '\n') || (c == '\r') || out->Eof()) {
#if WXUNICODE
      wx_line = wxU(to_utf8(cc_local_utf8, line).c_str());
#else
      wx_line = line.c_str();
#endif
      log += wx_line + wxT("\n");
      if (wx_line.Find(wxT("Warning:")) == 0)
        tc_warnings->AppendText(wx_line + wxT("\n"));
      else if (wx_line.Find(wxT("Error:")) == 0)
        tc_errors->AppendText(wx_line + wxT("\n"));
      else if (wx_line.Find(wxT("progress")) == 0) {
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

  MakeModal(false);
  ShowModal();
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
  wxFileDialog dlg(NULL, wxT("Choose an output file"), last_open_dir, wxT(""),
                   wxT("Log files (*.txt)|*.txt|" ALLFILES),
                   wxSAVE | wxOVERWRITE_PROMPT);
  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    file = new wxFile(dlg.GetPath(), wxFile::write);
    s = log + wxT("\n");
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
mux_dialog::done() {
  SetTitle(wxT("mkvmerge has finished"));

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
  wxString s;

  if (dlg == NULL)
    return;

  s.Printf(wxT("mkvmerge %s with a return code of %d. %s\n"),
           (status != 0) && (status != 1) ? wxT("FAILED") : wxT("finished"),
           status,
           status == 0 ? wxT("Everything went fine.") :
           status == 1 ?
#if defined(SYS_WINDOWS)
           wxT("There were warnings, or the process was terminated.")
#else
           wxT("There were warnings")
#endif
           : status == 2 ? wxT("There were ERRORs.") : wxT(""));
  dlg->update_window(s);
  dlg->done();
}

IMPLEMENT_CLASS(mux_dialog, wxDialog);
BEGIN_EVENT_TABLE(mux_dialog, wxDialog)
  EVT_BUTTON(ID_B_MUX_OK, mux_dialog::on_ok)
  EVT_BUTTON(ID_B_MUX_SAVELOG, mux_dialog::on_save_log)
  EVT_BUTTON(ID_B_MUX_ABORT, mux_dialog::on_abort)
END_EVENT_TABLE();
