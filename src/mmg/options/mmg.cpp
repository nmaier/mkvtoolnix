/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "options" dialog -- mmg tab

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
#include "common/strings/parsing.h"
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
  cb_set_delay_from_filename = new wxCheckBox(this, ID_CB_SET_DELAY_FROM_FILENAME, Z("Set the delay input field from the file name"));
  cb_set_delay_from_filename->SetToolTip(TIP("When a file is added its name is scanned. If it contains "
                                             "the word 'DELAY' followed by a number then this number "
                                             "is automatically put into the 'delay' input field for "
                                             "any audio track found in the file."));

  cb_filenew_after_add_to_jobqueue = new wxCheckBox(this, ID_CB_NEW_AFTER_ADD_TO_JOBQUEUE, Z("Clear inputs after adding a job to the job queue"));

  cb_filenew_after_successful_mux  = new wxCheckBox(this, ID_CB_NEW_AFTER_SUCCESSFUL_MUX, Z("Clear inputs after a successful muxing run"));

  wxString const clear_job_choices[] = {
    Z("only if the run was successfull"),
    Z("even if there were warnings"),
    Z("always"),
  };
  cb_clear_job_after_run           = new wxCheckBox(this, ID_CB_CLEAR_JOB_AFTER_RUN, Z("Remove job from job queue after run:"));
  cob_clear_job_after_run_mode     = new wxMTX_COMBOBOX_TYPE(this, ID_COB_CLEAR_JOB_AFTER_RUN_MODE, wxT("only if the run was successfull"), wxDefaultPosition, wxDefaultSize, 3, clear_job_choices, wxCB_READONLY);

  cb_on_top = new wxCheckBox(this, ID_CB_ON_TOP, Z("Always on top"));

  cb_warn_usage = new wxCheckBox(this, ID_CB_WARN_USAGE, Z("Warn about possible incorrect usage of mmg"));
  cb_warn_usage->SetToolTip(TIP("If checked mmg will warn if it thinks that "
                                "you're using it incorrectly. Such warnings "
                                "are shown at least once even if you turn "
                                "this feature off."));

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
  cob_ui_language = new wxMTX_COMBOBOX_TYPE(this, ID_COB_UI_LANGUAGE,  wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_DROPDOWN | wxCB_READONLY);

  std::vector<translation_c>::iterator translation = translation_c::ms_available_translations.begin();
  wxString select_locale;
  std::vector<locale_sorter_t> sorted_entries;
  while (translation != translation_c::ms_available_translations.end()) {
    wxString curr_entry = wxU(boost::format("%1% (%2%)") % translation->m_translated_name % translation->m_english_name);
    sorted_entries.push_back(locale_sorter_t(curr_entry, translation->get_locale()));

    if (   (select_locale.IsEmpty() && (translation->m_english_name == "English"))
        || balg::iequals(app->m_ui_locale, translation->get_locale()))
      select_locale = curr_entry;

    ++translation;
  }

  wxLogMessage(wxT("Locale selection logic: select_locale %s uu_locale_lower %s translation_c::get_default_ui_locale() %s app->m_ui_locale %s"),
               select_locale.c_str(), wxUCS(app->m_ui_locale), wxUCS(translation_c::get_default_ui_locale()), wxUCS(app->m_ui_locale));

  std::sort(sorted_entries.begin(), sorted_entries.end());

  std::vector<locale_sorter_t>::iterator locale_entry = sorted_entries.begin();
  while (locale_entry != sorted_entries.end()) {
    cob_ui_language->Append(locale_entry->display_val);
    m_sorted_locales.push_back(locale_entry->locale);
    ++locale_entry;
  }
#endif  // HAVE_LIBINTL_H

  auto st_default_subitle_charset = new wxStaticText(this, wxID_ANY, Z("Default subtitle charset"));
  cob_default_subtitle_charset    = new wxMTX_COMBOBOX_TYPE(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_DROPDOWN | wxCB_READONLY);

  auto st_scan_directory_for_playlists = new wxStaticText(this, -1, Z("Scan directory for other playlists:"));

  wxString const scan_directory_for_playlists_choices[] = {
    Z("always ask the user"),
    Z("always scan for other playlists"),
    Z("never scan for other playlists"),
  };
  cob_scan_directory_for_playlists = new wxMTX_COMBOBOX_TYPE(this, ID_COB_SCAN_DIRECTORY_FOR_PLAYLISTS, Z("always ask the user"), wxDefaultPosition, wxDefaultSize, 3, scan_directory_for_playlists_choices, wxCB_READONLY);

  auto st_min_playlist_duration = new wxStaticText(this, -1, Z("Minimum duration for playlists in seconds:"));
  tc_min_playlist_duration = new wxTextCtrl(this, -1, wxU(boost::format("%1%") % m_options.min_playlist_duration));
  tc_min_playlist_duration->SetToolTip(TIP("Only playlists whose duration are at least this long are considered and offered to the user for selection."));
  tc_min_playlist_duration->SetValidator(wxTextValidator(wxFILTER_NUMERIC));

  // Set the defaults.

  cb_on_top->SetValue(m_options.on_top);
  cb_filenew_after_add_to_jobqueue->SetValue(m_options.filenew_after_add_to_jobqueue);
  cb_filenew_after_successful_mux->SetValue(m_options.filenew_after_successful_mux);
  cb_warn_usage->SetValue(m_options.warn_usage);
  cb_gui_debugging->SetValue(m_options.gui_debugging);
  cb_set_delay_from_filename->SetValue(m_options.set_delay_from_filename);

  cb_clear_job_after_run->SetValue(m_options.clear_job_after_run_mode != CJAR_NEVER);
  set_combobox_selection(cob_clear_job_after_run_mode, std::max(static_cast<int>(m_options.clear_job_after_run_mode), 1) - 1);
  cob_clear_job_after_run_mode->Enable(cb_clear_job_after_run->IsChecked());

