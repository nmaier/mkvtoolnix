/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: unsigned integer value page class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/textctrl.h>

#include <ebml/EbmlUInteger.h>

#include "mmg/header_editor/bool_value_page.h"
#include "common/wx.h"

using namespace libebml;

he_bool_value_page_c::he_bool_value_page_c(header_editor_frame_c *parent,
                                           he_page_base_c *toplevel_page,
                                           EbmlMaster *master,
                                           const EbmlCallbacks &callbacks,
                                           const translatable_string_c &title,
                                           const translatable_string_c &description)
  : he_value_page_c(parent, toplevel_page, master, callbacks, vt_bool, title, description)
  , m_cb_bool(nullptr)
  , m_original_value(false)
{
}

he_bool_value_page_c::~he_bool_value_page_c() {
}

void
he_bool_value_page_c::translate_ui() {
  m_values.clear();
  m_values.Add(Z("no"));
  m_values.Add(Z("yes"));

  if (m_cb_bool) {
    size_t i;
    int selection = m_cb_bool->GetSelection();
    for (i = 0; m_values.size() > i; ++i)
      m_cb_bool->SetString(i, m_values[i]);
    m_cb_bool->SetSelection(selection);
  }

  he_value_page_c::translate_ui();
}

wxControl *
he_bool_value_page_c::create_input_control() {
  if (m_element)
    m_original_value = uint64(*static_cast<EbmlUInteger *>(m_element)) > 0;

  if (m_values.empty())
    translate_ui();

  m_cb_bool = new wxMTX_COMBOBOX_TYPE(this, wxID_ANY, m_values[m_original_value ? 1 : 0], wxDefaultPosition, wxDefaultSize, m_values, wxCB_DROPDOWN | wxCB_READONLY);

  return m_cb_bool;
}

wxString
he_bool_value_page_c::get_original_value_as_string() {
  if (m_element)
    return m_values[m_original_value ? 1 : 0];

  return m_values[0];
}

wxString
he_bool_value_page_c::get_current_value_as_string() {
  return m_cb_bool->GetValue();
}

void
he_bool_value_page_c::reset_value() {
  m_cb_bool->SetValue(m_values[m_original_value ? 1 : 0]);
}

bool
he_bool_value_page_c::validate_value() {
  return true;
}

void
he_bool_value_page_c::copy_value_to_element() {
  *static_cast<EbmlUInteger *>(m_element) = m_cb_bool->GetValue() == m_values[1];
}
