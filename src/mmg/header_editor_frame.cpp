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
#include "mmg.h"
#include "wxcommon.h"

static EbmlElement *
find_by_id(EbmlMaster *master,
           const EbmlId &id) {
  wxLogMessage(wxT("Looking for 0x%08u"), id.Value);
  int i;
  for (i = 0; master->ListSize() > i; ++i) {
    wxLogMessage(wxT("  ? 0x%08u"), (*master)[i]->Generic().GlobalId.Value);
    if ((*master)[i]->Generic().GlobalId == id)
      return (*master)[i];
  }

  return NULL;
}

// ------------------------------------------------------------

he_page_base_c::he_page_base_c(wxTreebook *parent)
  : wxPanel(parent)
  , m_tree(parent)
  , m_page_id(m_tree->GetTreeCtrl()->GetCount())
  , m_storage(NULL)
{
}

he_page_base_c::~he_page_base_c() {
  delete m_storage;
}

// ------------------------------------------------------------

class he_value_page_c: public he_page_base_c {
public:
  EbmlMaster *m_master;
  EbmlId m_id;

  wxString m_title, m_description, m_original_value;

  bool m_present;

  wxCheckBox *m_cb_add_or_remove;
  wxTextCtrl *m_te_text;
  wxButton *m_b_reset;

public:
  he_value_page_c(wxTreebook *parent, EbmlMaster *master, const EbmlId &id, const wxString &title, const wxString &description);
  virtual ~he_value_page_c();

  virtual void init();
};

he_value_page_c::he_value_page_c(wxTreebook *parent,
                                 EbmlMaster *master,
                                 const EbmlId &id,
                                 const wxString &title,
                                 const wxString &description)
  : he_page_base_c(parent)
  , m_master(master)
  , m_id(id)
  , m_title(title)
  , m_description(description)
  , m_present(false)
  , m_cb_add_or_remove(NULL)
  , m_te_text(NULL)
  , m_b_reset(NULL)
{
}

