/*
   mkvinfo -- utility for gathering information about Matroska files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   A wxWidgets GUI for mkvinfo

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#ifdef HAVE_WXWIDGETS

#include <wx/wx.h>
#include <wx/config.h>
#include <wx/treectrl.h>
#include <wx/dnd.h>

#include <ebml/EbmlVersion.h>
#include <matroska/KaxVersion.h>

#include "common/common_pch.h"
#include "common/translation.h"
#include "common/version.h"
#include "common/wx.h"
#include "info/wxwidgets_ui.h"
#include "info/mkvinfo.h"
#include "share/icons/32x32/mkvinfo.xpm"

using namespace libebml;
using namespace libmatroska;

static mi_frame *frame;

enum {
  mi_file_quit               = 1,
  mi_file_open               = 2,
  mi_file_savetext           = 3,
  mi_options_showall         = 101,
  mi_options_expandimportant = 102,
  mi_options_show_sizes      = 103,
  mi_help_about              = wxID_ABOUT
};

std::string
mi_app::get_ui_locale() {
  std::string locale;

#if defined(HAVE_LIBINTL_H)
  wxString w_locale;

  translation_c::initialize_available_translations();

  wxConfigBase *cfg = wxConfigBase::Get();
  cfg->SetPath(wxT("/GUI"));
  if (cfg->Read(wxT("ui_locale"), &w_locale)) {
    locale = wxMB(w_locale);
    if (-1 == translation_c::look_up_translation(locale))
      locale = "";
  }
#endif  // HAVE_LIBINTL_H

  return locale;
}

bool
mi_app::OnInit() {
  wxConfigBase *cfg = new wxConfig(wxT("mkvmergeGUI"));
  wxConfigBase::Set(cfg);

  setup(get_ui_locale());

  frame = new mi_frame(wxT("mkvinfo"), wxPoint(50, 50), wxSize(600, 400));
  frame->Show(true);
  frame->Refresh(true);
  frame->Update();
  while (Pending())
    Dispatch();

  if (g_options.m_file_name != "")
    frame->open_file(wxU(g_options.m_file_name));

  return true;
}

mi_frame::mi_frame(const wxString &title,
                   const wxPoint &pos,
                   const wxSize &size,
                   long style)
  : wxFrame(nullptr, -1, title, pos, size, style)
  , dnd_load(new mi_dndfile)
  , show_all_elements(0 != g_options.m_verbose)
  , expand_important_elements(true)
  , file_open(false)
  , tree(new wxTreeCtrl(this, 4254))
{
  SetIcon(wxIcon(mkvinfo_xpm));

  menu_file           = new wxMenu();
  menu_options        = new wxMenu();
  wxMenu *menu_help   = new wxMenu();
  wxMenuBar *menu_bar = new wxMenuBar();

  menu_file->Append(mi_file_open,     Z("&Open\tCtrl-O"),              Z("Open a Matroska file"));
  menu_file->Append(mi_file_savetext, Z("&Save info as text\tCtrl-S"), Z("Saves the information from the current file to a text file"));
  menu_file->Enable(mi_file_savetext, false);
  menu_file->AppendSeparator();
  menu_file->Append(mi_file_quit,     Z("E&xit\tCtrl-Q"),              Z("Quits mkvinfo"));

  menu_options->AppendCheckItem(mi_options_showall,         Z("Show &all elements\tCtrl-A"),         Z("Parse the file completely and show all elements"));
  menu_options->AppendCheckItem(mi_options_show_sizes,      Z("Show element si&zes\tCtrl-Z"),        Z("Show the size of each element including its header"));
  menu_options->AppendCheckItem(mi_options_expandimportant, Z("&Expand important elements\tCtrl-E"), Z("After loading a file expand the most important elements"));

  menu_options->Check(mi_options_showall,         show_all_elements);
  menu_options->Check(mi_options_show_sizes,      g_options.m_show_size);
  menu_options->Check(mi_options_expandimportant, expand_important_elements);

  menu_help->Append(mi_help_about, Z("&About\tF1"), Z("Show about dialog"));

  menu_bar->Append(menu_file,    Z("&File"));
  menu_bar->Append(menu_options, Z("&Options"));
  menu_bar->Append(menu_help,    Z("&Help"));

  SetMenuBar(menu_bar);

  tree->SetDropTarget(dnd_load);

  CreateStatusBar(1);
  SetStatusText(Z("ready"));
}

void
mi_frame::open_file(wxString file_name) {
  std::string cfile_name;

  tree->DeleteAllItems();

  cfile_name   = wxMB(file_name);
  item_ids[0]  = tree->AddRoot(file_name);
  last_percent = -1;
  num_elements = 0;

  menu_file->Enable(mi_file_open,          false);
  menu_file->Enable(mi_file_savetext,      false);
  menu_options->Enable(mi_options_showall, false);

  if (process_file(cfile_name)) {
    menu_file->Enable(mi_file_savetext, true);
    file_open    = true;
    current_file = file_name;
    if (expand_important_elements)
      expand_elements();

  } else {
    tree->DeleteAllItems();
    menu_file->Enable(mi_file_savetext, false);
  }

  menu_file->Enable(mi_file_open,          true);
  menu_options->Enable(mi_options_showall, true);
  SetStatusText(Z("ready"));
  tree->Refresh();
}

void
mi_frame::show_progress(int percent,
                        wxString msg) {
  if ((percent / 5) != (last_percent / 5)) {
    SetStatusText(wxString::Format(wxT("%s: %d%%"), msg.c_str(), percent));
    last_percent = percent;
  }
  wxYield();
}

void
mi_frame::expand_all_elements(wxTreeItemId &root,
                              bool expand) {
  if (expand)
    tree->Expand(root);
  else
    tree->Collapse(root);

  wxTreeItemIdValue cookie;
  wxTreeItemId child = tree->GetFirstChild(root, cookie);
  while (child > 0) {
    expand_all_elements(child, expand);
    child = tree->GetNextChild(root, cookie);
  }
}

void
mi_frame::expand_elements() {
  Freeze();

  expand_all_elements(item_ids[0], false);
  tree->Expand(item_ids[0]);

  wxTreeItemIdValue cl0, cl1;
  wxTreeItemId l0 = tree->GetFirstChild(item_ids[0], cl0);
  while (l0 > 0) {
    tree->Expand(l0);
    if (tree->GetItemText(l0).find(Z("Segment")) == 0) {
      wxTreeItemId l1 = tree->GetFirstChild(l0, cl1);
      while (l1 > 0) {
        if ((tree->GetItemText(l1).find(Z("Segment information")) == 0) || (tree->GetItemText(l1).find(Z("Segment tracks")) == 0))
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
  wxMessageBox(msg, Z("Error"), wxOK | wxICON_ERROR | wxCENTER, this);
}

void
mi_frame::save_elements(wxTreeItemId &root,
                        int level,
                        FILE *f) {
  if (0 <= level) {
    char level_buffer[10];
    memset(&level_buffer[1], ' ', 9);

    level_buffer[0]     = '|';
    level_buffer[level] = 0;
    int pcnt_before     =  elements_saved      * 100 / num_elements;
    int pcnt_now        = (elements_saved + 1) * 100 / num_elements;

    fprintf(f, "(%s) %s+ %s\n", NAME, level_buffer, wxMB(tree->GetItemText(root)));

    if ((pcnt_before / 5) != (pcnt_now / 5))
      show_progress(pcnt_now, Z("Writing info"));

    elements_saved++;
  }

  wxTreeItemIdValue cookie;
  wxTreeItemId child = tree->GetFirstChild(root, cookie);
  while (0 < child) {
    save_elements(child, level + 1, f);
    child = tree->GetNextChild(root, cookie);
  }
}

void
mi_frame::on_file_open(wxCommandEvent &WXUNUSED(event)) {
  wxFileDialog file_dialog(this, Z("Select Matroska file"), wxT(""), wxT(""),
                           Z("All supported files|*.mkv;*.mka;*.mks;*.mk3d;*.webm;*.webma;*.webmv|"
                             "Matroska files (*.mkv;*.mka;*.mks;*.mk3d)|*.mkv;*.mka;*.mks;*.mk3d|"
                             "WebM files (*.webm;*.webma;*.webmv)|*.webm;*.webma;*.webmv|"
                             "All files|*.*"));
  file_dialog.SetDirectory(last_dir);
  if (file_dialog.ShowModal() == wxID_OK) {
    open_file(file_dialog.GetPath().c_str());
    last_dir = file_dialog.GetDirectory();
  }
}

void
mi_frame::on_file_savetext(wxCommandEvent &WXUNUSED(event)) {
  wxFileDialog file_dialog(this, Z("Select output file"), wxT(""), wxT(""), Z("Text files (*.txt)|*.txt|All files|*.*"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
  file_dialog.SetDirectory(last_dir);
  if (file_dialog.ShowModal() == wxID_OK) {
    last_dir = file_dialog.GetDirectory();
    FILE *f = fopen(wxMB(file_dialog.GetPath()), "w");
    if (nullptr == f) {
      show_error(wxString::Format(Z("Could not create the file '%s'."), file_dialog.GetPath().c_str()));
      return;
    }

    menu_file->Enable(mi_file_open,          false);
    menu_file->Enable(mi_file_savetext,      false);
    menu_options->Enable(mi_options_showall, false);
    save_elements(item_ids[0], -1,           f);
    menu_file->Enable(mi_file_open,          true);
    menu_file->Enable(mi_file_savetext,      true);
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
  show_all_elements   = !show_all_elements;
  g_options.m_verbose = show_all_elements ? 2 : 0;
  menu_options->Check(mi_options_showall, show_all_elements);

  if (file_open)
    open_file(current_file);
}

void
mi_frame::on_options_show_sizes(wxCommandEvent &WXUNUSED(event)) {
  g_options.m_show_size = !g_options.m_show_size;
  menu_options->Check(mi_options_show_sizes, g_options.m_show_size);

  if (file_open)
    open_file(current_file);
}

void
mi_frame::on_options_expandimportant(wxCommandEvent &WXUNUSED(event)) {
  expand_important_elements = !expand_important_elements;
  menu_options->Check(mi_options_expandimportant, expand_important_elements);
}

void
mi_frame::on_help_about(wxCommandEvent &WXUNUSED(event)) {
  wxString msg;
  msg.Printf(Z("%s.\nCompiled with libebml %s + libmatroska %s.\n\n"
               "This program is licensed under the GPL v2 (see COPYING).\n"
               "It was written by Moritz Bunkus <moritz@bunkus.org>.\n"
               "Sources and the latest binaries are always available at\n"
               "http://www.bunkus.org/videotools/mkvtoolnix/"),
             wxUCS(get_version_info("mkvinfo GUI")), wxUCS(EbmlCodeVersion), wxUCS(KaxCodeVersion));
  wxMessageBox(msg, Z("About mkvinfo"), wxOK | wxICON_INFORMATION, this);
}

void
mi_frame::on_right_click(wxTreeEvent &event) {
  wxTreeItemId item = event.GetItem();
  if (tree->GetChildrenCount(item) == 0)
    return;

  expand_all_elements(item, !tree->IsExpanded(item));
}

bool
mi_dndfile::OnDropFiles(wxCoord,
                        wxCoord,
                        const wxArrayString &filenames) {
  unsigned int i;

  for (i = 0; i < filenames.GetCount(); i++) {
    wxString extension = filenames[i].AfterLast(wxT('.')).Lower();

    if (   (extension == wxT("mkv"))  || (extension == wxT("mka"))   || (extension == wxT("mks")   || (extension == wxT("mk3d")))
        || (extension == wxT("webm")) || (extension == wxT("webmv")) || (extension == wxT("webma")))
      frame->open_file(filenames[i]);

    else {
      frame->show_error(wxString::Format(Z("The dragged file '%s'\nis not a Matroska file."), filenames[i].c_str()));
      break;
    }
  }

  return true;
}

BEGIN_EVENT_TABLE(mi_frame, wxFrame)
  EVT_MENU(mi_file_open,               mi_frame::on_file_open)
  EVT_MENU(mi_file_savetext,           mi_frame::on_file_savetext)
  EVT_MENU(mi_file_quit,               mi_frame::on_file_quit)
  EVT_MENU(mi_options_showall,         mi_frame::on_options_showall)
  EVT_MENU(mi_options_show_sizes,      mi_frame::on_options_show_sizes)
  EVT_MENU(mi_options_expandimportant, mi_frame::on_options_expandimportant)
  EVT_MENU(mi_help_about,              mi_frame::on_help_about)
  EVT_TREE_ITEM_RIGHT_CLICK(4254,      mi_frame::on_right_click)
END_EVENT_TABLE()

IMPLEMENT_APP_NO_MAIN(mi_app)

void
ui_show_error(const std::string &error) {
  if (g_options.m_use_gui)
    frame->show_error(wxU(error.c_str()));
  else
    console_show_error(error);
}

void
ui_show_element(int level,
                const std::string &text,
                int64_t position,
                int64_t size) {
  if (!g_options.m_use_gui)
    console_show_element(level, text, position, size);

  else if (0 <= position)
    frame->add_item(level, wxU(create_element_text(text, position, size).c_str()));

  else
    frame->add_item(level, wxU(text.c_str()));
}

void
ui_show_progress(int percentage,
                 const std::string &text) {
  frame->show_progress(percentage, wxU(text.c_str()));
}

int
ui_run(int argc,
       char **argv) {
#if defined(SYS_WINDOWS)
  FreeConsole();
#endif

  wxEntry(argc, argv);
  return 0;
}

bool
ui_graphical_available() {
  return true;
}

#endif // HAVE_WXWIDGETS
