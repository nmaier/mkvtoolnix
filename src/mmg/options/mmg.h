/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   declarations for the options dialog -- mmg tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_MMG_OPTIONS_MMG_H
#define MTX_MMG_OPTIONS_MMG_H

#include "common/common_pch.h"

#include <vector>
#include <wx/log.h>

#include "mmg/options/tab_base.h"

#define ID_CB_ON_TOP                    15105
#define ID_CB_NEW_AFTER_ADD_TO_JOBQUEUE 15106
#define ID_CB_WARN_USAGE                15107
#define ID_CB_GUI_DEBUGGING             15108
#define ID_CB_SET_DELAY_FROM_FILENAME   15110
#define ID_COB_UI_LANGUAGE              15116
#define ID_CB_NEW_AFTER_SUCCESSFUL_MUX  15117
#define ID_CB_CHECK_FOR_UPDATES         15119
#define ID_COB_CLEAR_JOB_AFTER_RUN_MODE 15120
#define ID_CB_CLEAR_JOB_AFTER_RUN       15121
#define ID_COB_SCAN_DIRECTORY_FOR_PLAYLISTS 15124

class optdlg_mmg_tab: public optdlg_base_tab {
  DECLARE_CLASS(optdlg_mmg_tab);
  DECLARE_EVENT_TABLE();
public:
  wxCheckBox *cb_on_top;
  wxCheckBox *cb_filenew_after_add_to_jobqueue;
  wxCheckBox *cb_filenew_after_successful_mux;
  wxCheckBox *cb_warn_usage, *cb_gui_debugging;
  wxCheckBox *cb_set_delay_from_filename;
#if defined(HAVE_CURL_EASY_H)
  wxCheckBox *cb_check_for_updates;
#endif  // defined(HAVE_CURL_EASY_H)
  wxCheckBox *cb_clear_job_after_run;
  wxMTX_COMBOBOX_TYPE *cob_clear_job_after_run_mode, *cob_scan_directory_for_playlists, *cob_default_subtitle_charset;
  wxTextCtrl *tc_min_playlist_duration;

#if defined(HAVE_LIBINTL_H)
  wxMTX_COMBOBOX_TYPE *cob_ui_language;
  std::vector<std::string> m_sorted_locales;
#endif  // HAVE_LIBINTL_H

public:
  translation_table_c cob_priority_translations;

public:
  optdlg_mmg_tab(wxWindow *parent, mmg_options_t &options);

  void on_ok(wxCommandEvent &evt);
  void on_clear_job_after_run_pressed(wxCommandEvent &evt);

  std::string get_selected_ui_language();

  virtual void save_options();
  virtual wxString get_title();
};

#endif // MTX_MMG_OPTIONS_MMG_H
