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

using namespace std;

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
#define ID_B_RUN 10020
#define ID_B_SAVECMDLINE 10021
#define ID_B_COPYTOCLIPBOARD 10022
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

#define ID_M_FILE_LOAD 20000
#define ID_M_FILE_SAVE 20001
#define ID_M_FILE_EXIT 20002

#define ID_M_MUXING_START 20100
#define ID_M_MUXING_COPY_CMDLINE 20101
#define ID_M_MUXING_SAVE_CMDLINE 20102

#define ID_M_HELP_ABOUT 20200

typedef struct {
  char type;
  int64_t id;
  wxString *ctype;
  bool enabled;

  bool default_track, aac_is_sbr;
  wxString *language, *track_name, *cues, *delay, *stretch, *sub_charset;
  wxString *tags;
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
extern vector<mmg_file_t> files;
extern vector<mmg_attachment_t> attachments;

wxString &break_line(wxString &line, int break_after = 80);
wxString extract_language_code(wxString &source);
wxString shell_escape(wxString &source);

class mmg_dialog: public wxFrame {    
  DECLARE_CLASS(mmg_dialog);
  DECLARE_EVENT_TABLE();
protected:
  wxButton *b_browse_output, *b_run, *b_save_cmdline, *b_copy_to_clipboard;
  wxTextCtrl *tc_output, *tc_cmdline;

  wxString cmdline;

  wxTimer cmdline_timer;
  wxTimer status_bar_timer;

  wxStatusBar *status_bar;

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

  void on_clear_status_bar(wxTimerEvent &evt);
  void set_status_bar(wxString text);
};

extern mmg_dialog *mdlg;

class tab_input: public wxPanel {
  DECLARE_CLASS(tab_input);
  DECLARE_EVENT_TABLE();
protected:
  wxListBox *lb_input_files;
  wxButton *b_add_file, *b_remove_file, *b_browse_tags;
  wxCheckBox *cb_no_chapters, *cb_default, *cb_aac_is_sbr;
  wxCheckListBox *clb_tracks;
  wxComboBox *cob_language, *cob_cues, *cob_sub_charset;
  wxTextCtrl *tc_delay, *tc_track_name, *tc_stretch, *tc_tags;

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

  void no_track_mode();
  void audio_track_mode();
  void video_track_mode();
  void subtitle_track_mode();
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

public:
  tab_attachments(wxWindow *parent);

  void on_add_attachment(wxCommandEvent &evt);
  void on_remove_attachment(wxCommandEvent &evt);
  void on_attachment_selected(wxCommandEvent &evt);
  void on_description_changed(wxCommandEvent &evt);
  void on_mimetype_changed(wxCommandEvent &evt);
  void on_style_changed(wxCommandEvent &evt);

  void enable(bool e);
};

class tab_settings: public wxPanel {
  DECLARE_CLASS(tab_settings);
  DECLARE_EVENT_TABLE();
protected:
  wxTextCtrl *tc_mkvmerge;

public:
  tab_settings(wxWindow *parent);

  void on_browse(wxCommandEvent &evt);

  void load();
  void save();
};

#endif // __MMG_DIALOG_H
