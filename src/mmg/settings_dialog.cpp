/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   "settings" tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "wx/wxprec.h"

#include "wx/wx.h"
#include "wx/config.h"
#include "wx/listctrl.h"
#include "wx/notebook.h"
#include "wx/process.h"
#include "wx/statline.h"

#include "common.h"
#include "mmg.h"
#include "mmg_dialog.h"
#include "settings_dialog.h"

settings_dialog::settings_dialog(wxWindow *parent,
                                 mmg_settings_t &settings):
  wxDialog(parent, -1, wxT("Options"), wxDefaultPosition, wxDefaultSize),
  m_settings(settings) {

  wxStaticBox *sb_mmg, *sb_mkvmerge;
  wxStaticText *st_priority, *st_mkvmerge;
  wxButton *b_browse;

  // Create the controls.

  sb_mkvmerge = new wxStaticBox(this, -1, wxT("mkvmerge options"));

  st_mkvmerge  = new wxStaticText(this, -1, wxT("mkvmerge executable"));
  tc_mkvmerge  = new wxTextCtrl(this, ID_TC_MKVMERGE, m_settings.mkvmerge, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
  b_browse     = new wxButton(this, ID_B_BROWSEMKVMERGE, wxT("Browse"));

  st_priority  = new wxStaticText(this, -1, wxT("Process priority:"));
  cob_priority = new wxComboBox(this, ID_COB_PRIORITY, wxT(""), wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);

  cob_priority->SetToolTip(TIP("Sets the priority that mkvmerge will run with."));

#if defined(SYS_WINDOWS)
  cob_priority->Append(wxT("highest"));
  cob_priority->Append(wxT("higher"));
#endif
  cob_priority->Append(wxT("normal"));
  cob_priority->Append(wxT("lower"));
  cob_priority->Append(wxT("lowest"));

  cb_always_use_simpleblock = new wxCheckBox(this, ID_CB_ALWAYS_USE_SIMPLEBLOCK, wxT("Always use simple blocks"));
  cb_always_use_simpleblock->
    SetToolTip(TIP("Always adds '--engage use_simpleblock' "
                   "to the command line. That way Matroska's "
                   "new 'simple blocks' will be used which "
                   "save a bit of overhead at the cost of "
                   "not being backwards compatible."));


  sb_mmg = new wxStaticBox(this, -1, wxT("mmg options"));

  cb_autoset_output_filename = new wxCheckBox(this, ID_CB_AUTOSET_OUTPUT_FILENAME, wxT("Auto-set output filename"));
  cb_autoset_output_filename->
    SetToolTip(TIP("If checked mmg will automatically set the output filename "
                   "if it hasn't been set already. This happens when you add "
                   "a file. It will be set to the same name as the "
                   "input file but with the extension '.mkv'. If unset mmg "
                   "will not touch the output filename."));

  st_output_directory       = new wxStaticText(this, 0, wxT("Output directory:"));
  tc_output_directory       = new wxTextCtrl(this, ID_TC_OUTPUT_DIRECTORY, m_settings.output_directory);
  b_browse_output_directory = new wxButton(this, ID_B_BROWSE_OUTPUT_DIRECTORY, wxT("Browse"));

  tc_output_directory->SetToolTip(TIP("If left empty then mmg will set the output file name to be in the same directory as the first file added to this job. "
                                      "Otherwise this directory will be used."));

  cb_ask_before_overwriting = new wxCheckBox(this, ID_CB_ASK_BEFORE_OVERWRITING, wxT("Ask before overwriting things (files, jobs)"));
  cb_ask_before_overwriting->SetToolTip(TIP("If checked mmg will ask for "
                                            "confirmation before overwriting "
                                            "existing files, or before adding "
                                            "a new job if there's an old job "
                                            "whose description matches the "
                                            "new one."));

  cb_set_delay_from_filename = new wxCheckBox(this, ID_CB_SET_DELAY_FROM_FILENAME, wxT("Set the delay input field from the file name"));
  cb_set_delay_from_filename->
    SetToolTip(TIP("When a file is added its name is scanned. If it contains "
                   "the word 'DELAY' followed by a number then this number "
                   "is automatically put into the 'delay' input field for "
                   "any audio track found in the file."));

  cb_filenew_after_add_to_jobqueue = new wxCheckBox(this, ID_CB_NEW_AFTER_ADD_TO_JOBQUEUE, wxT("Clear inputs after adding a job to the job queue"));

  cb_on_top = new wxCheckBox(this, ID_CB_ON_TOP, wxT("Always on top"));

  cb_warn_usage = new wxCheckBox(this, ID_CB_WARN_USAGE, wxT("Warn about possible incorrect usage of mmg"));
  cb_warn_usage->SetToolTip(TIP("If checked mmg will warn if it thinks that "
                                "you're using it incorrectly. Such warnings "
                                "are shown at least once even if you turn "
                                "this feature off."));

  cb_gui_debugging = new wxCheckBox(this, ID_CB_GUI_DEBUGGING, wxT("Show mmg's debug window"));
  cb_gui_debugging->SetToolTip(TIP("Shows mmg's debug window in which "
                                   "debug messages will appear. This is only "
                                   "useful if you're helping the author "
                                   "debug a problem in mmg."));

  // Set the defaults.

  cb_autoset_output_filename->SetValue(m_settings.autoset_output_filename);
  cb_ask_before_overwriting->SetValue(m_settings.ask_before_overwriting);
  cb_on_top->SetValue(m_settings.on_top);
  cb_filenew_after_add_to_jobqueue->SetValue(m_settings.filenew_after_add_to_jobqueue);
  cb_warn_usage->SetValue(m_settings.warn_usage);
  cb_gui_debugging->SetValue(m_settings.gui_debugging);
  cb_always_use_simpleblock->SetValue(m_settings.always_use_simpleblock);
  cb_set_delay_from_filename->SetValue(m_settings.set_delay_from_filename);
  cob_priority->SetValue(m_settings.priority);

  enable_output_filename_controls(m_settings.autoset_output_filename);

  // Create the layout.

  wxStaticBoxSizer *siz_sb;
  wxBoxSizer *siz_all, *siz_line;
  wxFlexGridSizer *siz_fg;

  siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->AddSpacer(5);


  siz_sb = new wxStaticBoxSizer(sb_mkvmerge, wxVERTICAL);

  siz_fg = new wxFlexGridSizer(3);
  siz_fg->AddGrowableCol(1);

  siz_fg->Add(st_mkvmerge, 0, wxALIGN_CENTER_VERTICAL, 0);
  siz_fg->Add(tc_mkvmerge, 1, wxGROW | wxALIGN_CENTER_VERTICAL | wxTOP | wxBOTTOM | wxLEFT, 5);
  siz_fg->Add(b_browse, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);

  siz_fg->Add(st_priority, 0, wxALIGN_CENTER_VERTICAL, 0);
  siz_fg->Add(cob_priority, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);

  siz_sb->Add(siz_fg, 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_sb->AddSpacer(5);

  siz_sb->Add(cb_always_use_simpleblock, 0, wxLEFT, 5);
  siz_sb->AddSpacer(5);

  siz_all->Add(siz_sb, 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_all->AddSpacer(5);


  siz_sb = new wxStaticBoxSizer(sb_mmg, wxVERTICAL);
  siz_sb->AddSpacer(5);

  siz_sb->Add(cb_autoset_output_filename, 0, wxLEFT, 5);
  siz_sb->AddSpacer(5);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->AddSpacer(32);
  siz_line->Add(st_output_directory, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
  siz_line->Add(tc_output_directory, 1, wxALIGN_CENTER_VERTICAL | wxGROW | wxRIGHT, 5);
  siz_line->Add(b_browse_output_directory, 0, wxALIGN_CENTER_VERTICAL);

  siz_sb->Add(siz_line, 1, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_sb->AddSpacer(5);

  siz_sb->Add(cb_ask_before_overwriting, 0, wxLEFT, 5);
  siz_sb->AddSpacer(5);

  siz_sb->Add(cb_set_delay_from_filename, 0, wxLEFT, 5);
  siz_sb->AddSpacer(5);

  siz_sb->Add(cb_filenew_after_add_to_jobqueue, 0, wxLEFT, 5);
  siz_sb->AddSpacer(5);

#if defined(SYS_WINDOWS)
  siz_sb->Add(cb_on_top, 0, wxLEFT, 5);
  siz_sb->AddSpacer(5);
#else
  cb_on_top->Show(false);
#endif

  siz_sb->Add(cb_warn_usage, 0, wxLEFT, 5);
  siz_sb->AddSpacer(5);

  siz_sb->Add(cb_gui_debugging, 0, wxLEFT, 5);
  siz_sb->AddSpacer(5);

  siz_all->Add(siz_sb, 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_all->AddSpacer(5);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->AddStretchSpacer();
  siz_line->Add(CreateStdDialogButtonSizer(wxOK | wxCANCEL), 0, 0, 0);
  siz_line->AddSpacer(5);

  siz_all->Add(siz_line, 0, wxGROW, 0);
  siz_all->AddSpacer(5);

  SetSizer(siz_all);

  siz_all->SetMinSize(wxSize(600, -1));
  siz_all->SetSizeHints(this);
}

void
settings_dialog::on_browse_mkvmerge(wxCommandEvent &evt) {
  wxFileDialog dlg(this, wxT("Choose the mkvmerge executable"), tc_mkvmerge->GetValue().BeforeLast(PSEP), wxT(""),
#ifdef SYS_WINDOWS
                   wxT("Executable files (*.exe)|*.exe|" ALLFILES),
#else
                   wxT("All files (*)|*"),
#endif
                   wxOPEN);

  if (dlg.ShowModal() != wxID_OK)
    return;

  wxString file_name(dlg.GetPath().AfterLast('/').AfterLast('\\').Lower());

  if ((file_name == wxT("mmg.exe")) || (file_name == wxT("mmg"))) {
    wxMessageBox(wxT("Please do not select 'mmg' itself as the 'mkvmerge' executable."), wxT("Wrong file chosen"), wxOK | wxCENTER | wxICON_ERROR);
    return;
  }

  tc_mkvmerge->SetValue(dlg.GetPath());
}

void
settings_dialog::on_browse_output_directory(wxCommandEvent &evt) {
  wxDirDialog dlg(this, wxT("Choose the output directory"), tc_output_directory->GetValue());

  if (dlg.ShowModal() == wxID_OK)
    tc_output_directory->SetValue(dlg.GetPath());
}

void
settings_dialog::on_autoset_output_filename_selected(wxCommandEvent &evt) {
  enable_output_filename_controls(cb_autoset_output_filename->IsChecked());
}

void
settings_dialog::enable_output_filename_controls(bool enable) {
  st_output_directory->Enable(enable);
  tc_output_directory->Enable(enable);
  b_browse_output_directory->Enable(enable);
}

void
settings_dialog::on_ok(wxCommandEvent &evt) {
  m_settings.mkvmerge                      = tc_mkvmerge->GetValue();
  m_settings.priority                      = cob_priority->GetStringSelection();
  m_settings.output_directory              = tc_output_directory->GetValue();
  m_settings.autoset_output_filename       = cb_autoset_output_filename->IsChecked();
  m_settings.ask_before_overwriting        = cb_ask_before_overwriting->IsChecked();
  m_settings.on_top                        = cb_on_top->IsChecked();
  m_settings.filenew_after_add_to_jobqueue = cb_filenew_after_add_to_jobqueue->IsChecked();
  m_settings.warn_usage                    = cb_warn_usage->IsChecked();
  m_settings.gui_debugging                 = cb_gui_debugging->IsChecked();
  m_settings.always_use_simpleblock        = cb_always_use_simpleblock->IsChecked();
  m_settings.set_delay_from_filename       = cb_set_delay_from_filename->IsChecked();

  EndModal(wxID_OK);
}

IMPLEMENT_CLASS(settings_dialog, wxDialog);
BEGIN_EVENT_TABLE(settings_dialog, wxDialog)
  EVT_BUTTON(ID_B_BROWSEMKVMERGE, settings_dialog::on_browse_mkvmerge)
  EVT_BUTTON(ID_B_BROWSE_OUTPUT_DIRECTORY, settings_dialog::on_browse_output_directory)
  EVT_BUTTON(wxID_OK, settings_dialog::on_ok)
  EVT_CHECKBOX(ID_CB_AUTOSET_OUTPUT_FILENAME, settings_dialog::on_autoset_output_filename_selected)
END_EVENT_TABLE();
