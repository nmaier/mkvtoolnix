/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: value page base class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/statline.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include "common/ebml.h"
#include "mmg/header_editor/value_page.h"
#include "common/wx.h"

he_value_page_c::he_value_page_c(header_editor_frame_c *parent,
                                 he_page_base_c *toplevel_page,
                                 EbmlMaster *master,
                                 const EbmlCallbacks &callbacks,
                                 const value_type_e value_type,
                                 const translatable_string_c &title,
                                 const translatable_string_c &description)
  : he_page_base_c(parent, title)
  , m_master(master)
  , m_callbacks(callbacks)
  , m_sub_master_callbacks(nullptr)
  , m_description(description)
  , m_value_type(value_type)
  , m_present(false)
  , m_cb_add_or_remove(nullptr)
  , m_input(nullptr)
  , m_b_reset(nullptr)
  , m_element(find_ebml_element_by_id(m_master, m_callbacks.GlobalId))
  , m_toplevel_page(toplevel_page)
{
}

he_value_page_c::~he_value_page_c() {
}

void
he_value_page_c::init() {
  m_cb_add_or_remove = new wxCheckBox(this, ID_HE_CB_ADD_OR_REMOVE, wxEmptyString);
  m_input            = create_input_control();
  m_b_reset          = new wxButton(this, ID_HE_B_RESET);

  wxBoxSizer *siz = new wxBoxSizer(wxVERTICAL);

  siz->AddSpacer(5);
  siz->Add(new wxStaticText(this, wxID_ANY, wxU(m_title.get_translated())), 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz->AddSpacer(5);
  siz->Add(new wxStaticLine(this),                                          0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz->AddSpacer(5);

  wxFlexGridSizer *siz_fg = new wxFlexGridSizer(2, 5, 5);
  siz_fg->AddGrowableCol(1, 1);

  siz_fg->AddSpacer(5);
  siz_fg->AddSpacer(5);

  m_st_type_label = new wxStaticText(this, wxID_ANY, wxEmptyString);
  m_st_type       = new wxStaticText(this, wxID_ANY, wxEmptyString);
  siz_fg->Add(m_st_type_label, 0, wxALIGN_TOP,          0);
  siz_fg->Add(m_st_type,       0, wxALIGN_TOP | wxGROW, 0);

  if (!m_description.get_untranslated().empty()) {
    m_st_description_label = new wxStaticText(this, wxID_ANY, wxEmptyString);
    m_st_description       = new wxStaticText(this, wxID_ANY, wxEmptyString);
    siz_fg->Add(m_st_description_label, 0, wxALIGN_TOP,          0);
    siz_fg->Add(m_st_description,       0, wxALIGN_TOP | wxGROW, 0);
  }

  m_st_add_or_remove = new wxStaticText(this, wxID_ANY, wxEmptyString);

  if (nullptr == m_element)
    m_present        = false;

  else {
    m_present        = true;

    const EbmlSemantic *semantic = find_ebml_semantic(KaxSegment::ClassInfos, m_callbacks.GlobalId);
    if ((nullptr != semantic) && semantic->Mandatory)
      m_cb_add_or_remove->Disable();
  }

  m_st_status = new wxStaticText(this, wxID_ANY, wxEmptyString);
  siz_fg->Add(m_st_status,        0, wxALIGN_TOP, 0);
  siz_fg->Add(m_st_add_or_remove, 1, wxALIGN_TOP | wxGROW, 0);
  siz_fg->AddSpacer(0);
  siz_fg->Add(m_cb_add_or_remove, 1, wxALIGN_TOP | wxGROW, 0);

  if (m_present) {
    m_st_original_value = new wxStaticText(this, wxID_ANY, wxEmptyString);
    siz_fg->Add(m_st_original_value,                                              0, wxALIGN_TOP,          0);
    siz_fg->Add(new wxStaticText(this, wxID_ANY, get_original_value_as_string()), 1, wxALIGN_TOP | wxGROW, 0);
  }

  m_input->Enable(m_present);

  m_st_value_label = new wxStaticText(this, wxID_ANY, wxEmptyString);
  siz_fg->Add(m_st_value_label, 0, wxALIGN_CENTER_VERTICAL,          0);
  siz_fg->Add(m_input,          1, wxALIGN_CENTER_VERTICAL | wxGROW, 0);

  siz->Add(siz_fg, 0, wxGROW | wxLEFT | wxRIGHT, 5);

  siz->AddStretchSpacer();

  wxBoxSizer *siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->AddStretchSpacer();
  siz_line->Add(m_b_reset, 0, wxGROW, 0);

  siz->Add(siz_line, 0, wxGROW | wxLEFT | wxRIGHT, 5);
  siz->AddSpacer(5);

  SetSizer(siz);

  if (nullptr == m_toplevel_page)
    m_parent->append_page(this);
  else
    m_parent->append_sub_page(this, m_toplevel_page->m_page_id);

  m_toplevel_page->m_children.push_back(this);

  translate_ui();
}

void
he_value_page_c::translate_ui() {
  if (nullptr == m_b_reset)
    return;

  m_b_reset->SetLabel(Z("&Reset"));

  wxString type
    = vt_ascii_string     == m_value_type ? Z("ASCII string (no special chars like\nUmlaute etc)")
    : vt_string           == m_value_type ? Z("String")
    : vt_unsigned_integer == m_value_type ? Z("Unsigned integer")
    : vt_signed_integer   == m_value_type ? Z("Signed integer")
    : vt_float            == m_value_type ? Z("Floating point number")
    : vt_binary           == m_value_type ? Z("Binary (displayed as hex numbers)")
    : vt_bool             == m_value_type ? Z("Boolean (yes/no, on/off etc)")
    :                                       Z("unknown");

  m_st_type_label->SetLabel(Z("Type:"));
  m_st_type->SetLabel(type);
  if (!m_description.get_translated().empty()) {
    m_st_description_label->SetLabel(Z("Description:"));
    m_st_description->SetLabel(wxU(m_description.get_translated()));
  }

  if (nullptr == m_element) {
    m_st_add_or_remove->SetLabel(Z("This element is not currently present in the file.\nYou can let the header editor add the element\nto the file."));
    m_st_value_label->SetLabel(Z("New value:"));
    m_cb_add_or_remove->SetLabel(Z("Add element"));

  } else {
    m_st_add_or_remove->SetLabel(Z("This element is currently present in the file.\nYou can let the header editor remove the element\nfrom the file."));
    m_st_value_label->SetLabel(Z("Current value:"));
    m_cb_add_or_remove->SetLabel(Z("Remove element"));

    const EbmlSemantic *semantic = find_ebml_semantic(KaxSegment::ClassInfos, m_callbacks.GlobalId);
    if ((nullptr != semantic) && semantic->Mandatory)
      m_st_add_or_remove->SetLabel(Z("This element is currently present in the file.\nIt cannot be removed because it is a\nmandatory header field."));
  }

  m_st_status->SetLabel(Z("Status:"));
  if (m_present)
    m_st_original_value->SetLabel(Z("Original value:"));
}

void
he_value_page_c::on_reset_clicked(wxCommandEvent &) {
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

  EbmlMaster *actual_master = nullptr;
  if (nullptr != m_sub_master_callbacks) {
    actual_master = static_cast<EbmlMaster *>(find_ebml_element_by_id(m_master, m_sub_master_callbacks->GlobalId));

    if (nullptr == actual_master) {
      actual_master = static_cast<EbmlMaster *>(&m_sub_master_callbacks->Create());
      m_master->PushElement(*actual_master);
    }
  }

  if (nullptr == actual_master)
    actual_master = m_master;

  if (m_present && m_cb_add_or_remove->IsChecked()) {
    size_t i;
    for (i = 0; actual_master->ListSize() > i; ++i) {
      if ((*actual_master)[i]->Generic().GlobalId != m_callbacks.GlobalId)
        continue;

      EbmlElement *e = (*actual_master)[i];
      delete e;

      actual_master->Remove(i);

      break;
    }

    return;
  }

  if (!m_present) {
    m_element = &m_callbacks.Create();
    actual_master->PushElement(*m_element);
  }

  copy_value_to_element();
}

void
he_value_page_c::set_sub_master_callbacks(const EbmlCallbacks &callbacks) {
  m_sub_master_callbacks = &callbacks;

  EbmlMaster *sub_master = static_cast<EbmlMaster *>(find_ebml_element_by_id(m_master, m_sub_master_callbacks->GlobalId));
  if (nullptr != sub_master)
    m_element = find_ebml_element_by_id(sub_master, m_callbacks.GlobalId);
}

IMPLEMENT_CLASS(he_value_page_c, he_page_base_c);
BEGIN_EVENT_TABLE(he_value_page_c, he_page_base_c)
  EVT_BUTTON(ID_HE_B_RESET,            he_value_page_c::on_reset_clicked)
  EVT_CHECKBOX(ID_HE_CB_ADD_OR_REMOVE, he_value_page_c::on_add_or_remove_checked)
END_EVENT_TABLE();
