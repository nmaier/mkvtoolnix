#include "config.h"

#ifdef HAVE_WXWINDOWS

#include "wx/wx.h"

#include "common.h"
#include "mkvinfo.h"
#include "matroska.xpm"

class mi_app: public wxApp {
public:
  virtual bool OnInit();
};

class mi_frame: public wxFrame {
private:
  wxMenu *menu_options, *menu_file;
  bool show_all_elements;
  bool file_open;

public:
  mi_frame(const wxString &title, const wxPoint &pos, const wxSize &size,
           long style = wxDEFAULT_FRAME_STYLE);

  void open_file(const char *file_name);

protected:
  void on_file_open(wxCommandEvent &event);
  void on_file_savetext(wxCommandEvent &event);
  void on_file_quit(wxCommandEvent &event);
  void on_options_showall(wxCommandEvent &event);
  void on_help_about(wxCommandEvent &event);

private:
  DECLARE_EVENT_TABLE()
};

enum {
  mi_file_quit = 1,
  mi_file_open = 2,
  mi_options_showall = 3,
  mi_file_savetext = 4,
  mi_help_about = wxID_ABOUT
};

bool mi_app::OnInit() {
  bool use_gui;
  char *initial_file;

  parse_args(argc, argv, initial_file, use_gui);

  if (use_gui) {
    mi_frame *frame = new mi_frame(_T("mkvinfo"), wxPoint(50, 50),
                                   wxSize(600, 400));
    frame->Show(true);
    frame->Refresh(true);
    frame->Update();

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

  if (verbose == 0)
    show_all_elements = false;
  else
    show_all_elements = true;

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
                    _T("Quit this mkvinfo"));
  menu_options->AppendCheckItem(mi_options_showall,
                                _T("Show &all elements\tCtrl-A"),
                                _T("Parse the file completely and show all "
                                   "elements"));
  menu_options->Check(mi_options_showall, show_all_elements);
  menu_help->Append(mi_help_about, _T("&About...\tF1"),
                    _T("Show about dialog"));

  menu_bar->Append(menu_file, _T("&File"));
  menu_bar->Append(menu_options, _T("&Options"));
  menu_bar->Append(menu_help, _T("&Help"));

  SetMenuBar(menu_bar);

  CreateStatusBar(2);
  SetStatusText(_T("mkvinfo!!"));
}

void mi_frame::open_file(const char *file_name) {
  if (process_file(file_name))
    file_open = true;
}

void mi_frame::on_file_open(wxCommandEvent &WXUNUSED(event)) {
  wxFileDialog file_dialog(this, _T("Select Matroska file"), _T(""), _T(""),
                           _T("Matroska files (*.mkv;*.mka;*.mks)|"
                              "*.mkv;*.mka;*.mks|All files|*.*"));
  file_dialog.SetDirectory(wxGetWorkingDirectory());
  if (file_dialog.ShowModal() == wxID_OK) {
    wxString info;
    info.Printf(_T("Full file name: %s\nPath: %s\nName: %s"),
                file_dialog.GetPath().c_str(),
                file_dialog.GetDirectory().c_str(),
                file_dialog.GetFilename().c_str());
    wxMessageDialog msg_dialog(this, info, _T("Selected file"));
    msg_dialog.ShowModal();
    open_file(file_dialog.GetPath().c_str());
  }
}

void mi_frame::on_file_savetext(wxCommandEvent &WXUNUSED(event)) {
  wxFileDialog file_dialog(this, _T("Select output file"), _T(""), _T(""),
                           _T("Text files (*.txt)|*.txt|All files|*.*"),
                           wxSAVE | wxOVERWRITE_PROMPT);
  file_dialog.SetDirectory(wxGetWorkingDirectory());
  if (file_dialog.ShowModal() == wxID_OK) {
    wxString info;
    info.Printf(_T("Full file name: %s\nPath: %s\nName: %s"),
                file_dialog.GetPath().c_str(),
                file_dialog.GetDirectory().c_str(),
                file_dialog.GetFilename().c_str());
    wxMessageDialog msg_dialog(this, info, _T("Selected file"));
    msg_dialog.ShowModal();
  }
}

void mi_frame::on_file_quit(wxCommandEvent &WXUNUSED(event)) {
  Close(true);
}

void mi_frame::on_options_showall(wxCommandEvent &WXUNUSED(event)) {
  show_all_elements = !show_all_elements;
  menu_options->Check(mi_options_showall, show_all_elements);
}

void mi_frame::on_help_about(wxCommandEvent &WXUNUSED(event)) {
  wxString msg;
  msg.Printf(_T("mkvinfo v" VERSION".\n\nThis program is licensed under the "
                "GPL v2 (see COPYING).\nIt was written by Moritz Bunkus "
                "<moritz@bunkus.org>.\nhttp://www.bunkus.org/videotools/"
                "mkvtoolnix/"));
  wxMessageBox(msg, _T("About mkvinfo"), wxOK | wxICON_INFORMATION, this);
}

BEGIN_EVENT_TABLE(mi_frame, wxFrame)
  EVT_MENU(mi_file_open, mi_frame::on_file_open)
  EVT_MENU(mi_file_savetext, mi_frame::on_file_savetext)
  EVT_MENU(mi_file_quit, mi_frame::on_file_quit)
  EVT_MENU(mi_options_showall, mi_frame::on_options_showall)
  EVT_MENU(mi_help_about, mi_frame::on_help_about)
END_EVENT_TABLE()

IMPLEMENT_APP(mi_app)

#endif // HAVE_WXWINDOWS
