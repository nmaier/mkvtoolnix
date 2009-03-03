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

#include <vector>

#include <wx/filename.h>
#include <wx/frame.h>
#include <wx/menu.h>
#include <wx/panel.h>
#include <wx/sizer.h>
#include <wx/treebook.h>

#include "kax_analyzer.h"

#define ID_M_HE_FILE_OPEN      30000
#define ID_M_HE_FILE_SAVE      30001
#define ID_M_HE_FILE_QUIT      30002
#define ID_M_HE_FILE_LOADLAST1 30090
#define ID_M_HE_FILE_LOADLAST2 30091
#define ID_M_HE_FILE_LOADLAST3 30092
#define ID_M_HE_FILE_LOADLAST4 30093

#define ID_M_HE_HELP_HELP      30100

class he_page_base_c: public wxPanel {
public:
  std::vector<he_page_base_c *> m_children;
  wxTreebook *m_tree;
  int m_page_id;
  EbmlElement *m_storage;

public:
  he_page_base_c(wxTreebook *parent);
  virtual ~he_page_base_c();
};

class header_editor_frame_c: public wxFrame {
  DECLARE_CLASS(header_editor_frame_c);
  DECLARE_EVENT_TABLE();
public:
  std::vector<he_page_base_c *> m_pages;

  wxFileName m_file_name;

  wxMenu *m_file_menu;
  bool m_file_menu_sep;

  wxPanel *m_panel;
  wxBoxSizer *m_bs_panel;
  wxTreebook *m_tb_tree;

  kax_analyzer_c *m_analyzer;

public:
  header_editor_frame_c(wxWindow *parent);
  virtual ~header_editor_frame_c();

  bool may_close();
  void on_file_open(wxCommandEvent &evt);
  void on_file_save(wxCommandEvent &evt);
  void on_file_quit(wxCommandEvent &evt);
  void on_close(wxCloseEvent &evt);

protected:
  void update_file_menu();
  void enable_file_save_menu_entry();

  bool open_file(const wxFileName &file_name);

  void add_empty_page(void *some_ptr, const wxString &title, const wxString &text);

  void handle_segment_info(analyzer_data_c *data);
  void handle_tracks(analyzer_data_c *data);
};

#endif // __HEADER_EDITOR_FRAME_H
