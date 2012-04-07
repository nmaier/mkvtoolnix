/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   main dialog declarations

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MMG_DIALOG_H
#define __MMG_DIALOG_H

#include "common/common_pch.h"

#include <wx/html/helpctrl.h>
#include <wx/log.h>

#if defined(HAVE_CURL_EASY_H)
# include <wx/thread.h>

# include "common/version.h"
# include "common/xml/xml.h"
# include "mmg/update_checker.h"
#endif  // defined(HAVE_CURL_EASY_H)

#include "mmg/mmg.h"

#define ID_TC_OUTPUT                      10000
#define ID_B_BROWSEOUTPUT                 10001
#define ID_NOTEBOOK                       10002
#define ID_T_UPDATECMDLINE                10004
#define ID_T_STATUSBAR                    10005
#define ID_B_STARTMUXING                  10006
#define ID_B_COPYTOCLIPBOARD              10007
#define ID_B_ADD_TO_JOBQUEUE              10008
#define ID_HLC_DOWNLOAD_URL               10009
#define ID_B_UPDATE_CHECK_CLOSE           10010
#define ID_RE_CHANGELOG                   10011
#define ID_B_UPDATE_CHECK_DOWNLOAD        10012

#define ID_M_FILE_NEW                     30000
#define ID_M_FILE_LOAD                    30001
#define ID_M_FILE_SAVE                    30002
#define ID_M_FILE_SETOUTPUT               30003
#define ID_M_FILE_EXIT                    30004
#define ID_M_FILE_OPTIONS                 30005
#define ID_M_FILE_HEADEREDITOR            30006
#define ID_M_FILE_LOADSEPARATOR           30090
#define ID_M_FILE_LOADLAST1               30091
#define ID_M_FILE_LOADLAST2               30092
#define ID_M_FILE_LOADLAST3               30093
#define ID_M_FILE_LOADLAST4               30094

#define ID_M_MUXING_START                 30100
#define ID_M_MUXING_COPY_CMDLINE          30101
#define ID_M_MUXING_SAVE_CMDLINE          30102
#define ID_M_MUXING_CREATE_OPTIONFILE     30103
#define ID_M_MUXING_ADD_TO_JOBQUEUE       30104
#define ID_M_MUXING_MANAGE_JOBS           30105
#define ID_M_MUXING_ADD_CLI_OPTIONS       30106
#define ID_M_MUXING_SHOW_CMDLINE          30107

#define ID_M_CHAPTERS_NEW                 30200
#define ID_M_CHAPTERS_LOAD                30201
#define ID_M_CHAPTERS_SAVE                30202
#define ID_M_CHAPTERS_SAVEAS              30203
#define ID_M_CHAPTERS_SAVETOKAX           30204
#define ID_M_CHAPTERS_VERIFY              30205
#define ID_M_CHAPTERS_LOADSEPARATOR       30290
#define ID_M_CHAPTERS_LOADLAST1           30291
#define ID_M_CHAPTERS_LOADLAST2           30292
#define ID_M_CHAPTERS_LOADLAST3           30293
#define ID_M_CHAPTERS_LOADLAST4           30294

#define ID_M_WINDOW_INPUT                 30300
#define ID_M_WINDOW_ATTACHMENTS           30301
#define ID_M_WINDOW_GLOBAL                30302
#define ID_M_WINDOW_CHAPTEREDITOR         30303

#define ID_M_HELP_ABOUT                   31000
#define ID_M_HELP_HELP                    31001
#define ID_M_HELP_CHECK_FOR_UPDATES       31002

#define HELP_ID_CONTENTS                      1
#define HELP_ID_INTRODUCTION              10000
#define HELP_ID_SETUP                     20000
#define HELP_ID_MUXING                    30000
#define HELP_ID_CHAPTER_EDITOR            40000
#define HELP_ID_HEADER_EDITOR             50000

/* class tab_advanced; */
class tab_attachments;
class tab_chapters;
class tab_global;
class tab_input;
class job_dialog;
class header_editor_frame_c;
class mmg_dialog;

class mmg_dialog: public wxFrame {
  DECLARE_CLASS(mmg_dialog);
  DECLARE_EVENT_TABLE();
public:
  wxButton *b_browse_output;
  wxTextCtrl *tc_output;
  wxString previous_output_directory;

  wxString cmdline, cli_options;
  wxArrayString clargs;

  wxTimer status_bar_timer;

  wxStatusBar *status_bar;
  wxStaticBox *sb_output_filename;

  wxNotebook *notebook;
  wxMenu *file_menu, *chapter_menu;
  bool file_menu_sep, chapter_menu_sep;

  tab_input *input_page;
  tab_attachments *attachments_page;
  tab_global *global_page;
  tab_chapters *chapter_editor_page;

  wxButton *b_start_muxing, *b_copy_to_clipboard, *b_add_to_jobqueue;

  int last_job_id;

  bool muxing_in_progress;

