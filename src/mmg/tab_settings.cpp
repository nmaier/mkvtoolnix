/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  tab_input.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <flo.wagner@gmx.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief "settings" tab
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include "wx/wxprec.h"

#include "wx/wx.h"
#include "wx/config.h"
#include "wx/listctrl.h"
#include "wx/notebook.h"
#include "wx/process.h"
#include "wx/statline.h"

#include "common.h"
#include "matroskalogo_big.xpm"
#include "mmg.h"
#include "tab_settings.h"

tab_settings::tab_settings(wxWindow *parent):
  wxPanel(parent, -1, wxDefaultPosition, wxSize(100, 400),
          wxTAB_TRAVERSAL) {
  new wxStaticBox(this, -1, wxT("mkvmerge executable"), wxPoint(10, 5),
                  wxSize(475, 50));
  tc_mkvmerge =
    new wxTextCtrl(this, ID_TC_MKVMERGE, wxT(""), wxPoint(15, 25),
                   wxSize(370, -1), wxTE_READONLY);

  new wxButton(this, ID_B_BROWSEMKVMERGE, wxT("Browse"), wxPoint(395, 25),
               wxDefaultSize, 0);

  new wxStaticBox(this, -1, wxT("Miscellaneous options"), wxPoint(10, 65),
                  wxSize(475, 75));
  new wxStaticText(this, -1, wxT("Process priority:"), wxPoint(15, 85));
  cob_priority =
    new wxComboBox(this, ID_COB_PRIORITY, wxT(""), wxPoint(120, 85 + YOFF),
                   wxSize(90, -1), 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
  cob_priority->SetToolTip(wxT("Sets the priority that mkvmerge will run "
                               "with."));
#if defined(SYS_WINDOWS)
  cob_priority->Append(wxT("highest"));
  cob_priority->Append(wxT("higher"));
#endif
  cob_priority->Append(wxT("normal"));
  cob_priority->Append(wxT("lower"));
  cob_priority->Append(wxT("lowest"));

  cb_autoset_output_filename =
    new wxCheckBox(this, ID_CB_AUTOSET_OUTPUT_FILENAME,
                   wxT("Auto-set output filename"), wxPoint(15, 115 + YOFF));
  cb_autoset_output_filename->
    SetToolTip(wxT("If checked mmg will automatically set the output filename "
                   "if it hasn't been set already. This happens when you add "
                   "a file. It will be set to the same name as the "
                   "input file but with the extension '.mkv'. If unset mmg "
                   "will not touch the output filename."));


  new wxStaticBox(this, -1, wxT("About"), wxPoint(10, 350),
                  wxSize(475, 104));
  new wxStaticBitmap(this, -1, wxBitmap(matroskalogo_big_xpm),
                     wxPoint(20, 370), wxSize(64,64));
  new wxStaticText(this, wxID_STATIC,
                   wxT("mkvmerge GUI v" VERSION "\n"
                       "This GUI was written by Moritz Bunkus <moritz@"
                       "bunkus.org>\n"
                       "Based on mmg by Florian Wagner <flo.wagner@gmx.de>\n"
                       "mkvmerge GUI is licensed under the GPL.\n"
                       "http://www.bunkus.org/videotools/mkvtoolnix/"),
                   wxPoint(95, 360), wxDefaultSize, 0);

  load_preferences();
}

tab_settings::~tab_settings() {
  save_preferences();
}

void
tab_settings::on_browse(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, wxT("Choose the mkvmerge executable"),
                   tc_mkvmerge->GetValue().BeforeLast(PSEP), wxT(""),
#ifdef SYS_WINDOWS
                   wxT("Executable files (*.exe)|*.exe|" ALLFILES),
#else
                   wxT("All files (*)|*"),
#endif
                   wxOPEN);
  if(dlg.ShowModal() == wxID_OK) {
    tc_mkvmerge->SetValue(dlg.GetPath());
    mkvmerge_path = dlg.GetPath();
    save_preferences();
    query_mkvmerge_capabilities();
  }
}

void
tab_settings::on_xyz_selected(wxCommandEvent &evt) {
  save_preferences();
}

void
tab_settings::load_preferences() {
  wxConfig *cfg = (wxConfig *)wxConfigBase::Get();
  wxString priority;
  bool b;
  int i;

  cfg->SetPath(wxT("/GUI"));
  if (!cfg->Read(wxT("mkvmerge_executable"), &mkvmerge_path))
    mkvmerge_path = wxT("mkvmerge");
  tc_mkvmerge->SetValue(mkvmerge_path);
  query_mkvmerge_capabilities();

  if (!cfg->Read(wxT("process_priority"), &priority))
    priority = wxT("normal");
  cob_priority->SetSelection(0);
  for (i = 0; i < cob_priority->GetCount(); i++)
    if (priority == cob_priority->GetString(i)) {
      cob_priority->SetSelection(i);
      break;
    }

  if (!cfg->Read(wxT("autoset_output_filename"), &b))
    b = true;
  cb_autoset_output_filename->SetValue(b);
}

void
tab_settings::save_preferences() {
  wxConfig *cfg = (wxConfig *)wxConfigBase::Get();
  cfg->SetPath(wxT("/GUI"));
  cfg->Write(wxT("mkvmerge_executable"), tc_mkvmerge->GetValue());
  cfg->Write(wxT("process_priority"), cob_priority->GetValue());
  cfg->Write(wxT("autoset_output_filename"),
             cb_autoset_output_filename->IsChecked());
  cfg->Flush();
}

void
tab_settings::save(wxConfigBase *cfg) {
}

void
tab_settings::load(wxConfigBase *cfg) {
}

bool
tab_settings::validate_settings() {
  return true;
}

void
tab_settings::query_mkvmerge_capabilities() {
  wxString tmp;
  wxArrayString output;
  vector<wxString> parts;
  int result, i;

  tmp = wxT("\"") + mkvmerge_path + wxT("\" --capabilities");
#if defined(SYS_WINDOWS)
  result = wxExecute(tmp, output);
#else
  // Workaround for buggy behaviour of some wxWindows/GTK combinations.
  wxProcess *process;
  wxInputStream *out;
  int c;
  string tmps;

  process = new wxProcess(this, 1);
  process->Redirect();
  result = wxExecute(tmp, wxEXEC_ASYNC, process);
  if (result == 0)
    return;
  out = process->GetInputStream();
  tmps = "";
  while (1) {
    if (!out->Eof()) {
      c = out->GetC();
      if (c == '\n') {
        output.Add(wxU(tmps.c_str()));
        tmps = "";
      } else if (c < 0)
        break;
      else if (c != '\r')
        tmps += c;
    } else
      break;
  }
  if (tmps.length() > 0)
    output.Add(wxU(tmps.c_str()));
  result = 0;
#endif

  if (result == 0) {
    capabilities.clear();
    for (i = 0; i < output.Count(); i++) {
      tmp = output[i];
      strip(tmp);
      parts = split(tmp, wxU("="), 2);
      if (parts.size() == 1)
        capabilities[parts[0]] = wxT("true");
      else
        capabilities[parts[0]] = parts[1];
    }
  }
}

IMPLEMENT_CLASS(tab_settings, wxPanel);
BEGIN_EVENT_TABLE(tab_settings, wxPanel)
  EVT_BUTTON(ID_B_BROWSEMKVMERGE, tab_settings::on_browse)
  EVT_COMBOBOX(ID_COB_PRIORITY, tab_settings::on_xyz_selected)
  EVT_CHECKBOX(ID_CB_AUTOSET_OUTPUT_FILENAME, tab_settings::on_xyz_selected)
END_EVENT_TABLE();
