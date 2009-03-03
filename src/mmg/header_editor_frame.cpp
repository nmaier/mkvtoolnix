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
#include <wx/button.h>
#include <wx/statline.h>

#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackEntryData.h>

#include "commonebml.h"
#include "header_editor_frame.h"
#include "he_empty_page.h"
#include "he_string_value_page.h"
#include "he_unsigned_integer_value_page.h"
#include "mmg.h"
#include "wxcommon.h"

header_editor_frame_c::header_editor_frame_c(wxWindow *parent)
  : wxFrame(parent, wxID_ANY, Z("Header editor"), wxDefaultPosition, wxSize(800, 600))
  , m_file_menu(NULL)
  , m_file_menu_sep(false)
  , m_panel(NULL)
  , m_bs_panel(NULL)
  , m_tb_tree(NULL)
  , m_analyzer(NULL)
{
  wxPanel *panel = new wxPanel(this);

  m_tb_tree = new wxTreebook(panel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxNB_LEFT);

  m_bs_panel = new wxBoxSizer(wxVERTICAL);
  m_bs_panel->Add(m_tb_tree, 1, wxGROW | wxALL, 5);

  panel->SetSizer(m_bs_panel);

  clear_pages();

  m_file_menu = new wxMenu();
  m_file_menu->Append(ID_M_HE_FILE_OPEN,  Z("&Open\tCtrl-O"),  Z("Open an existing Matroska file"));
  m_file_menu->Append(ID_M_HE_FILE_SAVE,  Z("&Save\tCtrl-S"),  Z("Save the header values"));
  m_file_menu->Append(ID_M_HE_FILE_CLOSE, Z("&Close\tCtrl-W"), Z("Close the current file without saving"));
  m_file_menu->AppendSeparator();
  m_file_menu->Append(ID_M_HE_FILE_QUIT,  Z("&Quit\tCtrl-Q"),  Z("Quit the header editor"));

  m_headers_menu = new wxMenu();
  m_headers_menu->Append(ID_M_HE_HEADERS_EXPAND_ALL,   Z("&Expand all entries\tCtrl-E"),   Z("Epand all entries so that their sub-entries will be shown"));
  m_headers_menu->Append(ID_M_HE_HEADERS_COLLAPSE_ALL, Z("&Collapse all entries\tCtrl-P"), Z("Collapse all entries so that none of their sub-entries will be shown"));
  m_headers_menu->AppendSeparator();
  m_headers_menu->Append(ID_M_HE_HEADERS_VALIDATE,     Z("&Validate\tCtrl-T"),             Z("Validates the content of all changeable headers"));

  wxMenu *help_menu = new wxMenu();
  help_menu->Append(ID_M_HE_HELP_HELP, Z("&Help\tF1"), Z("Display usage information"));

  enable_menu_entries();

  wxMenuBar *menu_bar = new wxMenuBar();
  menu_bar->Append(m_file_menu,    Z("&File"));
  menu_bar->Append(m_headers_menu, Z("H&eaders"));
  menu_bar->Append(help_menu,      Z("&Help"));
  SetMenuBar(menu_bar);
}

header_editor_frame_c::~header_editor_frame_c() {
  delete m_analyzer;
}

bool
header_editor_frame_c::have_been_modified(std::vector<he_page_base_c *> &pages) {
  std::vector<he_page_base_c *>::iterator it = pages.begin();
  while (it != pages.end()) {
    if ((*it)->has_been_modified() || have_been_modified((*it)->m_children))
      return true;
    ++it;
  }

  return false;
}

int
header_editor_frame_c::validate_pages(std::vector<he_page_base_c *> &pages) {
  std::vector<he_page_base_c *>::iterator it = pages.begin();
  while (it != pages.end()) {
    if (!(*it)->validate())
      return (*it)->m_page_id;

    int result = validate_pages((*it)->m_children);
    if (-1 != result)
      return result;

    ++it;
  }

  return -1;
}

void
header_editor_frame_c::clear_pages() {
  m_bs_panel->Hide(m_tb_tree);

  m_tb_tree->DeleteAllPages();
  m_pages.clear();

  he_empty_page_c *page = new he_empty_page_c(m_tb_tree, Z("No file loaded"), Z("No file has been loaded yet. You can open a file by selecting 'Open' from the 'File' menu."));

  m_tb_tree->AddPage(page, Z("No file loaded"));

  m_bs_panel->Show(m_tb_tree);
  m_bs_panel->Layout();
}

