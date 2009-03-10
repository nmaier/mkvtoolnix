/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: value page base class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <wx/statline.h>

#include "commonebml.h"
#include "he_value_page.h"
#include "wxcommon.h"

he_value_page_c::he_value_page_c(wxTreebook *parent,
                                 he_page_base_c *toplevel_page,
                                 EbmlMaster *master,
                                 const EbmlCallbacks &callbacks,
                                 const value_type_e value_type,
                                 const wxString &title,
                                 const wxString &description)
  : he_page_base_c(parent)
  , m_master(master)
  , m_callbacks(callbacks)
  , m_title(title)
  , m_description(description)
  , m_value_type(value_type)
  , m_present(false)
  , m_cb_add_or_remove(NULL)
  , m_input(NULL)
  , m_b_reset(NULL)
  , m_element(find_ebml_element_by_id(m_master, m_callbacks.GlobalId))
  , m_toplevel_page(toplevel_page)
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

  wxFlexGridSizer *siz_fg = new wxFlexGridSizer(2, 5, 5);
  siz_fg->AddGrowableCol(1, 1);

  siz_fg->AddSpacer(5);
  siz_fg->AddSpacer(5);

  wxString type
    = vt_string           == m_value_type ? Z("String")
    : vt_unsigned_integer == m_value_type ? Z("Unsigned integer")
    : vt_signed_integer   == m_value_type ? Z("Signed integer")
    : vt_float            == m_value_type ? Z("Floating point number")
    : vt_binary           == m_value_type ? Z("Binary (displayed as hex numbers)")
    : vt_bool             == m_value_type ? Z("Boolean (yes/no, on/off etc)")
    :                                       Z("unknown");

  siz_fg->Add(new wxStaticText(this, wxID_ANY, Z("Type:")), 0, wxALIGN_TOP,          0);
  siz_fg->Add(new wxStaticText(this, wxID_ANY, type),       0, wxALIGN_TOP | wxGROW, 0);

  if (!m_description.IsEmpty()) {
    siz_fg->Add(new wxStaticText(this, wxID_ANY, Z("Description:")), 0, wxALIGN_TOP,          0);
    siz_fg->Add(new wxStaticText(this, wxID_ANY, m_description),     0, wxALIGN_TOP | wxGROW, 0);
  }

  wxString value_label;
  if (NULL == m_element) {
    m_present          = false;
    m_cb_add_or_remove = new wxCheckBox(this, ID_HE_CB_ADD_OR_REMOVE, Z("This element is not currently present in the file. Add the element to the file"));
    value_label        = Z("New value:");

  } else {
    m_present          = true;
    m_cb_add_or_remove = new wxCheckBox(this, ID_HE_CB_ADD_OR_REMOVE, Z("This element is currently present in the file. Remove the element from the file"));
    value_label        = Z("Current value:");
  }

  siz_fg->Add(new wxStaticText(this, wxID_ANY, Z("Status:")), 0, wxALIGN_TOP, 0);
  siz_fg->Add(m_cb_add_or_remove, 1, wxALIGN_TOP | wxGROW, 0);

  if (m_present) {
    siz_fg->Add(new wxStaticText(this, wxID_ANY, Z("Original value:")),           0, wxALIGN_TOP,          0);
    siz_fg->Add(new wxStaticText(this, wxID_ANY, get_original_value_as_string()), 1, wxALIGN_TOP | wxGROW, 0);
  }

  m_input = create_input_control();
  m_input->Enable(m_present);

  siz_fg->Add(new wxStaticText(this, wxID_ANY, value_label), 0, wxALIGN_CENTER_VERTICAL,          0);
  siz_fg->Add(m_input,                                       1, wxALIGN_CENTER_VERTICAL | wxGROW, 0);

  siz->Add(siz_fg, 0, wxGROW | wxLEFT | wxRIGHT, 5);

  siz->AddStretchSpacer();

  wxBoxSizer *siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->AddStretchSpacer();
  m_b_reset = new wxButton(this, ID_HE_B_RESET, Z("&Reset"));
  siz_line->Add(m_b_reset, 0, wxGROW, 0);

  siz->Add(siz_line, 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz->AddSpacer(5);

  SetSizer(siz);

  m_tree->AddSubPage(this, m_title);

  m_toplevel_page->m_children.push_back(this);
}

void
he_value_page_c::on_reset_clicked(wxCommandEvent &evt) {
  reset_value();
  m_cb_add_or_remove->SetValue(false);
  m_input->Enable(m_present);
}

void
he_value_page_c::on_add_or_remove_checked(wxCommandEvent &evt) {
  m_input->Enable(   (!m_present &&  evt.IsChecked())
                  || ( m_present && !evt.IsChecked()));
}

bool
he_value_page_c::has_this_been_modified() {
  return m_cb_add_or_remove->IsChecked() || (get_current_value_as_string() != get_original_value_as_string());
}

bool
he_value_page_c::validate_this() {
  if (!m_input->IsEnabled())
    return true;
  return validate_value();
}

void
he_value_page_c::modify_this() {
  if (!has_this_been_modified())
    return;

  if (m_present && m_cb_add_or_remove->IsChecked()) {
    int i;
    for (i = 0; m_master->ListSize() > i; ++i) {
      if ((*m_master)[i]->Generic().GlobalId != m_callbacks.GlobalId)
        continue;

      EbmlElement *e = (*m_master)[i];
      delete e;

      m_master->Remove(i);

      break;
    }

    return;
  }

  if (!m_present) {
    m_element = &m_callbacks.Create();
    m_master->PushElement(*m_element);
  }

  copy_value_to_element();
}

IMPLEMENT_CLASS(he_value_page_c, he_page_base_c);
BEGIN_EVENT_TABLE(he_value_page_c, he_page_base_c)
  EVT_BUTTON(ID_HE_B_RESET,            he_value_page_c::on_reset_clicked)
  EVT_CHECKBOX(ID_HE_CB_ADD_OR_REMOVE, he_value_page_c::on_add_or_remove_checked)
END_EVENT_TABLE();
