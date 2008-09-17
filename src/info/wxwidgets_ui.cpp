/*
   mkvinfo -- utility for gathering information about Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   A wxWidgets GUI for mkvinfo

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "config.h"

#ifdef HAVE_WXWIDGETS

#include <wx/wx.h>
#include <wx/treectrl.h>
#include <wx/dnd.h>

#include <ebml/EbmlVersion.h>
#include <matroska/KaxVersion.h>

#include "common.h"
#include "mkvinfo.h"
#include "wxwidgets_ui.h"
#if !defined(SYS_WINDOWS)
#include "matroska.xpm"
#endif
#include "wxcommon.h"

using namespace libebml;
using namespace libmatroska;

#if ! wxCHECK_VERSION(2,4,2)
# define wxTreeItemIdValue long
#endif

static mi_frame *frame;

enum {
  mi_file_quit = 1,
  mi_file_open = 2,
  mi_file_savetext = 3,
  mi_options_showall = 101,
  mi_options_expandimportant = 102,
  mi_help_about = wxID_ABOUT
};

bool
mi_app::OnInit() {
  string initial_file;
  vector<string> args;

  setup();

#if WXUNICODE
  int i;

  for (i = 1; i < argc; i++)
    args.push_back(string(wxMB(wxString(argv[i]))));
#else
  args = command_line_utf8(argc, argv);
#endif

  parse_args(args, initial_file);

  if (!use_gui) {
    console_main(args);
    return false;
  }

  frame = new mi_frame(wxT("mkvinfo"), wxPoint(50, 50), wxSize(600, 400));
  frame->Show(true);
  frame->Refresh(true);
  frame->Update();
  while (Pending())
    Dispatch();

  if (initial_file != "")
    frame->open_file(wxU(initial_file.c_str()));

  return true;
}

mi_frame::mi_frame(const wxString &title,
                   const wxPoint &pos,
                   const wxSize &size,
                   long style) :
  wxFrame(NULL, -1, title, pos, size, style) {
  wxMenu *menu_help;
  wxMenuBar *menu_bar;

  file_open = false;
  tree = NULL;

  if (verbose == 0)
    show_all_elements = false;
  else
    show_all_elements = true;
  expand_important_elements = true;

  SetIcon(wxICON(matroska));

  menu_file = new wxMenu();
  menu_options = new wxMenu();
  menu_help = new wxMenu();
  menu_bar = new wxMenuBar();

  menu_file->Append(mi_file_open, wxT("&Open\tCtrl-O"),
                    wxT("Open a Matroska file"));
  menu_file->Append(mi_file_savetext, wxT("&Save info as text\tCtrl-S"),
                    wxT("Saves the information from the current file to a "
                       "text file"));
  menu_file->Enable(mi_file_savetext, false);
  menu_file->AppendSeparator();
  menu_file->Append(mi_file_quit, wxT("E&xit\tCtrl-Q"),
                    wxT("Quits mkvinfo"));
  menu_options->AppendCheckItem(mi_options_showall,
                                wxT("Show &all elements\tCtrl-A"),
                                wxT("Parse the file completely and show all "
                                   "elements"));
  menu_options->Check(mi_options_showall, show_all_elements);
  menu_options->AppendCheckItem(mi_options_expandimportant,
                                wxT("&Expand important elements\tCtrl-E"),
                                wxT("After loading a file expand the most "
                                   "important elements"));
  menu_options->Check(mi_options_expandimportant, expand_important_elements);
  menu_help->Append(mi_help_about, wxT("&About...\tF1"),
                    wxT("Show about dialog"));

  menu_bar->Append(menu_file, wxT("&File"));
  menu_bar->Append(menu_options, wxT("&Options"));
  menu_bar->Append(menu_help, wxT("&Help"));

  SetMenuBar(menu_bar);

  tree = new wxTreeCtrl(this, 4254);
  dnd_load = new mi_dndfile();
  tree->SetDropTarget(dnd_load);

  CreateStatusBar(1);
  SetStatusText(wxT("ready"));

  last_dir = wxGetCwd();
}

void
mi_frame::open_file(wxString file_name) {
  string cfile_name;

  cfile_name = wxMB(file_name);
  tree->DeleteAllItems();
  item_ids[0] = tree->AddRoot(file_name);
  last_percent = -1;
  num_elements = 0;
  menu_file->Enable(mi_file_open, false);
  menu_file->Enable(mi_file_savetext, false);
  menu_options->Enable(mi_options_showall, false);

  if (process_file(cfile_name)) {
    file_open = true;
    menu_file->Enable(mi_file_savetext, true);
    current_file = file_name;
    if (expand_important_elements)
      expand_elements();
  } else {
    tree->DeleteAllItems();
    menu_file->Enable(mi_file_savetext, false);
  }
  menu_file->Enable(mi_file_open, true);
  menu_options->Enable(mi_options_showall, true);
  SetStatusText(wxT("ready"));
  tree->Refresh();
}

void
mi_frame::show_progress(int percent,
                        wxString msg) {
  wxString s;

  if ((percent / 5) != (last_percent / 5)) {
    s.Printf(wxT("%s: %d%%"), msg.c_str(), percent);
    SetStatusText(s);
    last_percent = percent;
  }
  wxYield();
}

void
mi_frame::expand_all_elements(wxTreeItemId &root,
                              bool expand) {
  wxTreeItemId child;
  wxTreeItemIdValue cookie;

  if (expand)
    tree->Expand(root);
  else
    tree->Collapse(root);
  child = tree->GetFirstChild(root, cookie);
  while (child > 0) {
    expand_all_elements(child, expand);
    child = tree->GetNextChild(root, cookie);
  }
}

void
mi_frame::expand_elements() {
  wxTreeItemId l0, l1;
  wxTreeItemIdValue cl0, cl1;

  Freeze();

  expand_all_elements(item_ids[0], false);
  tree->Expand(item_ids[0]);

  l0 = tree->GetFirstChild(item_ids[0], cl0);
  while (l0 > 0) {
    tree->Expand(l0);
    if (tree->GetItemText(l0).find(wxT("Segment")) == 0) {
      l1 = tree->GetFirstChild(l0, cl1);
      while (l1 > 0) {
        if ((tree->GetItemText(l1).find(wxT("Segment information")) == 0) ||
            (tree->GetItemText(l1).find(wxT("Segment tracks")) == 0))
          expand_all_elements(l1);
        l1 = tree->GetNextChild(l0, cl1);
      }
    }

    l0 = tree->GetNextChild(item_ids[0], cl0);
  }

  Thaw();
}

void
mi_frame::add_item(int level,
                   wxString text) {
  item_ids[level + 1] = tree->AppendItem(item_ids[level], text);
  num_elements++;
}

void
mi_frame::show_error(wxString msg) {
  wxMessageBox(msg, wxT("Error"), wxOK | wxICON_ERROR | wxCENTER, this);
}

void
mi_frame::save_elements(wxTreeItemId &root,
                        int level,
                        FILE *f) {
  wxTreeItemId child;
  wxTreeItemIdValue cookie;
  char level_buffer[10];
  int pcnt_before, pcnt_now;

  if (level >= 0) {
    memset(&level_buffer[1], ' ', 9);
    level_buffer[0] = '|';
    level_buffer[level] = 0;
    fprintf(f, "(%s) %s+ %s\n", NAME, level_buffer,
            wxMB(tree->GetItemText(root)));
    pcnt_before = elements_saved * 100 / num_elements;
    pcnt_now = (elements_saved + 1) * 100 / num_elements;
    if ((pcnt_before / 5) != (pcnt_now / 5))
      show_progress(pcnt_now, wxT("Writing info"));
    elements_saved++;
  }
  child = tree->GetFirstChild(root, cookie);
  while (child > 0) {
    save_elements(child, level + 1, f);
    child = tree->GetNextChild(root, cookie);
  }
}

void
mi_frame::on_file_open(wxCommandEvent &WXUNUSED(event)) {
  wxFileDialog file_dialog(this, wxT("Select Matroska file"), wxT(""), wxT(""),
                           wxT("Matroska files (*.mkv;*.mka;*.mks)|"
                               "*.mkv;*.mka;*.mks|All files|*.*"));
  file_dialog.SetDirectory(last_dir);
  if (file_dialog.ShowModal() == wxID_OK) {
    open_file(file_dialog.GetPath().c_str());
    last_dir = file_dialog.GetDirectory();
  }
}

void
mi_frame::on_file_savetext(wxCommandEvent &WXUNUSED(event)) {
  FILE *f;

  wxFileDialog file_dialog(this, wxT("Select output file"), wxT(""), wxT(""),
                           wxT("Text files (*.txt)|*.txt|All files|*.*"),
                           wxSAVE | wxOVERWRITE_PROMPT);
  file_dialog.SetDirectory(last_dir);
  if (file_dialog.ShowModal() == wxID_OK) {
    last_dir = file_dialog.GetDirectory();
    f = fopen(wxMB(file_dialog.GetPath()), "w");
    if (f == NULL) {
      wxString s;
      s.Printf(wxT("Could not create the file '%s'."),
               file_dialog.GetPath().c_str());
      show_error(s);
      return;
    }

    menu_file->Enable(mi_file_open, false);
    menu_file->Enable(mi_file_savetext, false);
    menu_options->Enable(mi_options_showall, false);
    save_elements(item_ids[0], -1, f);
    menu_file->Enable(mi_file_open, true);
    menu_file->Enable(mi_file_savetext, true);
    menu_options->Enable(mi_options_showall, true);

    fclose(f);
  }
}

void
mi_frame::on_file_quit(wxCommandEvent &WXUNUSED(event)) {
  Close(true);
}

void
mi_frame::on_options_showall(wxCommandEvent &WXUNUSED(event)) {
  show_all_elements = !show_all_elements;
  menu_options->Check(mi_options_showall, show_all_elements);
  if (show_all_elements)
    verbose = 2;
  else
    verbose = 0;
  if (file_open)
    open_file(current_file);
}

void
mi_frame::on_options_expandimportant(wxCommandEvent &WXUNUSED(event)) {
  expand_important_elements = !expand_important_elements;
  menu_options->Check(mi_options_expandimportant,
                      expand_important_elements);
}

void
mi_frame::on_help_about(wxCommandEvent &WXUNUSED(event)) {
  wxString msg;
  msg.Printf(wxT(VERSIONINFO ".\nCompiled with libebml %s + "
                 "libmatroska %s.\n\nThis program is licensed under the "
                 "GPL v2 (see COPYING).\nIt was written by Moritz Bunkus "
                 "<moritz@bunkus.org>.\nSources and the latest binaries are "
                 "always available at\nhttp://www.bunkus.org/videotools/"
                 "mkvtoolnix/"),
             wxCS2WS(EbmlCodeVersion), wxCS2WS(KaxCodeVersion));
  wxMessageBox(msg, wxT("About mkvinfo"), wxOK | wxICON_INFORMATION, this);
}

void
mi_frame::on_right_click(wxTreeEvent &event) {
  wxTreeItemId item;

  item = event.GetItem();
  if (tree->GetChildrenCount(item) == 0)
    return;
  expand_all_elements(item, !tree->IsExpanded(item));
}

bool
mi_dndfile::OnDropFiles(wxCoord x,
                        wxCoord y,
                        const wxArrayString &filenames) {
  wxString dnd_file;
  unsigned int i;

  for (i = 0; i < filenames.GetCount(); i++) {
    dnd_file = filenames[i];
    if ((dnd_file.Right(3).Lower() == wxT("mka")) ||
        (dnd_file.Right(3).Lower() == wxT("mkv")) ||
        (dnd_file.Right(3).Lower() == wxT("mks"))) {
      frame->open_file(dnd_file);
    } else {
      wxString msg;
      msg.Printf(wxT("The dragged file '%s'\nis not a Matroska file."),
                 dnd_file.c_str());
      frame->show_error(msg.c_str());
      break;
    }
  }
  return true;
}

BEGIN_EVENT_TABLE(mi_frame, wxFrame)
  EVT_MENU(mi_file_open, mi_frame::on_file_open)
  EVT_MENU(mi_file_savetext, mi_frame::on_file_savetext)
  EVT_MENU(mi_file_quit, mi_frame::on_file_quit)
  EVT_MENU(mi_options_showall, mi_frame::on_options_showall)
  EVT_MENU(mi_options_expandimportant, mi_frame::on_options_expandimportant)
  EVT_MENU(mi_help_about, mi_frame::on_help_about)
  EVT_TREE_ITEM_RIGHT_CLICK(4254, mi_frame::on_right_click)
END_EVENT_TABLE()

IMPLEMENT_APP_NO_MAIN(mi_app)

void
ui_show_error(const char *text) {
  if (use_gui)
    frame->show_error(wxU(text));
  else
    console_show_error(text);
}

void
ui_show_element(int level,
                const char *text,
                int64_t position) {
  if (!use_gui)
    console_show_element(level, text, position);

  else if (0 <= position) {
    string buffer = mxsprintf("%s at " LLD, text, position);
    frame->add_item(level, wxU(buffer.c_str()));

  } else
    frame->add_item(level, wxU(text));
}

void
ui_show_progress(int percentage,
                 const char *text) {
  frame->show_progress(percentage, wxU(text));
}

int
ui_run(int argc,
       char **argv) {
  wxEntry(argc, argv);
  return 0;
}

bool
ui_graphical_available() {
  return true;
}

#endif // HAVE_WXWIDGETS
