/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "options" dialog -- output tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/wx.h>
#include <wx/config.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/process.h>
#include <wx/statline.h>

#include "common/strings/editing.h"
#include "common/wx.h"
#include "mmg/mmg_dialog.h"
#include "mmg/mmg.h"
#include "mmg/options/output.h"

optdlg_output_tab::optdlg_output_tab(wxWindow *parent,
                                     mmg_options_t &options)
  : optdlg_base_tab{parent, options}
{
  // Create the controls.
  cb_autoset_output_filename = new wxCheckBox(this, ID_CB_AUTOSET_OUTPUT_FILENAME, Z("Auto-set output filename"));
  cb_autoset_output_filename->SetToolTip(TIP("If checked mmg will automatically set the output filename "
                                             "if it hasn't been set already. This happens when you add "
                                             "the first file. If unset mmg will not touch the output filename."));

  rb_odm_input_file           = new wxRadioButton(this, ID_RB_ODM_INPUT_FILE,           Z("Same directory as the first input file's"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
  rb_odm_parent_of_input_file = new wxRadioButton(this, ID_RB_ODM_PARENT_OF_INPUT_FILE, Z("Parent directory of the first input file"));
  rb_odm_previous             = new wxRadioButton(this, ID_RB_ODM_PREVIOUS,             Z("Use the previous output directory"));
  rb_odm_fixed                = new wxRadioButton(this, ID_RB_ODM_FIXED,                Z("Use this directory:"));

  tc_output_directory       = new wxTextCtrl(this, ID_TC_OUTPUT_DIRECTORY, m_options.output_directory);
  b_browse_output_directory = new wxButton(this, ID_B_BROWSE_OUTPUT_DIRECTORY, Z("Browse"));

  tc_output_directory->SetToolTip(TIP("If left empty then mmg will set the output file name to be in the same directory as the first file added to this job. "
                                      "Otherwise this directory will be used."));

  cb_ask_before_overwriting = new wxCheckBox(this, ID_CB_ASK_BEFORE_OVERWRITING, Z("Ask before overwriting things (files, jobs)"));
  cb_ask_before_overwriting->SetToolTip(TIP("If checked mmg will ask for "
                                            "confirmation before overwriting "
                                            "existing files, or before adding "
                                            "a new job if there's an old job "
                                            "whose description matches the "
                                            "new one."));

  // Set the defaults.

  cb_autoset_output_filename->SetValue(m_options.autoset_output_filename);
  cb_ask_before_overwriting->SetValue(m_options.ask_before_overwriting);

  rb_odm_input_file->          SetValue(m_options.output_directory_mode == ODM_FROM_FIRST_INPUT_FILE);
  rb_odm_parent_of_input_file->SetValue(m_options.output_directory_mode == ODM_PARENT_OF_FIRST_INPUT_FILE);
  rb_odm_previous->            SetValue(m_options.output_directory_mode == ODM_PREVIOUS);
  rb_odm_fixed->               SetValue(m_options.output_directory_mode == ODM_FIXED);

  enable_output_filename_controls(m_options.autoset_output_filename);

  // Create the layout.

  wxBoxSizer *siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->AddSpacer(5);

  siz_all->Add(new wxStaticText(this, wxID_ANY, Z("output options")), 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_all->AddSpacer(5);
  siz_all->Add(new wxStaticLine(this),                                0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_all->AddSpacer(5);

  siz_all->Add(cb_autoset_output_filename, 0, wxLEFT, 5);
  siz_all->AddSpacer(5);

#if defined(SYS_WINDOWS)
  int left_offset = 16;
#else
  int left_offset = 24;
#endif

  siz_all->Add(rb_odm_input_file, 0, wxLEFT, left_offset);
  siz_all->AddSpacer(5);

  siz_all->Add(rb_odm_parent_of_input_file, 0, wxLEFT, left_offset);
  siz_all->AddSpacer(5);

  siz_all->Add(rb_odm_previous, 0, wxLEFT, left_offset);
  siz_all->AddSpacer(5);

  auto siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(rb_odm_fixed, 0, wxALIGN_CENTER_VERTICAL, 0);
  siz_line->Add(tc_output_directory, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
  siz_line->Add(b_browse_output_directory, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);

  siz_all->Add(siz_line, 0, wxGROW | wxLEFT, left_offset);
  siz_all->AddSpacer(5);

  siz_all->Add(cb_ask_before_overwriting, 0, wxLEFT, 5);
  siz_all->AddSpacer(5);

  SetSizer(siz_all);
}

void
optdlg_output_tab::on_browse_output_directory(wxCommandEvent &) {
  wxDirDialog dlg(this, Z("Choose the output directory"), tc_output_directory->GetValue());

  if (dlg.ShowModal() == wxID_OK)
    tc_output_directory->SetValue(dlg.GetPath());
}

void
optdlg_output_tab::on_autoset_output_filename_selected(wxCommandEvent &) {
  enable_output_filename_controls(cb_autoset_output_filename->IsChecked());
}

void
optdlg_output_tab::enable_output_filename_controls(bool enable) {
  bool odm_is_fixed = rb_odm_fixed->GetValue();

  rb_odm_input_file->Enable(enable);
  rb_odm_previous->Enable(enable);
  rb_odm_fixed->Enable(enable);
  tc_output_directory->Enable(enable && odm_is_fixed);
  b_browse_output_directory->Enable(enable && odm_is_fixed);
}

void
optdlg_output_tab::save_options() {
  m_options.output_directory        = tc_output_directory->GetValue();
  m_options.autoset_output_filename = cb_autoset_output_filename->IsChecked();
  m_options.ask_before_overwriting  = cb_ask_before_overwriting->IsChecked();
  m_options.output_directory_mode   = rb_odm_input_file->GetValue()           ? ODM_FROM_FIRST_INPUT_FILE
                                    : rb_odm_parent_of_input_file->GetValue() ? ODM_PARENT_OF_FIRST_INPUT_FILE
                                    : rb_odm_previous->GetValue()             ? ODM_PREVIOUS
                                    :                                           ODM_FIXED;
}

wxString
optdlg_output_tab::get_title() {
  return Z("Output");
}

IMPLEMENT_CLASS(optdlg_output_tab, optdlg_base_tab);
BEGIN_EVENT_TABLE(optdlg_output_tab, optdlg_base_tab)
  EVT_BUTTON(ID_B_BROWSE_OUTPUT_DIRECTORY,        optdlg_output_tab::on_browse_output_directory)
  EVT_CHECKBOX(ID_CB_AUTOSET_OUTPUT_FILENAME,     optdlg_output_tab::on_autoset_output_filename_selected)
  EVT_RADIOBUTTON(ID_RB_ODM_INPUT_FILE,           optdlg_output_tab::on_autoset_output_filename_selected)
  EVT_RADIOBUTTON(ID_RB_ODM_PARENT_OF_INPUT_FILE, optdlg_output_tab::on_autoset_output_filename_selected)
  EVT_RADIOBUTTON(ID_RB_ODM_PREVIOUS,             optdlg_output_tab::on_autoset_output_filename_selected)
  EVT_RADIOBUTTON(ID_RB_ODM_FIXED,                optdlg_output_tab::on_autoset_output_filename_selected)
END_EVENT_TABLE();