void
he_value_page_c::init() {
  wxBoxSizer *siz = new wxBoxSizer(wxVERTICAL);

  siz->AddSpacer(5);
  siz->Add(new wxStaticText(this, wxID_ANY, m_title), 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz->AddSpacer(5);
  siz->Add(new wxStaticLine(this),                    0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz->AddSpacer(5);

  if (!m_description.IsEmpty()) {
    siz->AddSpacer(10);
    siz->Add(new wxStaticText(this, wxID_ANY, m_description), 0, wxGROW | wxLEFT | wxRIGHT, 5);
    siz->AddSpacer(15);
  }

  wxBoxSizer *siz_line = new wxBoxSizer(wxHORIZONTAL);
  m_te_text = new wxTextCtrl(this, wxID_ANY);

  EbmlUnicodeString *element = static_cast<EbmlUnicodeString *>(find_by_id(m_master, m_id));
  if (NULL == element) {
    m_present = false;

    siz->Add(new wxStaticText(this, wxID_ANY, Z("This element is not currently present in the file.")), 0, wxGROW | wxLEFT | wxRIGHT, 5);
    siz->AddSpacer(5);
    m_cb_add_or_remove = new wxCheckBox(this, wxID_ANY /* FIXME */, Z("Add this element to the file"));
    siz->Add(m_cb_add_or_remove, 0, wxGROW | wxLEFT | wxRIGHT, 5);
    siz->AddSpacer(15);

    siz_line->Add(new wxStaticText(this, wxID_ANY, Z("New value:")), 0, wxALIGN_CENTER_VERTICAL          | wxLEFT | wxRIGHT, 5);
    siz_line->Add(m_te_text,                                         0, wxALIGN_CENTER_VERTICAL | wxGROW | wxLEFT | wxRIGHT, 5);

  } else {
    m_present        = true;
    m_original_value = ((const UTFstring)(*element)).c_str();

    siz->Add(new wxStaticText(this, wxID_ANY, Z("This element is currently present in the file.")), 0, wxGROW | wxLEFT | wxRIGHT, 5);
    siz->AddSpacer(5);
    m_cb_add_or_remove = new wxCheckBox(this, wxID_ANY /* FIXME */, Z("Remove this element from the file"));
    siz->Add(m_cb_add_or_remove, 0, wxGROW | wxLEFT | wxRIGHT, 5);
    siz->AddSpacer(15);

    siz_line->Add(new wxStaticText(this, wxID_ANY, Z("Current value:")), 0, wxALIGN_CENTER_VERTICAL          | wxLEFT | wxRIGHT, 5);
    siz_line->Add(m_te_text,                                             0, wxALIGN_CENTER_VERTICAL | wxGROW | wxLEFT | wxRIGHT, 5);
  }

  siz->Add(siz_line, 0, wxGROW | wxLEFT | wxRIGHT, 5);

  siz->AddStretchSpacer();

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->AddStretchSpacer();
  m_b_reset = new wxButton(this, wxID_ANY /* FIXME */, Z("&Reset"));
  siz_line->Add(m_b_reset, 0, wxGROW, 0);

  siz->Add(siz_line, 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz->AddSpacer(5);

  SetSizer(siz);

  m_tree->AddSubPage(this, m_title);
}

he_value_page_c::~he_value_page_c() {
}

// ------------------------------------------------------------

class he_empty_page_c: public he_page_base_c {
public:
  wxString m_title, m_content;

public:
  he_empty_page_c(wxTreebook *parent, const wxString &title, const wxString &content);
  virtual ~he_empty_page_c();
};

he_empty_page_c::he_empty_page_c(wxTreebook *parent,
                                 const wxString &title,
                                 const wxString &content)
  : he_page_base_c(parent)
  , m_title(title)
  , m_content(content)
{
  wxBoxSizer *siz = new wxBoxSizer(wxVERTICAL);

  siz->AddSpacer(5);
  siz->Add(new wxStaticText(this, wxID_ANY, m_title),   0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz->AddSpacer(5);
  siz->Add(new wxStaticLine(this),                      0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz->AddSpacer(5);
  siz->Add(new wxStaticText(this, wxID_ANY, m_content), 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz->AddStretchSpacer();

  SetSizer(siz);
}

he_empty_page_c::~he_empty_page_c() {
}

// ------------------------------------------------------------

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

  add_empty_page(NULL, Z("No file loaded"), Z("No file has been loaded yet. You can do that by selecting 'Open' from the 'File' menu."));

  m_file_menu = new wxMenu();
  m_file_menu->Append(ID_M_HE_FILE_OPEN, Z("&Open\tCtrl-O"), Z("Open an existing Matroska file"));
  m_file_menu->Append(ID_M_HE_FILE_SAVE, Z("&Save\tCtrl-S"), Z("Save the header values"));
  m_file_menu->AppendSeparator();
  m_file_menu->Append(ID_M_HE_FILE_QUIT, Z("&Quit\tCtrl-Q"), Z("Quit the header editor"));

  enable_file_save_menu_entry();

  wxMenu *help_menu = new wxMenu();
  help_menu->Append(ID_M_HE_HELP_HELP, Z("&Help\tF1"), Z("Display usage information"));

  wxMenuBar *menu_bar = new wxMenuBar();
  menu_bar->Append(m_file_menu, Z("&File"));
  menu_bar->Append(help_menu, Z("&Help"));
  SetMenuBar(menu_bar);
}

header_editor_frame_c::~header_editor_frame_c() {
  delete m_analyzer;
}

void
header_editor_frame_c::add_empty_page(void *some_ptr,
                                      const wxString &title,
                                      const wxString &text) {
  m_bs_panel->Hide(m_tb_tree);

  he_empty_page_c *page = new he_empty_page_c(m_tb_tree, title, text);

  m_tb_tree->AddPage(page, title);

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

  enable_file_save_menu_entry();

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

  child_page = new he_value_page_c(m_tb_tree, info, KaxTitle::ClassInfos.GlobalId, Z("Title"), Z("The title for the whole movie."));
  child_page->init();
  page->m_children.push_back(child_page);

  child_page = new he_value_page_c(m_tb_tree, info, KaxSegmentFilename::ClassInfos.GlobalId, Z("Segment filename"), Z("The file name for this segment."));
  child_page->init();
  page->m_children.push_back(child_page);

  child_page = new he_value_page_c(m_tb_tree, info, KaxPrevFilename::ClassInfos.GlobalId, Z("Previous filename"), Z("The previous segment's file name."));
  child_page->init();
  page->m_children.push_back(child_page);

  child_page = new he_value_page_c(m_tb_tree, info, KaxNextFilename::ClassInfos.GlobalId, Z("Next filename"), Z("The next segment's file name."));
  child_page->init();
  page->m_children.push_back(child_page);

  // child_page = new he_value_page_c(m_tb_tree, info, Kax::ClassInfos.GlobalId, Z(""), Z("."));
  // child_page->init();
  // page->m_children.push_back(child_page);

  // child_page = new he_value_page_c(m_tb_tree, info, Kax::ClassInfos.GlobalId, Z(""), Z("."));
  // child_page->init();
  // page->m_children.push_back(child_page);

  // child_page = new he_value_page_c(m_tb_tree, info, Kax::ClassInfos.GlobalId, Z(""), Z("."));
  // child_page->init();
  // page->m_children.push_back(child_page);

  // child_page = new he_value_page_c(m_tb_tree, info, Kax::ClassInfos.GlobalId, Z(""), Z("."));
  // child_page->init();
  // page->m_children.push_back(child_page);
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
  }

  delete e;
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
header_editor_frame_c::on_file_quit(wxCommandEvent &evt) {
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
  EVT_MENU(ID_M_HE_FILE_QUIT,               header_editor_frame_c::on_file_quit)
  EVT_CLOSE(header_editor_frame_c::on_close)
END_EVENT_TABLE();
