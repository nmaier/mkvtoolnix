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
#include <wx/treectrl.h>

#include "he_page_base.h"
#include "kax_analyzer.h"

#define ID_M_HE_FILE_OPEN            20000
#define ID_M_HE_FILE_SAVE            20001
#define ID_M_HE_FILE_QUIT            20002
#define ID_M_HE_FILE_CLOSE           20003
#define ID_M_HE_FILE_RELOAD          20004
#define ID_M_HE_FILE_LOADLAST1       20090
#define ID_M_HE_FILE_LOADLAST2       20091
#define ID_M_HE_FILE_LOADLAST3       20092
#define ID_M_HE_FILE_LOADLAST4       20093

#define ID_M_HE_HEADERS_EXPAND_ALL   20100
#define ID_M_HE_HEADERS_COLLAPSE_ALL 20101
#define ID_M_HE_HEADERS_VALIDATE     20102

#define ID_M_HE_HELP_HELP            20900

#define ID_HE_CB_ADD_OR_REMOVE       21000
#define ID_HE_B_RESET                21001
#define ID_HE_TC_TREE                21002

class header_editor_frame_c: public wxFrame {
  DECLARE_CLASS(header_editor_frame_c);
  DECLARE_EVENT_TABLE();
public:
  std::vector<he_page_base_c *> m_pages, m_top_level_pages;

  wxFileName m_file_name;
  wxDateTime m_file_mtime;

  wxMenu *m_file_menu, *m_headers_menu;
  bool m_file_menu_sep;

  wxPanel *m_page_panel;
  wxBoxSizer *m_bs_main, *m_bs_page;

  wxTreeCtrl *m_tc_tree;
  wxTreeItemId m_root_id;

  kax_analyzer_c *m_analyzer;

  EbmlElement *m_e_segment_info, *m_e_tracks;

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

  void on_tree_sel_changed(wxTreeEvent &evt);

  bool open_file(wxFileName file_name);

  void append_page(he_page_base_c *page, const wxString &title);
  void append_sub_page(he_page_base_c *page, const wxString &title, wxTreeItemId parent_id);

  wxPanel *get_page_panel() {
    return m_page_panel;
  }

protected:
  bool may_close();

  void update_file_menu();
  void enable_menu_entries();

  void clear_pages();

  void handle_segment_info(analyzer_data_c *data);
  void handle_tracks(analyzer_data_c *data);

  bool have_been_modified();
  void do_modifications();
  wxTreeItemId validate_pages();
  bool validate();

  void display_update_element_result(kax_analyzer_c::update_element_result_e result);

  he_page_base_c *find_page_for_item(wxTreeItemId id);
};

#endif // __HEADER_EDITOR_FRAME_H
