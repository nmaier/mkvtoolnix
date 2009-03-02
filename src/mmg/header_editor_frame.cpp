/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <wx/filename.h>
#include <wx/statline.h>

#include "header_editor_frame.h"
#include "kax_analyzer.h"
#include "mmg.h"
#include "wxcommon.h"

header_editor_frame_c::header_editor_frame_c(wxWindow *parent)
  : wxFrame(parent, wxID_ANY, Z("Header editor"), wxDefaultPosition, wxSize(600, 600))
  , m_file_menu(NULL)
  , m_file_menu_sep(false)
  , m_panel(NULL)
  , m_bs_panel(NULL)
  , m_tb_tree(NULL)
{
  wxPanel *panel = new wxPanel(this);

  m_tb_tree = new wxTreebook(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_LEFT);
  // m_tb_tree->GetTreeCtrl()->SetMinSize(wxSize(300, -1));
  // m_tb_tree->SetAutoLayout(true);

  m_bs_panel = new wxBoxSizer(wxVERTICAL);
  m_bs_panel->Add(m_tb_tree, 1, wxGROW | wxALL, 5);

  panel->SetSizer(m_bs_panel);

  add_empty_page(NULL, Z("No file loaded"), Z("No file has been loaded yet. You can do that by selecting 'Open' from the 'File' menu."));

  m_file_menu = new wxMenu();
  m_file_menu->Append(ID_M_HE_FILE_OPEN, Z("&Open\tCtrl-O"), Z("Open an existing Matroska file"));
  m_file_menu->Append(ID_M_HE_FILE_SAVE, Z("&Save\tCtrl-S"), Z("Save the header values"));
  m_file_menu->AppendSeparator();
  m_file_menu->Append(ID_M_HE_FILE_CLOSE, Z("&Close\tCtrl-W"), Z("Close the header editor"));

  enable_file_save_menu_entry();

  wxMenu *help_menu = new wxMenu();
  help_menu->Append(ID_M_HE_HELP_HELP, Z("&Help\tF1"), Z("Display usage information"));

  wxMenuBar *menu_bar = new wxMenuBar();
  menu_bar->Append(m_file_menu, Z("&File"));
  menu_bar->Append(help_menu, Z("&Help"));
  SetMenuBar(menu_bar);
}

header_editor_frame_c::~header_editor_frame_c() {
}

