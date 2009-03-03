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
  , m_element(find_ebml_element_by_id(m_master, m_id))
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
