/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  mmg.h

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <root@sirelvis.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief main stuff - declarations
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __MMG_DIALOG_H
#define __MMG_DIALOG_H

#include <vector>
#include <stdint.h>

#include "matroska/KaxChapters.h"

#include "os.h"
#include "wx/confbase.h"
#include "wx/process.h"
#include "wx/treectrl.h"

using namespace std;
using namespace libmatroska;

#ifdef SYS_WINDOWS
#define ALLFILES "All Files (*.*)|*.*"
#define PSEP '\\'
#define YOFF (-4)
#else
#define ALLFILES "All Files (*)|*"
#define PSEP '/'
#define YOFF (-2)
#endif

#define ID_DIALOG 10000
#define ID_TC_OUTPUT 10001
#define ID_B_BROWSEOUTPUT 10002
#define ID_NOTEBOOK 10003
#define ID_PANEL 10004
#define ID_LB_INPUTFILES 10005
#define ID_B_ADDFILE 10006
#define ID_B_REMOVEFILE 10007
#define ID_CLB_TRACKS 10008
#define ID_CB_LANGUAGE 10009
#define ID_TC_TRACKNAME 10010
#define ID_CB_CUES 10011
#define ID_CB_MAKEDEFAULT 10012
#define ID_TC_DELAY 10013
#define ID_TC_STRETCH 10014
#define ID_CB_NOCHAPTERS 10015
#define ID_CB_SUBTITLECHARSET 10016
#define ID_CB_AACISSBR 10017
#define ID_TC_TAGS 10018
#define ID_B_BROWSETAGS 10019
#define ID_TC_CMDLINE 10023
#define ID_T_UPDATECMDLINE 10024
#define ID_B_ADDATTACHMENT 10025
#define ID_B_REMOVEATTACHMENT 10026
#define ID_CB_MIMETYPE 10027
#define ID_TC_DESCRIPTION 10028
#define ID_CB_ATTACHMENTSTYLE 10029
#define ID_LB_ATTACHMENTS 10030
#define ID_TC_MKVMERGE 10031
#define ID_B_BROWSEMKVMERGE 10032
#define ID_T_STATUSBAR 10033
#define ID_B_BROWSEGLOBALTAGS 10034
#define ID_B_BROWSECHAPTERS 10035
#define ID_CB_SPLIT 10036
#define ID_RB_SPLITBYSIZE 10037
#define ID_RB_SPLITBYTIME 10038
#define ID_CB_SPLITBYSIZE 10039
#define ID_CB_SPLITBYTIME 10040
#define ID_CB_DONTLINK 10041
#define ID_TC_SPLITMAXFILES 10042
#define ID_CB_ASPECTRATIO 10043
#define ID_CB_FOURCC 10044
#define ID_TC_PREVIOUSSEGMENTUID 10045
#define ID_TC_NEXTSEGMENTUID 10046
#define ID_TC_CHAPTERS 10047
#define ID_CB_CHAPTERLANGUAGE 10048
#define ID_CB_CHAPTERCHARSET 10049
#define ID_TC_GLOBALTAGS 10050
#define ID_TC_SEGMENTTITLE 10051
#define ID_CB_NOCUES 10052
#define ID_CB_NOCLUSTERSINMETASEEK 10053
#define ID_CB_ENABLELACING 10054
#define ID_B_MUX_OK 10055
#define ID_B_MUX_SAVELOG 10056
#define ID_B_MUX_ABORT 10057
#define ID_T_ATTACHMENTVALUES 10058
#define ID_T_INPUTVALUES 10059
#define ID_TRC_CHAPTERS 10060
#define ID_B_ADDCHAPTER 10061
#define ID_B_REMOVECHAPTER 10062
#define ID_T_CHAPTERVALUES 10063
#define ID_TC_CHAPTERNAME 10064
#define ID_TC_CHAPTERLANGUAGES 10065
#define ID_TC_CHAPTERCOUNTRYCODES 10066
#define ID_TC_CHAPTERSTART 10067
#define ID_TC_CHAPTEREND 10068
#define ID_CB_CHAPTERSELECTLANGUAGECODE 10069
#define ID_CB_CHAPTERSELECTCOUNTRYCODE 10070

#define ID_M_FILE_LOAD 20000
#define ID_M_FILE_SAVE 20001
#define ID_M_FILE_EXIT 20002
#define ID_M_FILE_LOADSEPARATOR 20090
#define ID_M_FILE_LOADLAST1 20091
#define ID_M_FILE_LOADLAST2 20092
#define ID_M_FILE_LOADLAST3 20093
#define ID_M_FILE_LOADLAST4 20094

#define ID_M_MUXING_START 20100
#define ID_M_MUXING_COPY_CMDLINE 20101
#define ID_M_MUXING_SAVE_CMDLINE 20102

#define ID_M_CHAPTERS_NEW 20200
#define ID_M_CHAPTERS_LOAD 20201
#define ID_M_CHAPTERS_SAVE 20202
#define ID_M_CHAPTERS_SAVEAS 20203
#define ID_M_CHAPTERS_VERIFY 20204

#define ID_M_HELP_ABOUT 29900

