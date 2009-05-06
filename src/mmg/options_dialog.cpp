/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "options" dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <algorithm>
#include <string>
#include <vector>

#include <wx/wxprec.h>

#include <wx/wx.h>
#include <wx/config.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/process.h>
#include <wx/statline.h>

#include "common/common.h"
#include "common/string_editing.h"
#include "common/translation.h"
#include "common/wxcommon.h"
#include "mmg/mmg_dialog.h"
#include "mmg/mmg.h"
#include "mmg/options_dialog.h"

struct locale_sorter_t {
  wxString display_val;
  std::string locale;

  locale_sorter_t(const wxString &n_display_val,
                  const std::string &n_locale)
    : display_val(n_display_val)
    , locale(n_locale)
  {
  };

  bool operator <(const locale_sorter_t &cmp) const {
    return display_val < cmp.display_val;
  };
};

options_dialog::options_dialog(wxWindow *parent,
                               mmg_options_t &options)
  : wxDialog(parent, -1, Z("Options"), wxDefaultPosition, wxDefaultSize)
  , m_options(options) {

  wxStaticBox *sb_mmg, *sb_mkvmerge;
  wxStaticText *st_priority, *st_mkvmerge;
  wxButton *b_browse;

  if (cob_priority_translations.entries.empty()) {
#ifdef SYS_WINDOWS
    cob_priority_translations.add(wxT("highest"), Z("highest"));
    cob_priority_translations.add(wxT("higher"),  Z("higher"));
#endif
    cob_priority_translations.add(wxT("normal"),  Z("normal"));
    cob_priority_translations.add(wxT("lower"),   Z("lower"));
    cob_priority_translations.add(wxT("lowest"),  Z("lowest"));
  }

  // Create the controls.

  sb_mkvmerge = new wxStaticBox(this, -1, Z("mkvmerge options"));

  st_mkvmerge  = new wxStaticText(this, -1, Z("mkvmerge executable"));
  tc_mkvmerge  = new wxTextCtrl(this, ID_TC_MKVMERGE, m_options.mkvmerge, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
  b_browse     = new wxButton(this, ID_B_BROWSEMKVMERGE, Z("Browse"));

  st_priority  = new wxStaticText(this, -1, Z("Process priority:"));
  cob_priority = new wxMTX_COMBOBOX_TYPE(this, ID_COB_PRIORITY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);

  cob_priority->SetToolTip(TIP("Sets the priority that mkvmerge will run with."));

  int i;
  for (i = 0; cob_priority_translations.entries.size() > i; ++i)
    cob_priority->Append(cob_priority_translations.entries[i].translated);

  sb_mmg = new wxStaticBox(this, -1, Z("mmg options"));

  cb_autoset_output_filename = new wxCheckBox(this, ID_CB_AUTOSET_OUTPUT_FILENAME, Z("Auto-set output filename"));
  cb_autoset_output_filename->SetToolTip(TIP("If checked mmg will automatically set the output filename "
                                             "if it hasn't been set already. This happens when you add "
                                             "the first file. If unset mmg will not touch the output filename."));

  rb_odm_input_file = new wxRadioButton(this, ID_RB_ODM_INPUT_FILE, Z("Same directory as the first input file's"), wxDefaultPosition, wxDefaultSize, wxRB_GROUP);
  rb_odm_previous   = new wxRadioButton(this, ID_RB_ODM_PREVIOUS, Z("Use the previous output directory"));
  rb_odm_fixed      = new wxRadioButton(this, ID_RB_ODM_FIXED, Z("Use this directory:"));

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

  cb_set_delay_from_filename = new wxCheckBox(this, ID_CB_SET_DELAY_FROM_FILENAME, Z("Set the delay input field from the file name"));
  cb_set_delay_from_filename->SetToolTip(TIP("When a file is added its name is scanned. If it contains "
                                             "the word 'DELAY' followed by a number then this number "
                                             "is automatically put into the 'delay' input field for "
                                             "any audio track found in the file."));

  cb_filenew_after_add_to_jobqueue = new wxCheckBox(this, ID_CB_NEW_AFTER_ADD_TO_JOBQUEUE, Z("Clear inputs after adding a job to the job queue"));

  cb_filenew_after_successful_mux = new wxCheckBox(this, ID_CB_NEW_AFTER_SUCCESSFUL_MUX, Z("Clear inputs after a successful muxing run"));

  cb_on_top = new wxCheckBox(this, ID_CB_ON_TOP, Z("Always on top"));

  cb_warn_usage = new wxCheckBox(this, ID_CB_WARN_USAGE, Z("Warn about possible incorrect usage of mmg"));
  cb_warn_usage->SetToolTip(TIP("If checked mmg will warn if it thinks that "
                                "you're using it incorrectly. Such warnings "
                                "are shown at least once even if you turn "
                                "this feature off."));

  cb_gui_debugging = new wxCheckBox(this, ID_CB_GUI_DEBUGGING, Z("Show mmg's debug window"));
  cb_gui_debugging->SetToolTip(TIP("Shows mmg's debug window in which debug messages will appear. "
                                   "This is only useful if you're helping the author debug a problem in mmg."));

#if defined(HAVE_LIBINTL_H)
  wxStaticText *st_ui_language = new wxStaticText(this, -1, Z("Interface language:"));
  cob_ui_language = new wxMTX_COMBOBOX_TYPE(this, ID_COB_UI_LANGUAGE,  wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);

  std::string ui_locale_lower = downcase(app->m_ui_locale);
  std::vector<translation_c>::iterator translation = translation_c::ms_available_translations.begin();
  wxString select_locale;
  std::vector<locale_sorter_t> sorted_entries;
  while (translation != translation_c::ms_available_translations.end()) {
    wxString curr_entry = wxCS2WS((boost::format("%1% (%2%)") % translation->m_translated_name % translation->m_english_name).str());
    sorted_entries.push_back(locale_sorter_t(curr_entry, translation->get_locale()));

    if (   (select_locale.IsEmpty() && (translation->m_english_name == "English"))
        || (ui_locale_lower == downcase(translation->get_locale())))
      select_locale = curr_entry;

    ++translation;
  }

  wxLogMessage(wxT("Locale selection logic: select_locale %s uu_locale_lower %s translation_c::get_default_ui_locale() %s app->m_ui_locale %s"),
               select_locale.c_str(), wxCS2WS(ui_locale_lower), wxCS2WS(translation_c::get_default_ui_locale()), wxCS2WS(app->m_ui_locale));

  std::sort(sorted_entries.begin(), sorted_entries.end());

  std::vector<locale_sorter_t>::iterator locale_entry = sorted_entries.begin();
  while (locale_entry != sorted_entries.end()) {
    cob_ui_language->Append(locale_entry->display_val);
    m_sorted_locales.push_back(locale_entry->locale);
    ++locale_entry;
  }
#endif  // HAVE_LIBINTL_H

  // Set the defaults.

  cb_autoset_output_filename->SetValue(m_options.autoset_output_filename);
  cb_ask_before_overwriting->SetValue(m_options.ask_before_overwriting);
  cb_on_top->SetValue(m_options.on_top);
  cb_filenew_after_add_to_jobqueue->SetValue(m_options.filenew_after_add_to_jobqueue);
  cb_filenew_after_successful_mux->SetValue(m_options.filenew_after_successful_mux);
  cb_warn_usage->SetValue(m_options.warn_usage);
  cb_gui_debugging->SetValue(m_options.gui_debugging);
  cb_set_delay_from_filename->SetValue(m_options.set_delay_from_filename);
  select_priority(m_options.priority);

  rb_odm_input_file->SetValue(m_options.output_directory_mode == ODM_FROM_FIRST_INPUT_FILE);
  rb_odm_previous->SetValue(m_options.output_directory_mode == ODM_PREVIOUS);
  rb_odm_fixed->SetValue(m_options.output_directory_mode == ODM_FIXED);

#if defined(HAVE_LIBINTL_H)
  set_combobox_selection(cob_ui_language, select_locale);
#endif  // HAVE_LIBINTL_H

  enable_output_filename_controls(m_options.autoset_output_filename);

  // Create the layout.

  wxStaticBoxSizer *siz_sb;
  wxBoxSizer *siz_all, *siz_line;
  wxFlexGridSizer *siz_fg;
  int left_offset;

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

  siz_all->Add(siz_sb, 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_all->AddSpacer(5);

  siz_sb = new wxStaticBoxSizer(sb_mmg, wxVERTICAL);
  siz_sb->AddSpacer(5);

#if defined(HAVE_LIBINTL_H)
  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(st_ui_language, 0, wxALIGN_CENTER_VERTICAL);
  siz_line->Add(cob_ui_language, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);

  siz_sb->Add(siz_line, 0, wxGROW | wxLEFT, 5);
  siz_sb->AddSpacer(5);
#endif  // HAVE_LIBINTL_H

  siz_sb->Add(cb_autoset_output_filename, 0, wxLEFT, 5);
  siz_sb->AddSpacer(5);

#if defined(SYS_WINDOWS)
  left_offset = 16;
#else
  left_offset = 24;
#endif

  siz_sb->Add(rb_odm_input_file, 0, wxLEFT, left_offset);
  siz_sb->AddSpacer(5);

  siz_sb->Add(rb_odm_previous, 0, wxLEFT, left_offset);
  siz_sb->AddSpacer(5);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(rb_odm_fixed, 0, wxALIGN_CENTER_VERTICAL, 0);
  siz_line->Add(tc_output_directory, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
  siz_line->Add(b_browse_output_directory, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);

  siz_sb->Add(siz_line, 0, wxGROW | wxLEFT, left_offset);
  siz_sb->AddSpacer(5);

  siz_sb->Add(cb_ask_before_overwriting, 0, wxLEFT, 5);
  siz_sb->AddSpacer(5);

  siz_sb->Add(cb_set_delay_from_filename, 0, wxLEFT, 5);
  siz_sb->AddSpacer(5);

  siz_sb->Add(cb_filenew_after_add_to_jobqueue, 0, wxLEFT, 5);
  siz_sb->AddSpacer(5);

  siz_sb->Add(cb_filenew_after_successful_mux, 0, wxLEFT, 5);
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
options_dialog::on_browse_mkvmerge(wxCommandEvent &evt) {
  wxFileDialog dlg(this, Z("Choose the mkvmerge executable"), tc_mkvmerge->GetValue().BeforeLast(PSEP), wxEmptyString,
#ifdef SYS_WINDOWS
                   wxString::Format(Z("Executable files (*.exe)|*.exe|%s"), ALLFILES.c_str()),
#else
                   wxT("All files (*)|*"),
#endif
                   wxFD_OPEN);

  if (dlg.ShowModal() != wxID_OK)
    return;

  wxString file_name(dlg.GetPath().AfterLast('/').AfterLast('\\').Lower());

  if ((file_name == wxT("mmg.exe")) || (file_name == wxT("mmg"))) {
    wxMessageBox(Z("Please do not select 'mmg' itself as the 'mkvmerge' executable."), Z("Wrong file chosen"), wxOK | wxCENTER | wxICON_ERROR);
    return;
  }

  tc_mkvmerge->SetValue(dlg.GetPath());
}

void
options_dialog::on_browse_output_directory(wxCommandEvent &evt) {
  wxDirDialog dlg(this, Z("Choose the output directory"), tc_output_directory->GetValue());

  if (dlg.ShowModal() == wxID_OK)
    tc_output_directory->SetValue(dlg.GetPath());
}

void
options_dialog::on_autoset_output_filename_selected(wxCommandEvent &evt) {
  enable_output_filename_controls(cb_autoset_output_filename->IsChecked());
}

void
options_dialog::enable_output_filename_controls(bool enable) {
  bool odm_is_fixed = rb_odm_fixed->GetValue();

  rb_odm_input_file->Enable(enable);
  rb_odm_previous->Enable(enable);
  rb_odm_fixed->Enable(enable);
  tc_output_directory->Enable(enable && odm_is_fixed);
  b_browse_output_directory->Enable(enable && odm_is_fixed);
}

void
options_dialog::on_ok(wxCommandEvent &evt) {
  m_options.mkvmerge                      = tc_mkvmerge->GetValue();
  m_options.priority                      = get_selected_priority();
  m_options.output_directory              = tc_output_directory->GetValue();
  m_options.autoset_output_filename       = cb_autoset_output_filename->IsChecked();
  m_options.ask_before_overwriting        = cb_ask_before_overwriting->IsChecked();
  m_options.on_top                        = cb_on_top->IsChecked();
  m_options.filenew_after_add_to_jobqueue = cb_filenew_after_add_to_jobqueue->IsChecked();
  m_options.filenew_after_successful_mux  = cb_filenew_after_successful_mux->IsChecked();
  m_options.warn_usage                    = cb_warn_usage->IsChecked();
  m_options.gui_debugging                 = cb_gui_debugging->IsChecked();
  m_options.set_delay_from_filename       = cb_set_delay_from_filename->IsChecked();
  m_options.output_directory_mode         = rb_odm_input_file->GetValue() ? ODM_FROM_FIRST_INPUT_FILE :
                                            rb_odm_previous->GetValue()   ? ODM_PREVIOUS :
                                                                            ODM_FIXED;

#if defined(HAVE_LIBINTL_H)
  std::string new_ui_locale = m_sorted_locales[cob_ui_language->GetSelection()];

  if (downcase(new_ui_locale) != downcase(app->m_ui_locale))
    wxMessageBox(Z("Changing the interface language requires a restart to take effect."), Z("Restart required"), wxOK | wxCENTER | wxICON_INFORMATION);

  app->m_ui_locale  = new_ui_locale;

  wxConfigBase *cfg = wxConfigBase::Get();
  cfg->SetPath(wxT("/GUI"));
  cfg->Write(wxT("ui_locale"), wxString(new_ui_locale.c_str(), wxConvUTF8));
#endif  // HAVE_LIBINTL_H

  EndModal(wxID_OK);
}

void
options_dialog::select_priority(const wxString &priority) {
  cob_priority->SetValue(cob_priority_translations.to_translated(priority));
}

wxString
options_dialog::get_selected_priority() {
  return cob_priority_translations.to_english(cob_priority->GetValue());
}

IMPLEMENT_CLASS(options_dialog, wxDialog);
BEGIN_EVENT_TABLE(options_dialog, wxDialog)
  EVT_BUTTON(ID_B_BROWSEMKVMERGE,             options_dialog::on_browse_mkvmerge)
  EVT_BUTTON(ID_B_BROWSE_OUTPUT_DIRECTORY,    options_dialog::on_browse_output_directory)
  EVT_BUTTON(wxID_OK,                         options_dialog::on_ok)
  EVT_CHECKBOX(ID_CB_AUTOSET_OUTPUT_FILENAME, options_dialog::on_autoset_output_filename_selected)
  EVT_RADIOBUTTON(ID_RB_ODM_INPUT_FILE,       options_dialog::on_autoset_output_filename_selected)
  EVT_RADIOBUTTON(ID_RB_ODM_PREVIOUS,         options_dialog::on_autoset_output_filename_selected)
  EVT_RADIOBUTTON(ID_RB_ODM_FIXED,            options_dialog::on_autoset_output_filename_selected)
END_EVENT_TABLE();
