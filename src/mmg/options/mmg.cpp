/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "options" dialog -- mmg tab

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

#include "common/common_pch.h"
#include "common/strings/editing.h"
#include "common/translation.h"
#include "common/wx.h"
#include "mmg/cli_options_dlg.h"
#include "mmg/mmg_dialog.h"
#include "mmg/mmg.h"
#include "mmg/options/mmg.h"

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

optdlg_mmg_tab::optdlg_mmg_tab(wxWindow *parent,
                               mmg_options_t &options)
  : optdlg_base_tab(parent, options)
{
  // Create the controls.
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

  cb_filenew_after_successful_mux  = new wxCheckBox(this, ID_CB_NEW_AFTER_SUCCESSFUL_MUX, Z("Clear inputs after a successful muxing run"));

  cb_on_top = new wxCheckBox(this, ID_CB_ON_TOP, Z("Always on top"));

  cb_warn_usage = new wxCheckBox(this, ID_CB_WARN_USAGE, Z("Warn about possible incorrect usage of mmg"));
  cb_warn_usage->SetToolTip(TIP("If checked mmg will warn if it thinks that "
                                "you're using it incorrectly. Such warnings "
                                "are shown at least once even if you turn "
                                "this feature off."));

  cb_disable_header_removal_compression = new wxCheckBox(this, ID_CB_DISABLE_HRC, Z("Disable header removal compression for audio and video tracks by default"));
  cb_disable_header_removal_compression->SetToolTip(TIP("If checked mmg will set the 'compression' drop down box to 'none' for all audio and video tracks by default. "
                                                        "The user can still change the compression setting afterwards."));

#if defined(HAVE_CURL_EASY_H)
  cb_check_for_updates = new wxCheckBox(this, ID_CB_CHECK_FOR_UPDATES, Z("Check online for the latest release"));
  cb_check_for_updates->SetToolTip(TIP("Check online whether or not a new release of MKVToolNix is available on the home page. "
                                       "Will only check when mmg starts and at most once a day. "
                                       "No information is transmitted to the server."));
#endif  // defined(HAVE_CURL_EASY_H)

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
    wxString curr_entry = wxU(boost::format("%1% (%2%)") % translation->m_translated_name % translation->m_english_name);
    sorted_entries.push_back(locale_sorter_t(curr_entry, translation->get_locale()));

    if (   (select_locale.IsEmpty() && (translation->m_english_name == "English"))
        || (ui_locale_lower == downcase(translation->get_locale())))
      select_locale = curr_entry;

    ++translation;
  }

  wxLogMessage(wxT("Locale selection logic: select_locale %s uu_locale_lower %s translation_c::get_default_ui_locale() %s app->m_ui_locale %s"),
               select_locale.c_str(), wxUCS(ui_locale_lower), wxUCS(translation_c::get_default_ui_locale()), wxUCS(app->m_ui_locale));

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
  cb_disable_header_removal_compression->SetValue(m_options.disable_a_v_compression);

  rb_odm_input_file->SetValue(m_options.output_directory_mode == ODM_FROM_FIRST_INPUT_FILE);
  rb_odm_previous->SetValue(m_options.output_directory_mode == ODM_PREVIOUS);
  rb_odm_fixed->SetValue(m_options.output_directory_mode == ODM_FIXED);

#if defined(HAVE_LIBINTL_H)
  set_combobox_selection(cob_ui_language, select_locale);
#endif  // HAVE_LIBINTL_H

#if defined(HAVE_CURL_EASY_H)
  cb_check_for_updates->SetValue(m_options.check_for_updates);
