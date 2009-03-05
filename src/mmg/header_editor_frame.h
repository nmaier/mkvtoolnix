/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor declarations

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __HEADER_EDITOR_FRAME_H
#define __HEADER_EDITOR_FRAME_H

#include "os.h"

#include <wx/filename.h>
#include <wx/frame.h>
#include <wx/menu.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/treebook.h>

#include "he_page_base.h"
#include "kax_analyzer.h"

#define ID_M_HE_FILE_OPEN            100000
#define ID_M_HE_FILE_SAVE            100001
#define ID_M_HE_FILE_QUIT            100002
#define ID_M_HE_FILE_CLOSE           100003
#define ID_M_HE_FILE_RELOAD          100004
#define ID_M_HE_FILE_LOADLAST1       100090
#define ID_M_HE_FILE_LOADLAST2       100091
#define ID_M_HE_FILE_LOADLAST3       100092
#define ID_M_HE_FILE_LOADLAST4       100093

#define ID_M_HE_HEADERS_EXPAND_ALL   100100
#define ID_M_HE_HEADERS_COLLAPSE_ALL 100101
#define ID_M_HE_HEADERS_VALIDATE     100102

#define ID_M_HE_HELP_HELP            100900

#define ID_HE_CB_ADD_OR_REMOVE       110000
#define ID_HE_B_RESET                110001

class header_editor_frame_c: public wxFrame {
  DECLARE_CLASS(header_editor_frame_c);
  DECLARE_EVENT_TABLE();
public:
  std::vector<he_page_base_c *> m_pages;

  wxFileName m_file_name;
  wxDateTime m_file_mtime;

  wxMenu *m_file_menu, *m_headers_menu;
  bool m_file_menu_sep;

  wxPanel *m_panel;
  wxBoxSizer *m_bs_panel;
  wxTreebook *m_tb_tree;

  kax_analyzer_c *m_analyzer;

public:
  header_editor_frame_c(wxWindow *parent);
  virtual ~header_editor_frame_c();

  void on_file_open(wxCommandEvent &evt);
  void on_file_save(wxCommandEvent &evt);
  void on_file_reload(wxCommandEvent &evt);
  void on_file_close(wxCommandEvent &evt);
  void on_file_quit(wxCommandEvent &evt);

  void on_close_window(wxCloseEvent &evt);

  void on_headers_expand_all(wxCommandEvent &evt);
  void on_headers_collapse_all(wxCommandEvent &evt);
  void on_headers_validate(wxCommandEvent &evt);

  void on_help_help(wxCommandEvent &evt);

protected:
  bool may_close();

  void update_file_menu();
  void enable_menu_entries();

  bool open_file(wxFileName file_name);

  void clear_pages();

  void handle_segment_info(analyzer_data_c *data);
  void handle_tracks(analyzer_data_c *data);

  bool have_been_modified();
  void do_modifications();
  int validate_pages();
  bool validate();

  void display_update_element_result(kax_analyzer_c::update_element_result_e result);
};

#endif // __HEADER_EDITOR_FRAME_H