typedef struct {
  char type;
  int64_t id;
  wxString *ctype;
  bool enabled;

  bool default_track, aac_is_sbr;
  wxString *language, *track_name, *cues, *delay, *stretch, *sub_charset;
  wxString *tags, *fourcc, *aspect_ratio;
} mmg_track_t;

typedef struct {
  wxString *file_name;
  vector<mmg_track_t> *tracks;
  bool no_chapters;
} mmg_file_t;

typedef struct {
  wxString *file_name, *description, *mime_type;
  int style;
} mmg_attachment_t;

extern wxString last_open_dir;
extern wxString mkvmerge_path;
extern vector<wxString> last_settings;
extern vector<mmg_file_t> files;
extern vector<mmg_attachment_t> attachments;
extern wxArrayString sorted_charsets;
extern wxArrayString sorted_iso_codes;

wxString &break_line(wxString &line, int break_after = 80);
wxString extract_language_code(wxString source);
wxString shell_escape(wxString source);

class tab_input: public wxPanel {
  DECLARE_CLASS(tab_input);
  DECLARE_EVENT_TABLE();
protected:
  wxListBox *lb_input_files;
  wxButton *b_add_file, *b_remove_file, *b_browse_tags;
  wxCheckBox *cb_no_chapters, *cb_default, *cb_aac_is_sbr;
  wxCheckListBox *clb_tracks;
  wxComboBox *cob_language, *cob_cues, *cob_sub_charset;
  wxComboBox *cob_aspect_ratio, *cob_fourcc;
  wxTextCtrl *tc_delay, *tc_track_name, *tc_stretch, *tc_tags;

  wxTimer value_copy_timer;

  int selected_file, selected_track;

public:
  tab_input(wxWindow *parent);

  void on_add_file(wxCommandEvent &evt);
  void on_remove_file(wxCommandEvent &evt);
  void on_file_selected(wxCommandEvent &evt);
  void on_track_selected(wxCommandEvent &evt);
  void on_track_enabled(wxCommandEvent &evt);
  void on_nochapters_clicked(wxCommandEvent &evt);
  void on_default_track_clicked(wxCommandEvent &evt);
  void on_aac_is_sbr_clicked(wxCommandEvent &evt);
  void on_language_selected(wxCommandEvent &evt);
  void on_cues_selected(wxCommandEvent &evt);
  void on_subcharset_selected(wxCommandEvent &evt);
  void on_browse_tags(wxCommandEvent &evt);
  void on_delay_changed(wxCommandEvent &evt);
  void on_stretch_changed(wxCommandEvent &evt);
  void on_track_name_changed(wxCommandEvent &evt);
  void on_aspect_ratio_changed(wxCommandEvent &evt);
  void on_fourcc_changed(wxCommandEvent &evt);
  void on_value_copy_timer(wxTimerEvent &evt);

  void no_track_mode();
  void audio_track_mode();
  void video_track_mode();
  void subtitle_track_mode();

  void save(wxConfigBase *cfg);
  void load(wxConfigBase *cfg);
  bool validate_settings();
};

class tab_attachments: public wxPanel {
  DECLARE_CLASS(tab_attachments);
  DECLARE_EVENT_TABLE();
protected:
  wxListBox *lb_attachments;
  wxButton *b_add_attachment, *b_remove_attachment;
  wxComboBox *cob_mimetype, *cob_style;
  wxTextCtrl *tc_description;

  int selected_attachment;

  wxTimer t_get_entries;

public:
  tab_attachments(wxWindow *parent);

  void on_add_attachment(wxCommandEvent &evt);
  void on_remove_attachment(wxCommandEvent &evt);
  void on_attachment_selected(wxCommandEvent &evt);
  void on_description_changed(wxCommandEvent &evt);
  void on_mimetype_changed(wxTimerEvent &evt);
  void on_style_changed(wxCommandEvent &evt);

  void enable(bool e);

  void save(wxConfigBase *cfg);
  void load(wxConfigBase *cfg);
  bool validate_settings();
};

class tab_settings: public wxPanel {
  DECLARE_CLASS(tab_settings);
  DECLARE_EVENT_TABLE();
protected:
  wxTextCtrl *tc_mkvmerge;

public:
  tab_settings(wxWindow *parent);
  virtual ~tab_settings();

  void on_browse(wxCommandEvent &evt);

  void load_preferences();
  void save_preferences();

  void save(wxConfigBase *cfg);
  void load(wxConfigBase *cfg);
  bool validate_settings();
};

class tab_global: public wxPanel {
  DECLARE_CLASS(tab_global);
  DECLARE_EVENT_TABLE();
public:
  wxTextCtrl *tc_chapters, *tc_global_tags, *tc_split_max_files, *tc_title;
  wxTextCtrl *tc_next_segment_uid, *tc_previous_segment_uid;
  wxTextCtrl *tc_split_bytes, *tc_split_time;
  wxCheckBox *cb_split, *cb_dontlink;
  wxRadioButton *rb_split_by_size, *rb_split_by_time;
  wxComboBox *cob_split_by_size, *cob_split_by_time;
  wxComboBox *cob_chap_language, *cob_chap_charset;
  wxCheckBox *cb_no_cues, *cb_no_clusters, *cb_enable_lacing;

public:
  tab_global(wxWindow *parent);