#endif  // defined(HAVE_CURL_EASY_H)

  enable_output_filename_controls(m_options.autoset_output_filename);

  // Create the layout.

  wxBoxSizer *siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->AddSpacer(5);

  siz_all->Add(new wxStaticText(this, wxID_ANY, Z("mmg options")), 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_all->AddSpacer(5);
  siz_all->Add(new wxStaticLine(this),                             0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_all->AddSpacer(5);

  wxBoxSizer *siz_line;
#if defined(HAVE_LIBINTL_H)
  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(st_ui_language, 0, wxALIGN_CENTER_VERTICAL);
  siz_line->Add(cob_ui_language, 0, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);

  siz_all->Add(siz_line, 0, wxGROW | wxLEFT, 5);
  siz_all->AddSpacer(5);
#endif  // HAVE_LIBINTL_H

  siz_all->Add(cb_autoset_output_filename, 0, wxLEFT, 5);
  siz_all->AddSpacer(5);

#if defined(SYS_WINDOWS)
  int left_offset = 16;
#else
  int left_offset = 24;
#endif

  siz_all->Add(rb_odm_input_file, 0, wxLEFT, left_offset);
  siz_all->AddSpacer(5);

  siz_all->Add(rb_odm_previous, 0, wxLEFT, left_offset);
  siz_all->AddSpacer(5);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(rb_odm_fixed, 0, wxALIGN_CENTER_VERTICAL, 0);
  siz_line->Add(tc_output_directory, 1, wxALIGN_CENTER_VERTICAL | wxLEFT, 5);
  siz_line->Add(b_browse_output_directory, 0, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT, 5);

  siz_all->Add(siz_line, 0, wxGROW | wxLEFT, left_offset);
  siz_all->AddSpacer(5);

  siz_all->Add(cb_ask_before_overwriting, 0, wxLEFT, 5);
  siz_all->AddSpacer(5);

  siz_all->Add(cb_set_delay_from_filename, 0, wxLEFT, 5);
  siz_all->AddSpacer(5);

  siz_all->Add(cb_filenew_after_add_to_jobqueue, 0, wxLEFT, 5);
  siz_all->AddSpacer(5);

  siz_all->Add(cb_filenew_after_successful_mux, 0, wxLEFT, 5);
  siz_all->AddSpacer(5);

#if defined(SYS_WINDOWS)
  siz_all->Add(cb_on_top, 0, wxLEFT, 5);
  siz_all->AddSpacer(5);
#else
  cb_on_top->Show(false);
#endif

  siz_all->Add(cb_warn_usage, 0, wxLEFT, 5);
  siz_all->AddSpacer(5);

  siz_all->Add(cb_disable_header_removal_compression, 0, wxLEFT, 5);
  siz_all->AddSpacer(5);

#if defined(HAVE_CURL_EASY_H)
  siz_all->Add(cb_check_for_updates, 0, wxLEFT, 5);
  siz_all->AddSpacer(5);
#endif  // defined(HAVE_CURL_EASY_H)

  siz_all->Add(cb_gui_debugging, 0, wxLEFT, 5);
  siz_all->AddSpacer(5);

  SetSizer(siz_all);
}

void
optdlg_mmg_tab::on_browse_output_directory(wxCommandEvent &evt) {
  wxDirDialog dlg(this, Z("Choose the output directory"), tc_output_directory->GetValue());

  if (dlg.ShowModal() == wxID_OK)
    tc_output_directory->SetValue(dlg.GetPath());
}

void
optdlg_mmg_tab::on_autoset_output_filename_selected(wxCommandEvent &evt) {
  enable_output_filename_controls(cb_autoset_output_filename->IsChecked());
}

void
optdlg_mmg_tab::enable_output_filename_controls(bool enable) {
  bool odm_is_fixed = rb_odm_fixed->GetValue();

  rb_odm_input_file->Enable(enable);
  rb_odm_previous->Enable(enable);
  rb_odm_fixed->Enable(enable);
  tc_output_directory->Enable(enable && odm_is_fixed);
  b_browse_output_directory->Enable(enable && odm_is_fixed);
}

std::string
optdlg_mmg_tab::get_selected_ui_language() {
#if defined(HAVE_LIBINTL_H)
  return m_sorted_locales[cob_ui_language->GetSelection()];
#else
  return empty_string;
#endif
}

void
optdlg_mmg_tab::save_options() {
  m_options.output_directory              = tc_output_directory->GetValue();
  m_options.autoset_output_filename       = cb_autoset_output_filename->IsChecked();
  m_options.ask_before_overwriting        = cb_ask_before_overwriting->IsChecked();
  m_options.on_top                        = cb_on_top->IsChecked();
  m_options.filenew_after_add_to_jobqueue = cb_filenew_after_add_to_jobqueue->IsChecked();
  m_options.filenew_after_successful_mux  = cb_filenew_after_successful_mux->IsChecked();
  m_options.warn_usage                    = cb_warn_usage->IsChecked();
  m_options.gui_debugging                 = cb_gui_debugging->IsChecked();
  m_options.set_delay_from_filename       = cb_set_delay_from_filename->IsChecked();
  m_options.disable_a_v_compression       = cb_disable_header_removal_compression->IsChecked();
  m_options.output_directory_mode         = rb_odm_input_file->GetValue() ? ODM_FROM_FIRST_INPUT_FILE
                                          : rb_odm_previous->GetValue()   ? ODM_PREVIOUS
                                          :                                 ODM_FIXED;
#if defined(HAVE_CURL_EASY_H)
  m_options.check_for_updates             = cb_check_for_updates->IsChecked();
#endif  // defined(HAVE_CURL_EASY_H)

#if defined(HAVE_LIBINTL_H)
  std::string new_ui_locale = get_selected_ui_language();

  if (downcase(new_ui_locale) != downcase(app->m_ui_locale)) {
    app->m_ui_locale  = new_ui_locale;

    wxConfigBase *cfg = wxConfigBase::Get();
    cfg->SetPath(wxT("/GUI"));
    cfg->Write(wxT("ui_locale"), wxString(new_ui_locale.c_str(), wxConvUTF8));

    app->init_ui_locale();
    mdlg->translate_ui();
    cli_options_dlg::clear_cli_option_list();
  }
#endif  // HAVE_LIBINTL_H
}

wxString
optdlg_mmg_tab::get_title() {
  return Z("mmg");
}

IMPLEMENT_CLASS(optdlg_mmg_tab, optdlg_base_tab);
BEGIN_EVENT_TABLE(optdlg_mmg_tab, optdlg_base_tab)
  EVT_BUTTON(ID_B_BROWSE_OUTPUT_DIRECTORY,    optdlg_mmg_tab::on_browse_output_directory)
  EVT_CHECKBOX(ID_CB_AUTOSET_OUTPUT_FILENAME, optdlg_mmg_tab::on_autoset_output_filename_selected)
  EVT_RADIOBUTTON(ID_RB_ODM_INPUT_FILE,       optdlg_mmg_tab::on_autoset_output_filename_selected)
  EVT_RADIOBUTTON(ID_RB_ODM_PREVIOUS,         optdlg_mmg_tab::on_autoset_output_filename_selected)
  EVT_RADIOBUTTON(ID_RB_ODM_FIXED,            optdlg_mmg_tab::on_autoset_output_filename_selected)
END_EVENT_TABLE();
