#include "config.h"

#ifdef HAVE_WXWINDOWS

#include "wx/wx.h"
#include "wx/treectrl.h"

#include "common.h"
#include "mkvinfo.h"
#include "matroska.xpm"

mi_frame *frame;

enum {
  mi_file_quit = 1,
  mi_file_open = 2,
  mi_file_savetext = 3,
  mi_options_showall = 101,
  mi_options_expandall = 102,
  mi_help_about = wxID_ABOUT
};

bool mi_app::OnInit() {
  char *initial_file;

  parse_args(argc, argv, initial_file, use_gui);

  if (use_gui) {
    frame = new mi_frame(_T("mkvinfo"), wxPoint(50, 50), wxSize(600, 400));
    frame->Show(true);
    frame->Refresh(true);
    frame->Update();
    while (Pending())
      Dispatch();

    if (initial_file != NULL)
      frame->open_file(initial_file);

    return true;
  } else {
    console_main(argc, argv);

    return false;
  }
}

mi_frame::mi_frame(const wxString &title, const wxPoint &pos,
                   const wxSize &size, long style) :
  wxFrame(NULL, -1, title, pos, size, style) {
  wxMenu *menu_help;
  wxMenuBar *menu_bar;

  file_open = false;
  tree = NULL;

  if (verbose == 0)
    show_all_elements = false;
  else
    show_all_elements = true;
  show_all_elements_expanded = false;

  SetIcon(wxICON(matroska));

  menu_file = new wxMenu();
  menu_options = new wxMenu();
  menu_help = new wxMenu();
  menu_bar = new wxMenuBar();

  menu_file->Append(mi_file_open, _T("&Open\tCtrl-O"),
                    _T("Open a Maotroska file"));
  menu_file->Append(mi_file_savetext, _T("&Save info as text\tCtrl-S"),
                    _T("Saves the information from the current file to a "
                       "text file"));
  menu_file->Enable(mi_file_savetext, false);
  menu_file->AppendSeparator();
  menu_file->Append(mi_file_quit, _T("E&xit\tCtrl-Q"),
                    _T("Quits mkvinfo"));
  menu_options->AppendCheckItem(mi_options_showall,
                                _T("Show &all elements\tCtrl-A"),
                                _T("Parse the file completely and show all "
                                   "elements"));
  menu_options->Check(mi_options_showall, show_all_elements);
//   menu_options->AppendCheckItem(mi_options_expandall,
//                                 _T("&Expand all elements\tCtrl-E"),
//                                 _T("Show all elements expanded and not only "
//                                    "the most important ones"));
//   menu_options->Check(mi_options_expandall, false);
  menu_help->Append(mi_help_about, _T("&About...\tF1"),
                    _T("Show about dialog"));

  menu_bar->Append(menu_file, _T("&File"));
  menu_bar->Append(menu_options, _T("&Options"));
  menu_bar->Append(menu_help, _T("&Help"));

  SetMenuBar(menu_bar);

  CreateStatusBar(1);
  SetStatusText(_T("ready"));
}

void mi_frame::open_file(const char *file_name) {
  if (tree != NULL)
    tree->Destroy();
  tree = new wxTreeCtrl(this, -1);
  item_ids[0] = tree->AddRoot(file_name);
  last_percent = -1;
  tree->Freeze();
  tree->Hide();
  num_elements = 0;
  if (process_file(file_name)) {
    file_open = true;
    menu_file->Enable(mi_file_savetext, true);
    current_file = file_name;
    expand_elements();
    tree->Thaw();
    tree->Show();
  } else {
    tree->Destroy();
    menu_file->Enable(mi_file_savetext, false);
  }
  SetStatusText(_T("ready"));
}

void mi_frame::show_progress(int percent, const char *msg) {
  wxString s;
  mi_app *app;

  if ((percent / 5) == (last_percent / 5))
    return;
  s.Printf("%s: %d%%", msg, percent);
  SetStatusText(s);
  app = &wxGetApp();
  while (app->Pending())
    app->Dispatch();
  last_percent = percent;
}