void
header_editor_frame_c::add_empty_page(void *some_ptr,
                                      const wxString &title,
                                      const wxString &text) {
    // wxPanel *panel = new wxPanel(m_tb_tree);

    // wxString animals[] = { wxT("Fox"), wxT("Hare"), wxT("Rabbit"),
    //     wxT("Sabre-toothed tiger"), wxT("T Rex") };

    // wxRadioBox *radiobox1 = new wxRadioBox(panel, wxID_ANY, wxT("Choose one"),
    //     wxDefaultPosition, wxDefaultSize, 5, animals, 2, wxRA_SPECIFY_ROWS);

    // wxString computers[] = { wxT("Amiga"), wxT("Commodore 64"), wxT("PET"),
    //     wxT("Another") };

    // wxRadioBox *radiobox2 = new wxRadioBox(panel, wxID_ANY,
    //     wxT("Choose your favourite"), wxDefaultPosition, wxDefaultSize,
    //     4, computers, 0, wxRA_SPECIFY_COLS);

    // wxBoxSizer *sizerPanel = new wxBoxSizer(wxVERTICAL);
    // sizerPanel->Add(radiobox1, 2, wxEXPAND);
    // sizerPanel->Add(radiobox2, 1, wxEXPAND);
    // panel->SetSizer(sizerPanel);

    // m_tb_tree->AddPage(panel, wxT("muhkuh"));


  m_bs_panel->Hide(m_tb_tree);

  wxPanel *page   = new wxPanel(m_tb_tree);
  wxBoxSizer *siz = new wxBoxSizer(wxVERTICAL);

  siz->AddSpacer(5);
  siz->Add(new wxStaticText(page, wxID_ANY, title), 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz->AddSpacer(5);
  siz->Add(new wxStaticLine(page),                  0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz->AddSpacer(5);
  siz->Add(new wxStaticText(page, wxID_ANY, text),  0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz->AddStretchSpacer();

  page->SetSizer(siz);

  m_tb_tree->AddPage(page, title);

  m_bs_panel->Show(m_tb_tree);
  m_bs_panel->Layout();
}

void
header_editor_frame_c::on_file_open(wxCommandEvent &evt) {
  add_empty_page(NULL, wxT("/home/mosu/prog/video/mkvtoolnix/data/v.mkv"), wxT("muh and the muhkuh for muh /home/mosu/prog/video/mkvtoolnix/data/v.mkv and alskjhd qweh qweoqh ridfh qwjr0q h /home/mosu/prog/video/mkvtoolnix/data/v.mkv qwoeih qwoie qwehqowhe "));
  return;

  if (open_file(wxFileName(wxT("/home/mosu/prog/video/mkvtoolnix/data/v.mkv"))))
    return;

  wxFileDialog dlg(this, Z("Open a Matroska file"), last_open_dir, wxEmptyString, wxString::Format(Z("Matroska files (*.mkv;*.mka;*.mks)|*.mkv;*.mka;*.mks|%s"), ALLFILES.c_str()), wxFD_OPEN);
  if (dlg.ShowModal() != wxID_OK)
    return;

  if (!open_file(wxFileName(dlg.GetPath())))
    return;

  last_open_dir = dlg.GetDirectory();
}

bool
header_editor_frame_c::open_file(const wxFileName &file_name) {
  if (!kax_analyzer_c::probe(wxMB(file_name.GetFullPath()))) {
    wxMessageBox(Z("The file you tried to open is not a Matroska file."), Z("Wrong file selected"), wxOK | wxCENTER | wxICON_ERROR);
    return false;
  }

  kax_analyzer_c *analyzer = new kax_analyzer_c(this, wxMB(file_name.GetFullPath()));
  if (!analyzer->process()) {
    delete analyzer;
    analyzer = NULL;
    return false;
  }

  m_file_name = file_name;

  SetTitle(wxString::Format(Z("Header editor: %s"), m_file_name.GetFullName().c_str()));

  enable_file_save_menu_entry();

  m_tb_tree->DeleteAllPages();
  add_empty_page(NULL, m_file_name.GetFullPath(), wxString::Format(Z("You are editing the file '%s' in the directory '%s'."), m_file_name.GetFullName().c_str(), m_file_name.GetPath().c_str()));

  delete analyzer;

  return true;
}

void
header_editor_frame_c::on_file_save(wxCommandEvent &evt) {
}

void
header_editor_frame_c::on_close(wxCloseEvent &evt) {
  if (!may_close() && evt.CanVeto())
    evt.Veto();
  else
    Destroy();
}

void
header_editor_frame_c::on_file_close(wxCommandEvent &evt) {
  Close();
}

bool
header_editor_frame_c::may_close() {
  return true;
}

void
header_editor_frame_c::update_file_menu() {
  int i;
  for (i = ID_M_HE_FILE_LOADLAST1; i <= ID_M_HE_FILE_LOADLAST4; i++) {
    wxMenuItem *mi = m_file_menu->Remove(i);
    delete mi;
  }

  // if ((last_settings.size() > 0) && !file_menu_sep) {
  //   file_menu->AppendSeparator();
  //   file_menu_sep = true;
  // }
  // for (i = 0; i < last_settings.size(); i++) {
  //   s.Printf(wxT("&%u. %s"), i + 1, last_settings[i].c_str());
  //   file_menu->Append(ID_M_FILE_LOADLAST1 + i, s);
  // }
}

void
header_editor_frame_c::enable_file_save_menu_entry() {
  m_file_menu->Enable(ID_M_HE_FILE_SAVE, !m_file_name.IsOk());
}

IMPLEMENT_CLASS(header_editor_frame_c, wxFrame);
BEGIN_EVENT_TABLE(header_editor_frame_c, wxFrame)
  EVT_MENU(ID_M_HE_FILE_OPEN,               header_editor_frame_c::on_file_open)
  EVT_MENU(ID_M_HE_FILE_SAVE,               header_editor_frame_c::on_file_save)
  EVT_MENU(ID_M_HE_FILE_CLOSE,              header_editor_frame_c::on_file_close)
  EVT_CLOSE(header_editor_frame_c::on_close)
END_EVENT_TABLE();
