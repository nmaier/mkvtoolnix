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
 * "settings" tab
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
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
#include "mmg_dialog.h"
#include "tab_settings.h"

tab_settings::tab_settings(wxWindow *parent):
  wxPanel(parent, -1, wxDefaultPosition, wxSize(100, 400),
          wxTAB_TRAVERSAL) {
  wxStaticBoxSizer *siz_mmg_exe, *siz_misc, *siz_about;
  wxBoxSizer *siz_all, *siz_proc_prio;
  wxButton *b_browse;

  siz_mmg_exe =
    new wxStaticBoxSizer(new wxStaticBox(this, -1,
                                         wxT("mkvmerge executable")),
                         wxHORIZONTAL);

  tc_mkvmerge =
    new wxTextCtrl(this, ID_TC_MKVMERGE, wxT(""), wxDefaultPosition,
                   wxDefaultSize, wxTE_READONLY);
  siz_mmg_exe->Add(tc_mkvmerge, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxALL,
                   5);
  b_browse = new wxButton(this, ID_B_BROWSEMKVMERGE, wxT("Browse"));
  siz_mmg_exe->Add(b_browse, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);

  siz_misc =
    new wxStaticBoxSizer(new wxStaticBox(this, -1,
                                         wxT("Miscellaneous options")),
                         wxVERTICAL);
  siz_misc->Add(0, 5, 0, 0, 0);
  siz_proc_prio = new wxBoxSizer(wxHORIZONTAL);
  siz_proc_prio->Add(new wxStaticText(this, -1, wxT("Process priority:")),
                     0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);
  cob_priority =
    new wxComboBox(this, ID_COB_PRIORITY, wxT(""), wxDefaultPosition,
                   wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);
  cob_priority->SetToolTip(TIP("Sets the priority that mkvmerge will run "
                               "with."));
#if defined(SYS_WINDOWS)
  cob_priority->Append(wxT("highest"));
  cob_priority->Append(wxT("higher"));
#endif
  cob_priority->Append(wxT("normal"));
  cob_priority->Append(wxT("lower"));
  cob_priority->Append(wxT("lowest"));
  siz_proc_prio->Add(cob_priority, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
  siz_misc->Add(siz_proc_prio);
  siz_misc->Add(0, 5, 0, 0, 0);

  cb_autoset_output_filename =
    new wxCheckBox(this, ID_CB_AUTOSET_OUTPUT_FILENAME,
                   wxT("Auto-set output filename"));
  cb_autoset_output_filename->
    SetToolTip(TIP("If checked mmg will automatically set the output filename "
                   "if it hasn't been set already. This happens when you add "
                   "a file. It will be set to the same name as the "
                   "input file but with the extension '.mkv'. If unset mmg "
                   "will not touch the output filename."));
  siz_misc->Add(cb_autoset_output_filename, 0, wxLEFT, 5);
  siz_misc->Add(0, 5, 0, 0, 0);

  cb_ask_before_overwriting =
    new wxCheckBox(this, ID_CB_ASK_BEFORE_OVERWRITING,
                   wxT("Ask before overwriting things (files, jobs)"));
  cb_ask_before_overwriting->SetToolTip(TIP("If checked mmg will ask for "
                                            "confirmation before overwriting "
                                            "existing files, or before adding "
                                            "a new job if there's an old job "
                                            "whose description matches the "
                                            "new one."));
  siz_misc->Add(cb_ask_before_overwriting, 0, wxLEFT, 5);

  siz_misc->Add(0, 5, 0, 0, 0);
  cb_filenew_after_add_to_jobqueue =
    new wxCheckBox(this, ID_CB_NEW_AFTER_ADD_TO_JOBQUEUE,
                   wxT("Clear inputs after adding a job to the job queue"));
  siz_misc->Add(cb_filenew_after_add_to_jobqueue,0, wxLEFT, 5);

  cb_on_top = new wxCheckBox(this, ID_CB_ON_TOP, wxT("Always on top"));
#if defined(SYS_WINDOWS)
  siz_misc->Add(0, 5, 0, 0, 0);
  siz_misc->Add(cb_on_top, 0, wxLEFT, 5);
#else
  cb_on_top->Show(false);
#endif
  siz_misc->Add(0, 5, 0, 0, 0);

  siz_about = new wxStaticBoxSizer(new wxStaticBox(this, -1, wxT("About")),
                                   wxHORIZONTAL);
  siz_about->Add(new wxStaticBitmap(this, -1, wxBitmap(matroskalogo_big_xpm)),
                 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
  siz_about->Add(new wxStaticText(this, wxID_STATIC,
                                  wxT("mkvmerge GUI v" VERSION " ('"
                                      VERSIONNAME "')\n"
                                      "built on " __DATE__ " " __TIME__ "\n\n"
                                      "This GUI was written by Moritz Bunkus "
                                      "<moritz@bunkus.org>\n"
                                      "Based on mmg by Florian Wagner "
                                      "<flo.wagner@gmx.de>\n"
                                      "mkvmerge GUI is licensed under the "
                                      "GPL.\n"
                                      "http://www.bunkus.org/videotools/"
                                      "mkvtoolnix/")),
                 0, wxLEFT | wxRIGHT, 5);

  siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->Add(siz_mmg_exe, 0, wxGROW | wxALL, 5);
  siz_all->Add(siz_misc, 0, wxGROW | wxALL, 5);
  siz_all->Add(0, 0, 1, wxGROW, 0);
  siz_all->Add(siz_about, 0, wxGROW | wxALL, 5);
  SetSizer(siz_all);

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
    wxString file_name;

    file_name = dlg.GetPath().AfterLast('/').AfterLast('\\').Lower();
    if ((file_name == wxT("mmg.exe")) || (file_name == wxT("mmg"))) {
      wxMessageBox(wxT("Please do not select 'mmg' itself as the 'mkvmerge' "
                       "executable."), wxT("Wrong file chosen"),
                   wxOK | wxCENTER | wxICON_ERROR);
      return;
    }
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
tab_settings::on_on_top_selected(wxCommandEvent &evt) {
  save_preferences();
  set_on_top(cb_on_top->IsChecked());
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
  if (!cfg->Read(wxT("ask_before_overwriting"), &b))
    b = true;
  cb_ask_before_overwriting->SetValue(b);
  if (!cfg->Read(wxT("filenew_after_add_to_jobqueue"), &b))
    b = false;
  cb_filenew_after_add_to_jobqueue->SetValue(b);
  if (!cfg->Read(wxT("on_top"), &b))
    b = false;
  cb_on_top->SetValue(b);
  set_on_top(b);
}

void
tab_settings::save_preferences() {
  wxConfig *cfg = (wxConfig *)wxConfigBase::Get();
  cfg->SetPath(wxT("/GUI"));
  cfg->Write(wxT("mkvmerge_executable"), tc_mkvmerge->GetValue());
  cfg->Write(wxT("process_priority"), cob_priority->GetValue());
  cfg->Write(wxT("autoset_output_filename"),
             cb_autoset_output_filename->IsChecked());
  cfg->Write(wxT("ask_before_overwriting"),
             cb_ask_before_overwriting->IsChecked());
  cfg->Write(wxT("filenew_after_add_to_jobqueue"),
             cb_filenew_after_add_to_jobqueue->IsChecked());
  cfg->Write(wxT("on_top"), cb_on_top->IsChecked());
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
  c = 0;
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
  EVT_CHECKBOX(ID_CB_NEW_AFTER_ADD_TO_JOBQUEUE, tab_settings::on_xyz_selected)
  EVT_CHECKBOX(ID_CB_ON_TOP, tab_settings::on_on_top_selected)
END_EVENT_TABLE();