void mi_frame::expand_all_elements(wxTreeItemId &root, bool expand) {
  wxTreeItemId child;
  long cookie;

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

void mi_frame::expand_elements() {
  wxTreeItemId l0, l1;
  long cl0, cl1;

  Freeze();

  if (show_all_elements_expanded) {
    expand_all_elements(item_ids[0]);
    Thaw();
    return;
  }

  expand_all_elements(item_ids[0], false);
  tree->Expand(item_ids[0]);

  l0 = tree->GetFirstChild(item_ids[0], cl0);
  while (l0 > 0) {
    tree->Expand(l0);
    if (tree->GetItemText(l0).find("Segment") == 0) {
      l1 = tree->GetFirstChild(l0, cl1);
      while (l1 > 0) {
        if ((tree->GetItemText(l1).find("Segment information") == 0) ||
            (tree->GetItemText(l1).find("Segment tracks") == 0))
          expand_all_elements(l1);
        l1 = tree->GetNextChild(l0, cl1);
      }
    }

    l0 = tree->GetNextChild(item_ids[0], cl0);
  }

  Thaw();
}

void mi_frame::add_item(int level, const char *text) {
  item_ids[level + 1] = tree->AppendItem(item_ids[level], text);
  num_elements++;
}

void mi_frame::show_error(const char *msg) {
  wxMessageBox(msg, _T("Error"), wxOK | wxICON_ERROR | wxCENTER, this);
}

void mi_frame::save_elements(wxTreeItemId &root, int level, FILE *f) {
  wxTreeItemId child;
  long cookie;
  char level_buffer[10];
  int pcnt_before, pcnt_now;

  if (level >= 0) {
    memset(&level_buffer[1], ' ', 9);
    level_buffer[0] = '|';
    level_buffer[level] = 0;
    fprintf(f, "(%s) %s+ %s\n", NAME, level_buffer,
            tree->GetItemText(root).c_str());
    pcnt_before = elements_saved * 100 / num_elements;
    pcnt_now = (elements_saved + 1) * 100 / num_elements;
    if ((pcnt_before / 5) != (pcnt_now / 5))
      show_progress(pcnt_now, "Writing info");
    elements_saved++;
  }
  child = tree->GetFirstChild(root, cookie);
  while (child > 0) {
    save_elements(child, level + 1, f);
    child = tree->GetNextChild(root, cookie);
  }
}

void mi_frame::on_file_open(wxCommandEvent &WXUNUSED(event)) {
  wxFileDialog file_dialog(this, _T("Select Matroska file"), _T(""), _T(""),
                           _T("Matroska files (*.mkv;*.mka;*.mks)|"
                              "*.mkv;*.mka;*.mks|All files|*.*"));
  file_dialog.SetDirectory(wxGetWorkingDirectory());
  if (file_dialog.ShowModal() == wxID_OK)
    open_file(file_dialog.GetPath().c_str());
}

void mi_frame::on_file_savetext(wxCommandEvent &WXUNUSED(event)) {
  FILE *f;

  wxFileDialog file_dialog(this, _T("Select output file"), _T(""), _T(""),
                           _T("Text files (*.txt)|*.txt|All files|*.*"),
                           wxSAVE | wxOVERWRITE_PROMPT);
  file_dialog.SetDirectory(wxGetWorkingDirectory());
  if (file_dialog.ShowModal() == wxID_OK) {
    f = fopen(file_dialog.GetPath().c_str(), "w");
    if (f == NULL) {
      wxString s;
      s.Printf("Could not create the file '%s'.",
               file_dialog.GetPath().c_str());
      show_error(s.c_str());
      return;
    }

    save_elements(item_ids[0], -1, f);

    fclose(f);
  }
}

void mi_frame::on_file_quit(wxCommandEvent &WXUNUSED(event)) {
  Close(true);
}

void mi_frame::on_options_showall(wxCommandEvent &WXUNUSED(event)) {
  show_all_elements = !show_all_elements;
  menu_options->Check(mi_options_showall, show_all_elements);
  if (show_all_elements)
    verbose = 2;
  else
    verbose = 0;
  if (file_open)
    open_file(current_file);
}

void mi_frame::on_options_expandall(wxCommandEvent &WXUNUSED(event)) {
  show_all_elements_expanded = !show_all_elements_expanded;
  menu_options->Check(mi_options_expandall, show_all_elements_expanded);
  if (file_open) {
    tree->Hide();
    expand_elements();
    tree->Show();
  }
}

void mi_frame::on_help_about(wxCommandEvent &WXUNUSED(event)) {
  wxString msg;
  msg.Printf(_T("mkvinfo v" VERSION".\n\nThis program is licensed under the "
                "GPL v2 (see COPYING).\nIt was written by Moritz Bunkus "
                "<moritz@bunkus.org>.\nSources and the latest binaries are "
                "always available at\nhttp://www.bunkus.org/videotools/"
                "mkvtoolnix/"));
  wxMessageBox(msg, _T("About mkvinfo"), wxOK | wxICON_INFORMATION, this);
}

BEGIN_EVENT_TABLE(mi_frame, wxFrame)
  EVT_MENU(mi_file_open, mi_frame::on_file_open)
  EVT_MENU(mi_file_savetext, mi_frame::on_file_savetext)
  EVT_MENU(mi_file_quit, mi_frame::on_file_quit)
  EVT_MENU(mi_options_showall, mi_frame::on_options_showall)
  EVT_MENU(mi_options_expandall, mi_frame::on_options_expandall)
  EVT_MENU(mi_help_about, mi_frame::on_help_about)
END_EVENT_TABLE()

IMPLEMENT_APP(mi_app)

#endif // HAVE_WXWINDOWS
