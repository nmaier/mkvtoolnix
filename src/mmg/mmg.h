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

typedef struct {
  char type;
  int64_t id;
  wxString *ctype;
  bool selected;

  bool default_track, aac_is_sbr;
  wxString *language, *track_name, *cues, *delay, *stretch, *sub_charset;
  wxString *tags;
} mmg_track_t;

typedef struct {
  wxString *file_name;
  vector<mmg_track_t> *tracks;
  bool no_chapters;
} mmg_file_t;

class mmg_dialog: public wxFrame {    
  DECLARE_CLASS(mmg_dialog);
  DECLARE_EVENT_TABLE();
protected:
  wxListBox *lb_input_files;
  wxButton *b_add_file, *b_remove_file, *b_browse_output, *b_browse_tags;
  wxCheckBox *cb_no_chapters, *cb_default, *cb_aac_is_sbr;
  wxCheckListBox *clb_tracks;
  wxComboBox *cob_language, *cob_cues, *cob_sub_charset;
  wxTextCtrl *tc_delay, *tc_track_name, *tc_stretch, *tc_output, *tc_tags;

  wxString last_open_dir;
  vector<mmg_file_t> *files;
  int selected_file;

public:
  mmg_dialog();

  void on_add_file(wxCommandEvent &evt);
  void on_remove_file(wxCommandEvent &evt);
  void on_browse_output(wxCommandEvent &evt);
  void on_file_selected(wxCommandEvent &evt);
  void on_track_selected(wxCommandEvent &evt);
  void on_nochapters_clicked(wxCommandEvent &evt);
};

wxString &break_line(wxString &line, int break_after = 80);

#endif // __MMG_DIALOG_H
