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
  int i;
  for (i = 0; master->ListSize() > i; ++i)
    if ((*master)[i]->Generic().GlobalId == id)
      return (*master)[i];

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
  DECLARE_CLASS(he_value_page_c);
  DECLARE_EVENT_TABLE();
public:
  enum value_type_e {
    vt_string,
    vt_integer,
    vt_float,
  };

public:
  EbmlMaster *m_master;
  EbmlId m_id;

  wxString m_title, m_description;

  value_type_e m_value_type;

  bool m_present;

  wxCheckBox *m_cb_add_or_remove;
  wxControl *m_input;
  wxButton *m_b_reset;

  EbmlElement *m_element;

public:
  he_value_page_c(wxTreebook *parent, EbmlMaster *master, const EbmlId &id, const value_type_e value_type, const wxString &title, const wxString &description);
  virtual ~he_value_page_c();

  void init();
  void on_reset_clicked(wxCommandEvent &evt);
  void on_add_or_remove_checked(wxCommandEvent &evt);

  virtual bool has_been_modified();

  virtual wxControl *create_input_control() = 0;
  virtual wxString get_original_value_as_string() = 0;
  virtual wxString get_current_value_as_string() = 0;
  virtual void reset_value() = 0;
  virtual bool validate_value() = 0;

  virtual bool validate();
};

he_value_page_c::he_value_page_c(wxTreebook *parent,
                                 EbmlMaster *master,
                                 const EbmlId &id,
                                 const value_type_e value_type,
                                 const wxString &title,
                                 const wxString &description)
  : he_page_base_c(parent)
  , m_master(master)
  , m_id(id)
  , m_title(title)
  , m_description(description)
  , m_value_type(value_type)
  , m_present(false)
  , m_cb_add_or_remove(NULL)
  , m_input(NULL)
  , m_b_reset(NULL)
  , m_element(find_by_id(m_master, m_id))
{
}