#if defined(HAVE_LIBINTL_H)
  set_combobox_selection(cob_ui_language, select_locale);
#endif  // HAVE_LIBINTL_H

  cob_default_subtitle_charset->Append(Z("Default"));
  cob_default_subtitle_charset->Append(sorted_charsets);

  auto idx = sorted_charsets.Index(m_options.default_subtitle_charset);
  cob_default_subtitle_charset->SetSelection(idx == wxNOT_FOUND ? 0 : idx + 1);

#if defined(HAVE_CURL_EASY_H)
  cb_check_for_updates->SetValue(m_options.check_for_updates);
#endif  // defined(HAVE_CURL_EASY_H)

  cob_scan_directory_for_playlists->SetSelection(static_cast<int>(m_options.scan_directory_for_playlists));

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

  siz_all->Add(cb_set_delay_from_filename, 0, wxLEFT, 5);
  siz_all->AddSpacer(5);

  siz_all->Add(cb_filenew_after_add_to_jobqueue, 0, wxLEFT, 5);
  siz_all->AddSpacer(5);

  siz_all->Add(cb_filenew_after_successful_mux, 0, wxLEFT, 5);
  siz_all->AddSpacer(5);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(cb_clear_job_after_run,       0, wxALIGN_CENTER_VERTICAL,                             0);
  siz_line->Add(cob_clear_job_after_run_mode, 1, wxALIGN_CENTER_VERTICAL | wxLEFT | wxRIGHT | wxGROW, 5);

  siz_all->Add(siz_line, 0, wxLEFT | wxGROW, 5);
  siz_all->AddSpacer(5);

#if defined(SYS_WINDOWS)
  siz_all->Add(cb_on_top, 0, wxLEFT, 5);
  siz_all->AddSpacer(5);
#else
  cb_on_top->Show(false);
#endif

  siz_all->Add(cb_warn_usage, 0, wxLEFT, 5);
  siz_all->AddSpacer(5);

#if defined(HAVE_CURL_EASY_H)
  siz_all->Add(cb_check_for_updates, 0, wxLEFT, 5);
  siz_all->AddSpacer(5);
#endif  // defined(HAVE_CURL_EASY_H)

  siz_all->Add(cb_gui_debugging, 0, wxLEFT, 5);
  siz_all->AddSpacer(5);

  auto siz_fg = new wxFlexGridSizer{2, 5, 5};
  siz_fg->AddGrowableCol(1);

  siz_fg->Add(st_default_subitle_charset,   0, wxALIGN_CENTER_VERTICAL);
  siz_fg->Add(cob_default_subtitle_charset, 1, wxALIGN_CENTER_VERTICAL | wxGROW);

  siz_fg->Add(st_scan_directory_for_playlists,  0, wxALIGN_CENTER_VERTICAL);
  siz_fg->Add(cob_scan_directory_for_playlists, 1, wxALIGN_CENTER_VERTICAL | wxGROW);

  siz_fg->Add(st_min_playlist_duration, 0, wxALIGN_CENTER_VERTICAL);
  siz_fg->Add(tc_min_playlist_duration, 1, wxALIGN_CENTER_VERTICAL | wxGROW);

  siz_all->Add(siz_fg, 0, wxLEFT | wxRIGHT | wxGROW, 5);
  siz_all->AddSpacer(5);

  SetSizerAndFit(siz_all);
}

void
optdlg_mmg_tab::on_clear_job_after_run_pressed(wxCommandEvent &) {
  cob_clear_job_after_run_mode->Enable(cb_clear_job_after_run->IsChecked());
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
  m_options.on_top                        = cb_on_top->IsChecked();
  m_options.filenew_after_add_to_jobqueue = cb_filenew_after_add_to_jobqueue->IsChecked();
  m_options.filenew_after_successful_mux  = cb_filenew_after_successful_mux->IsChecked();
  m_options.warn_usage                    = cb_warn_usage->IsChecked();
  m_options.gui_debugging                 = cb_gui_debugging->IsChecked();
  m_options.set_delay_from_filename       = cb_set_delay_from_filename->IsChecked();
  m_options.clear_job_after_run_mode      = cb_clear_job_after_run->IsChecked() ? static_cast<clear_job_after_run_mode_e>(cob_clear_job_after_run_mode->GetSelection() + 1) : CJAR_NEVER;
#if defined(HAVE_CURL_EASY_H)
  m_options.check_for_updates             = cb_check_for_updates->IsChecked();
#endif  // defined(HAVE_CURL_EASY_H)
  auto sel                                = cob_default_subtitle_charset->GetSelection();
  m_options.default_subtitle_charset      = 0 == sel ? wxString{wxEmptyString} : sorted_charsets[sel - 1];
  m_options.scan_directory_for_playlists  = static_cast<scan_directory_for_playlists_e>(cob_scan_directory_for_playlists->GetSelection());
  if (!parse_number(to_utf8(tc_min_playlist_duration->GetValue()), m_options.min_playlist_duration))
    m_options.min_playlist_duration = 0;

#if defined(HAVE_LIBINTL_H)
  std::string new_ui_locale = get_selected_ui_language();

  if (!balg::iequals(new_ui_locale, app->m_ui_locale)) {
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
  EVT_CHECKBOX(ID_CB_CLEAR_JOB_AFTER_RUN,     optdlg_mmg_tab::on_clear_job_after_run_pressed)
END_EVENT_TABLE();