  wxHtmlHelpController *help;
  wxLogWindow *log_window;

  bool warned_chapter_editor_not_empty;

  mmg_options_t options;

  std::vector<header_editor_frame_c *> header_editor_frames;

#if defined(SYS_WINDOWS)
  bool m_taskbar_msg_received;
#endif

#if defined(HAVE_CURL_EASY_H)
  bool m_checking_for_updates, m_interactive_update_check;
  wxMutex m_update_check_mutex;
  mtx_release_version_t m_release_version;
  mtx::xml::document_cptr m_releases_info;
  update_check_dlg_c *m_update_check_dlg;
#endif  // defined(HAVE_CURL_EASY_H)

#if defined(SYS_WINDOWS)
public:
  static size_t ms_dpi_x, ms_dpi_y;
#endif

public:
  mmg_dialog();
  virtual ~mmg_dialog();

  void on_browse_output(wxCommandEvent &evt);
  void on_run(wxCommandEvent &evt);
  void on_save_cmdline(wxCommandEvent &evt);
  void on_show_cmdline(wxCommandEvent &evt);
  void on_copy_to_clipboard(wxCommandEvent &evt);
  void on_create_optionfile(wxCommandEvent &evt);

  void on_quit(wxCommandEvent &evt);
  void on_file_new(wxCommandEvent &evt);
  void on_file_load(wxCommandEvent &evt);
  void on_file_save(wxCommandEvent &evt);
  void on_file_options(wxCommandEvent &evt);
  void on_help(wxCommandEvent &evt);
  void on_about(wxCommandEvent &evt);

  void on_update_command_line(wxTimerEvent &evt);
  void update_command_line();
  wxString &get_command_line();
  wxArrayString &get_command_line_args();

  void load(wxString file_name, bool used_for_jobs = false);
  void save(wxString file_name, bool used_for_jobs = false);

  void on_clear_status_bar(wxTimerEvent &evt);
  void set_status_bar(wxString text);

  void set_last_settings_in_menu(wxString name);
  void on_file_load_last(wxCommandEvent &evt);
  void update_file_menu();

  void set_last_chapters_in_menu(wxString name);
  void on_chapters_load_last(wxCommandEvent &evt);
  void update_chapter_menu();

  void on_new_chapters(wxCommandEvent &evt);
  void on_load_chapters(wxCommandEvent &evt);
  void on_save_chapters(wxCommandEvent &evt);
  void on_save_chapters_as(wxCommandEvent &evt);
  void on_save_chapters_to_kax_file(wxCommandEvent &evt);
  void on_verify_chapters(wxCommandEvent &evt);
  void on_set_default_chapter_values(wxCommandEvent &evt);

  void on_window_selected(wxCommandEvent &evt);

  void set_title_maybe(const wxString &new_title);
  void set_output_maybe(const wxString &new_output);
  void remove_output_filename();
  void handle_webm_mode(bool enabled);

  void on_add_to_jobqueue(wxCommandEvent &evt);
  void on_manage_jobs(wxCommandEvent &evt);

  void on_add_cli_options(wxCommandEvent &evt);

  void on_close(wxCloseEvent &evt);

  void on_run_header_editor(wxCommandEvent &evt);
  void header_editor_frame_closed(header_editor_frame_c *frame);
  void create_header_editor_window(const wxString &file_name = wxEmptyString);

  void translate_ui();

  void load_job_queue();
  void save_job_queue();

  void set_on_top(bool on_top);
  void restore_on_top();

  void muxing_has_finished(int exit_code);

  bool check_before_overwriting();
  bool is_output_file_name_valid();

  void load_preferences();
  void save_preferences();

  void remember_previous_output_directory();

  void query_mkvmerge_capabilities();

  void display_help(int id);

  wxString suggest_file_name_extension();
  void update_output_file_name_extension();

#if defined(HAVE_CURL_EASY_H)
  void maybe_check_for_updates();
  void check_for_updates(bool interactive);
  void set_release_version(mtx_release_version_t const &release_version);
  void set_releases_info(mtx::xml::document_cptr const &releases_info);

  void on_check_for_updates(wxCommandEvent &evt);
  void on_update_check_state_changed(wxCommandEvent &evt);
  wxString version_key_for_config();
#endif  // defined(HAVE_CURL_EASY_H)

public:
  static size_t scale_with_dpi(size_t width_or_height, bool is_width = true);
  static wxSize scale_with_dpi(const wxSize &size);
  static bool is_higher_dpi();

protected:
#if defined(SYS_WINDOWS)
  virtual WXLRESULT MSWWindowProc(WXUINT msg, WXWPARAM wParam, WXLPARAM lParam);
  void RegisterWindowMessages();
  void init_dpi_settings();
#endif

  void set_menu_item_strings(int id, const wxString &title, const wxString &help_text = wxEmptyString);
  void create_menus();
};

extern mmg_dialog *mdlg;

#endif // __MMG_DIALOG_H