he_value_page_c::~he_value_page_c() {
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

  wxString value_label;
  if (NULL == m_element) {
    m_present = false;

    siz->Add(new wxStaticText(this, wxID_ANY, Z("This element is not currently present in the file.")), 0, wxGROW | wxLEFT | wxRIGHT, 5);
    siz->AddSpacer(5);
    m_cb_add_or_remove = new wxCheckBox(this, ID_HE_CB_ADD_OR_REMOVE, Z("Add this element to the file"));
    siz->Add(m_cb_add_or_remove, 0, wxGROW | wxLEFT | wxRIGHT, 5);
    siz->AddSpacer(15);

    value_label = Z("New value:");

  } else {
    m_present = true;

    siz->Add(new wxStaticText(this, wxID_ANY, Z("This element is currently present in the file.")), 0, wxGROW | wxLEFT | wxRIGHT, 5);
    siz->AddSpacer(5);
    m_cb_add_or_remove = new wxCheckBox(this, ID_HE_CB_ADD_OR_REMOVE, Z("Remove this element from the file"));
    siz->Add(m_cb_add_or_remove, 0, wxGROW | wxLEFT | wxRIGHT, 5);
    siz->AddSpacer(15);

    siz->Add(new wxStaticText(this, wxID_ANY, wxString::Format(Z("Original value: %s"), get_original_value_as_string().c_str())), 0, 0, 0);
    siz->AddSpacer(15);

    value_label = Z("Current value:");
  }

  m_input = create_input_control();
  m_input->Enable(NULL != m_element);

  wxBoxSizer *siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(new wxStaticText(this, wxID_ANY, value_label), 0, wxALIGN_CENTER_VERTICAL,          0);
  siz_line->AddSpacer(5);
  siz_line->Add(m_input,                                       1, wxALIGN_CENTER_VERTICAL | wxGROW, 0);

  siz->Add(siz_line, 0, wxGROW | wxLEFT | wxRIGHT, 5);

  siz->AddStretchSpacer();

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->AddStretchSpacer();
  m_b_reset = new wxButton(this, ID_HE_B_RESET, Z("&Reset"));
  siz_line->Add(m_b_reset, 0, wxGROW, 0);

  siz->Add(siz_line, 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz->AddSpacer(5);

  SetSizer(siz);

  m_tree->AddSubPage(this, m_title);
}

void
he_value_page_c::on_reset_clicked(wxCommandEvent &evt) {
  reset_value();
  m_cb_add_or_remove->SetValue(false);
  m_input->Enable(NULL != m_element);
}

void
he_value_page_c::on_add_or_remove_checked(wxCommandEvent &evt) {
  m_input->Enable(   ((NULL == m_element) &&  evt.IsChecked())
                  || ((NULL != m_element) && !evt.IsChecked()));
}

bool
he_value_page_c::has_been_modified() {
  return m_cb_add_or_remove->IsChecked() || (get_current_value_as_string() != get_original_value_as_string());
}

bool
he_value_page_c::validate() {
  if (!m_input->IsEnabled())
    return true;
  return validate_value();
}

IMPLEMENT_CLASS(he_value_page_c, he_page_base_c);
BEGIN_EVENT_TABLE(he_value_page_c, he_page_base_c)
  EVT_BUTTON(ID_HE_B_RESET,            he_value_page_c::on_reset_clicked)
  EVT_CHECKBOX(ID_HE_CB_ADD_OR_REMOVE, he_value_page_c::on_add_or_remove_checked)
END_EVENT_TABLE();

// ------------------------------------------------------------

class he_string_value_page_c: public he_value_page_c {
public:
  wxTextCtrl *m_tc_text;
  wxString m_original_value;

public:
  he_string_value_page_c(wxTreebook *parent, EbmlMaster *master, const EbmlId &id, const wxString &title, const wxString &description);
  virtual ~he_string_value_page_c();

  virtual wxControl *create_input_control();
  virtual wxString get_original_value_as_string();
  virtual wxString get_current_value_as_string();
  virtual void reset_value();
  virtual bool validate_value();
};

he_string_value_page_c::he_string_value_page_c(wxTreebook *parent,
                                               EbmlMaster *master,
                                               const EbmlId &id,
                                               const wxString &title,
                                               const wxString &description)
  : he_value_page_c(parent, master, id, vt_string, title, description)
  , m_tc_text(NULL)
{
  if (NULL != m_element)
    m_original_value = UTFstring(*static_cast<EbmlUnicodeString *>(m_element)).c_str();
}

he_string_value_page_c::~he_string_value_page_c() {
}

wxControl *
he_string_value_page_c::create_input_control() {
  m_tc_text = new wxTextCtrl(this, wxID_ANY, m_original_value);
  return m_tc_text;
}

wxString
he_string_value_page_c::get_original_value_as_string() {
  return m_original_value;
}

wxString
he_string_value_page_c::get_current_value_as_string() {
  return m_tc_text->GetValue();
}

void
he_string_value_page_c::reset_value() {
  m_tc_text->SetValue(m_original_value);
}

bool
he_string_value_page_c::validate_value() {
  return true;
}

// ------------------------------------------------------------

class he_unsigned_integer_value_page_c: public he_value_page_c {
public:
  wxTextCtrl *m_tc_text;
  uint64_t m_original_value;

public:
  he_unsigned_integer_value_page_c(wxTreebook *parent, EbmlMaster *master, const EbmlId &id, const wxString &title, const wxString &description);
  virtual ~he_unsigned_integer_value_page_c();

  virtual wxControl *create_input_control();
  virtual wxString get_original_value_as_string();
  virtual wxString get_current_value_as_string();
  virtual bool validate_value();
  virtual void reset_value();
};

he_unsigned_integer_value_page_c::he_unsigned_integer_value_page_c(wxTreebook *parent,
                                                                   EbmlMaster *master,
                                                                   const EbmlId &id,
                                                                   const wxString &title,
                                                                   const wxString &description)
  : he_value_page_c(parent, master, id, vt_string, title, description)
  , m_tc_text(NULL)
  , m_original_value(0)
{
  if (NULL != m_element)
    m_original_value = uint64(*static_cast<EbmlUInteger *>(m_element));
}

he_unsigned_integer_value_page_c::~he_unsigned_integer_value_page_c() {
}

wxControl *
he_unsigned_integer_value_page_c::create_input_control() {
  m_tc_text = new wxTextCtrl(this, wxID_ANY, get_original_value_as_string());
  return m_tc_text;
}

wxString
he_unsigned_integer_value_page_c::get_original_value_as_string() {
  if (NULL != m_element)
    return wxString::Format(_T("%") wxLongLongFmtSpec _T("u"), m_original_value);

  return wxEmptyString;
}

wxString
he_unsigned_integer_value_page_c::get_current_value_as_string() {
  return m_tc_text->GetValue();
}

void
he_unsigned_integer_value_page_c::reset_value() {
  m_tc_text->SetValue(get_original_value_as_string());
}

bool
he_unsigned_integer_value_page_c::validate_value() {
  return true;                  // FIXME
}

// ------------------------------------------------------------

class he_empty_page_c: public he_page_base_c {
public:
  wxString m_title, m_content;

public:
  he_empty_page_c(wxTreebook *parent, const wxString &title, const wxString &content);
  virtual ~he_empty_page_c();

  virtual bool has_been_modified();
  virtual bool validate();
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

bool
he_empty_page_c::has_been_modified() {
  return false;
}

bool
he_empty_page_c::validate() {
  return true;
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