void
header_editor_frame_c::on_file_open(wxCommandEvent &evt) {
  wxString home;
  wxGetEnv(wxT("HOME"), &home);
  open_file(wxFileName(wxString::Format(wxT("%s/prog/video/mkvtoolnix/data/v.mkv"), home.c_str())));
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

  if (NULL != m_analyzer) {
    delete m_analyzer;
    m_analyzer = NULL;
  }

  m_analyzer = new kax_analyzer_c(this, wxMB(file_name.GetFullPath()));
  if (!m_analyzer->process()) {
    delete m_analyzer;
    m_analyzer = NULL;
    return false;
  }

  m_file_name = file_name;

  SetTitle(wxString::Format(Z("Header editor: %s"), m_file_name.GetFullName().c_str()));

  enable_menu_entries();

  m_bs_panel->Hide(m_tb_tree);

  m_tb_tree->DeleteAllPages();
  m_pages.clear();

  int i;
  for (i = 0; m_analyzer->data.size() > i; ++i) {
    analyzer_data_c *data = m_analyzer->data[i];

    if (data->id == KaxInfo::ClassInfos.GlobalId)
      handle_segment_info(data);

    else if (data->id == KaxTracks::ClassInfos.GlobalId)
      handle_tracks(data);
  }

  m_bs_panel->Show(m_tb_tree);
  m_bs_panel->Layout();

  return true;
}

void
header_editor_frame_c::handle_segment_info(analyzer_data_c *data) {
  EbmlElement *e = m_analyzer->read_element(data, KaxInfo::ClassInfos);
  if (NULL == e)
    return;

  he_empty_page_c *page = new he_empty_page_c(m_tb_tree, Z("Segment information"), wxEmptyString);
  m_tb_tree->AddPage(page, Z("Segment information"));
  page->m_storage = e;
  m_pages.push_back(page);

  KaxInfo *info = static_cast<KaxInfo *>(e);
  he_value_page_c *child_page;

  child_page = new he_string_value_page_c(m_tb_tree, info, KaxTitle::ClassInfos.GlobalId, Z("Title"), Z("The title for the whole movie."));
  child_page->init();
  page->m_children.push_back(child_page);

  child_page = new he_string_value_page_c(m_tb_tree, info, KaxSegmentFilename::ClassInfos.GlobalId, Z("Segment filename"), Z("The file name for this segment."));
  child_page->init();
  page->m_children.push_back(child_page);

  child_page = new he_string_value_page_c(m_tb_tree, info, KaxPrevFilename::ClassInfos.GlobalId, Z("Previous filename"), Z("The previous segment's file name."));
  child_page->init();
  page->m_children.push_back(child_page);

  child_page = new he_string_value_page_c(m_tb_tree, info, KaxNextFilename::ClassInfos.GlobalId, Z("Next filename"), Z("The next segment's file name."));
  child_page->init();
  page->m_children.push_back(child_page);

  // child_page = new he_string_value_page_c(m_tb_tree, info, Kax::ClassInfos.GlobalId, Z(""), Z("."));
  // child_page->init();
  // page->m_children.push_back(child_page);

  // child_page = new he_string_value_page_c(m_tb_tree, info, Kax::ClassInfos.GlobalId, Z(""), Z("."));
  // child_page->init();
  // page->m_children.push_back(child_page);

  // child_page = new he_string_value_page_c(m_tb_tree, info, Kax::ClassInfos.GlobalId, Z(""), Z("."));
  // child_page->init();
  // page->m_children.push_back(child_page);

  // child_page = new he_string_value_page_c(m_tb_tree, info, Kax::ClassInfos.GlobalId, Z(""), Z("."));
  // child_page->init();
  // page->m_children.push_back(child_page);

  m_tb_tree->ExpandNode(page->m_page_id);
}

void
header_editor_frame_c::handle_tracks(analyzer_data_c *data) {
  EbmlElement *e = m_analyzer->read_element(data, KaxTracks::ClassInfos);
  if (NULL == e)
    return;

  KaxTracks *tracks = static_cast<KaxTracks *>(e);
  int i;
  for (i = 0; tracks->ListSize() > i; ++i) {
    KaxTrackEntry *k_track_entry = dynamic_cast<KaxTrackEntry *>((*tracks)[i]);
    if (NULL == k_track_entry)
      continue;

    KaxTrackType *k_track_type = dynamic_cast<KaxTrackType *>(FINDFIRST(k_track_entry, KaxTrackType));
    if (NULL == k_track_type)
      continue;

    unsigned int track_number = 0;
    KaxTrackNumber *k_track_number = dynamic_cast<KaxTrackNumber *>(FINDFIRST(k_track_entry, KaxTrackNumber));
    if (NULL != k_track_number)
      track_number = uint64(*k_track_number);

    wxString title;
    switch (uint64(*k_track_type)) {
      case track_audio:
        title.Printf(Z("Audio track %u"), track_number);
        break;
      case track_video:
        title.Printf(Z("Video track %u"), track_number);
        break;
      case track_subtitle:
        title.Printf(Z("Subtitle track %u"), track_number);
        break;
      default:
        continue;
    }

    he_empty_page_c *page = new he_empty_page_c(m_tb_tree, title, wxEmptyString);
    m_tb_tree->AddPage(page, title);
    m_pages.push_back(page);

    he_value_page_c *child_page;

    child_page = new he_unsigned_integer_value_page_c(m_tb_tree, k_track_entry, KaxTrackUID::ClassInfos.GlobalId, Z("Track UID"), Z("This track's unique identifier."));
    child_page->init();
    page->m_children.push_back(child_page);

    m_tb_tree->ExpandNode(page->m_page_id);
  }

  delete e;
}