  void on_browse_chapters(wxCommandEvent &evt);
  void on_browse_global_tags(wxCommandEvent &evt);
  void on_split_clicked(wxCommandEvent &evt);
  void on_splitby_clicked(wxCommandEvent &evt);

  void save(wxConfigBase *cfg);
  void load(wxConfigBase *cfg);
  bool validate_settings();
};

class tab_chapters: public wxPanel {
  DECLARE_CLASS(tab_chapters);
  DECLARE_EVENT_TABLE();
public:
  wxTreeCtrl *tc_chapters;
  wxTreeItemId tid_root;
  wxButton *b_add_chapter, *b_remove_chapter;
  wxMenu *m_chapters;

  wxTextCtrl *tc_chapter_name, *tc_language_codes, *tc_country_codes;
  wxTextCtrl *tc_start_time, *tc_end_time;
  wxComboBox *cob_add_language_code, *cob_add_country_code;

  wxTimer value_copy_timer;

  wxString file_name;

  KaxChapters *chapters;

public:
  tab_chapters(wxWindow *parent, wxMenu *nm_chapters);
  ~tab_chapters();

  void on_new_chapters(wxCommandEvent &evt);
  void on_load_chapters(wxCommandEvent &evt);
  void on_save_chapters(wxCommandEvent &evt);
  void on_save_chapters_as(wxCommandEvent &evt);
  void on_verify_chapters(wxCommandEvent &evt);
  void on_add_chapter(wxCommandEvent &evt);
  void on_remove_chapter(wxCommandEvent &evt);
  void on_copy_values(wxTimerEvent &evt);
  void on_entry_selected(wxTreeEvent &evt);
  void on_language_code_selected(wxCommandEvent &evt);
  void on_country_code_selected(wxCommandEvent &evt);

  void add_recursively(wxTreeItemId &parent, EbmlMaster &master);
  wxString create_chapter_label(KaxChapterAtom &chapter);

  void enable_inputs(bool enable);
};

class mux_dialog: public wxDialog {
  DECLARE_CLASS(mux_dialog);
  DECLARE_EVENT_TABLE();
protected:
  long pid;
  wxStaticText *st_label;
  wxGauge *g_progress;
  wxProcess *process;
  wxString log;
  wxButton *b_ok, *b_save_log, *b_abort;
  wxTextCtrl *tc_output, *tc_warnings, *tc_errors;
public:

  mux_dialog(wxWindow *parent);
  ~mux_dialog();

  void update_window(wxString text);
  void update_gauge(long value);

  void on_ok(wxCommandEvent &evt);
  void on_save_log(wxCommandEvent &evt);
  void on_abort(wxCommandEvent &evt);
};

class mux_process: public wxProcess {
public:
  mux_dialog *dlg;

  mux_process(mux_dialog *mdlg);
  virtual void OnTerminate(int pid, int status);
};

class mmg_dialog: public wxFrame {    
  DECLARE_CLASS(mmg_dialog);
  DECLARE_EVENT_TABLE();
protected:
  wxButton *b_browse_output;
  wxTextCtrl *tc_output, *tc_cmdline;

  wxString cmdline;
  wxArrayString clargs;

  wxTimer cmdline_timer;
  wxTimer status_bar_timer;

  wxStatusBar *status_bar;

  wxMenu *file_menu;
  bool file_menu_sep;

  tab_input *input_page;
  tab_attachments *attachments_page;
  tab_global *global_page;
  tab_settings *settings_page;
  tab_chapters *chapter_editor_page;

public:
  mmg_dialog();

  void on_browse_output(wxCommandEvent &evt);
  void on_run(wxCommandEvent &evt);
  void on_save_cmdline(wxCommandEvent &evt);
  void on_copy_to_clipboard(wxCommandEvent &evt);

  void on_quit(wxCommandEvent &evt);
  void on_file_load(wxCommandEvent &evt);
  void on_file_save(wxCommandEvent &evt);
  void on_about(wxCommandEvent &evt);

  void on_update_command_line(wxTimerEvent &evt);
  void update_command_line();
  wxString &get_command_line();
  wxArrayString &get_command_line_args();

  void load(wxString file_name);
  void save(wxString file_name);

  void on_clear_status_bar(wxTimerEvent &evt);
  void set_status_bar(wxString text);

  void set_last_settings_in_menu(wxString name);
  void on_file_load_last(wxCommandEvent &evt);
  void update_file_menu();

  void on_new_chapters(wxCommandEvent &evt);
  void on_load_chapters(wxCommandEvent &evt);
  void on_save_chapters(wxCommandEvent &evt);
  void on_save_chapters_as(wxCommandEvent &evt);
  void on_verify_chapters(wxCommandEvent &evt);
};

class mmg_app: public wxApp {
public:
  virtual bool OnInit();
  virtual int OnExit();
};

extern mmg_dialog *mdlg;
extern mmg_app *app;

#endif // __MMG_DIALOG_H
