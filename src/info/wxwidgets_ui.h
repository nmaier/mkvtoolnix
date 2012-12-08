/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definition of global variables and functions

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_WXWIDGETS_UI_H
#define MTX_WXWIDGETS_UI_H

#include "common/common_pch.h"

#ifdef HAVE_WXWIDGETS

#include <wx/wx.h>
#include <wx/dnd.h>
#include <wx/treectrl.h>

#include "common/wx.h"

class mi_app: public wxApp {
public:
  virtual bool OnInit();

  std::string get_ui_locale();
};

class mi_dndfile: public wxFileDropTarget {
public:
  virtual bool OnDropFiles(wxCoord x, wxCoord y,
                           const wxArrayString &filenames);
};

class mi_frame: public wxFrame {
private:
  //For Drag-n-Drop
  mi_dndfile *dnd_load;
  wxMenu *menu_options, *menu_file;
  bool show_all_elements, expand_important_elements;
  bool file_open;
  int last_percent;
  int64_t num_elements, elements_saved;
  wxString current_file, last_dir;

  wxTreeCtrl *tree;
  wxTreeItemId item_ids[10];

public:
  mi_frame(const wxString &title, const wxPoint &pos, const wxSize &size,
           long style = wxDEFAULT_FRAME_STYLE);

  void open_file(wxString file_name);
  void show_progress(int percent, wxString msg);
  void show_error(wxString msg);
  void add_item(int level, wxString text);

protected:
  void on_file_open(wxCommandEvent &event);
  void on_file_savetext(wxCommandEvent &event);
  void on_file_quit(wxCommandEvent &event);
  void on_options_showall(wxCommandEvent &event);
  void on_options_show_sizes(wxCommandEvent &event);
  void on_options_expandimportant(wxCommandEvent &event);
  void on_help_about(wxCommandEvent &event);
  void on_right_click(wxTreeEvent &event);

  void expand_elements();
  void expand_all_elements(wxTreeItemId &root, bool expand = true);

  void save_elements(wxTreeItemId &root, int level, FILE *f);

private:
  DECLARE_EVENT_TABLE()
};

#endif // HAVE_WXWIDGETS

#endif  // MTX_WXWIDGETS_UI_H