void
header_editor_frame_c::on_file_save(wxCommandEvent &evt) {
}

void
header_editor_frame_c::on_file_close(wxCommandEvent &evt) {
  if (   have_been_modified(m_pages)
      && (wxYES != wxMessageBox(Z("Some header values have been modified. Do you really want to close without saving the file?"), Z("Headers modified"),
                                wxYES_NO | wxICON_QUESTION, this)))
    return;

  clear_pages();

  delete m_analyzer;
  m_analyzer = NULL;

  m_file_name.Clear();

  enable_menu_entries();
}

void
header_editor_frame_c::on_close_window(wxCloseEvent &evt) {
  if (!may_close() && evt.CanVeto())
    evt.Veto();
  else
    Destroy();
}

void
header_editor_frame_c::on_file_quit(wxCommandEvent &evt) {
  Close();
}

void
header_editor_frame_c::on_headers_expand_all(wxCommandEvent &evt) {
  m_tb_tree->Freeze();
  int i;
  for (i = 0; m_tb_tree->GetTreeCtrl()->GetCount() > i; ++i)
    m_tb_tree->ExpandNode(i);
  m_tb_tree->Thaw();
}

void
header_editor_frame_c::on_headers_collapse_all(wxCommandEvent &evt) {
  m_tb_tree->Freeze();
  int i;
  for (i = 0; m_tb_tree->GetTreeCtrl()->GetCount() > i; ++i)
    m_tb_tree->CollapseNode(i);
  m_tb_tree->Thaw();
}

void
header_editor_frame_c::on_headers_validate(wxCommandEvent &evt) {
  validate();
}

bool
header_editor_frame_c::validate() {
  int page_id = validate_pages(m_pages);

  if (-1 == page_id) {
    wxMessageBox(Z("All header values are OK."), Z("Header validation"), wxOK | wxICON_INFORMATION, this);
    return true;
  }

  m_tb_tree->SetSelection(page_id);
  wxMessageBox(Z("There were errors in the header values. The first error has been selected."), Z("Header validation"), wxOK | wxICON_ERROR, this);

  return false;
}

void
header_editor_frame_c::on_help_help(wxCommandEvent &evt) {
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
header_editor_frame_c::enable_menu_entries() {
  m_file_menu->Enable(ID_M_HE_FILE_SAVE,               m_file_name.IsOk());
  m_file_menu->Enable(ID_M_HE_FILE_CLOSE,              m_file_name.IsOk());
  m_headers_menu->Enable(ID_M_HE_HEADERS_EXPAND_ALL,   m_file_name.IsOk());
  m_headers_menu->Enable(ID_M_HE_HEADERS_COLLAPSE_ALL, m_file_name.IsOk());
  m_headers_menu->Enable(ID_M_HE_HEADERS_VALIDATE,     m_file_name.IsOk());
}

IMPLEMENT_CLASS(header_editor_frame_c, wxFrame);
BEGIN_EVENT_TABLE(header_editor_frame_c, wxFrame)
  EVT_MENU(ID_M_HE_FILE_OPEN,               header_editor_frame_c::on_file_open)
  EVT_MENU(ID_M_HE_FILE_SAVE,               header_editor_frame_c::on_file_save)
  EVT_MENU(ID_M_HE_FILE_CLOSE,              header_editor_frame_c::on_file_close)
  EVT_MENU(ID_M_HE_FILE_QUIT,               header_editor_frame_c::on_file_quit)
  EVT_MENU(ID_M_HE_HEADERS_EXPAND_ALL,      header_editor_frame_c::on_headers_expand_all)
  EVT_MENU(ID_M_HE_HEADERS_COLLAPSE_ALL,    header_editor_frame_c::on_headers_collapse_all)
  EVT_MENU(ID_M_HE_HEADERS_VALIDATE,        header_editor_frame_c::on_headers_validate)
  EVT_MENU(ID_M_HE_HELP_HELP,               header_editor_frame_c::on_help_help)
  EVT_CLOSE(header_editor_frame_c::on_close_window)
END_EVENT_TABLE();
