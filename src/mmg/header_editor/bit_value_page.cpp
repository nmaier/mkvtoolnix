/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: bit value page class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/wx.h>

#include <ebml/EbmlBinary.h>

#include "mmg/header_editor/bit_value_page.h"
#include "common/wx.h"

using namespace libebml;

he_bit_value_page_c::he_bit_value_page_c(header_editor_frame_c *parent,
                                         he_page_base_c *toplevel_page,
                                         EbmlMaster *master,
                                         const EbmlCallbacks &callbacks,
                                         const translatable_string_c &title,
                                         const translatable_string_c &description,
                                         int bit_length)
  : he_value_page_c(parent, toplevel_page, master, callbacks, vt_binary, title, description)
  , m_tc_text(nullptr)
  , m_original_value(128)
  , m_bit_length(bit_length)
{
}

he_bit_value_page_c::~he_bit_value_page_c() {
}

wxControl *
he_bit_value_page_c::create_input_control() {
  if (nullptr != m_element)
    m_original_value = bitvalue_c(*static_cast<EbmlBinary *>(m_element));

  m_tc_text = new wxTextCtrl(this, wxID_ANY, get_original_value_as_string());
  return m_tc_text;
}

wxString
he_bit_value_page_c::get_original_value_as_string() {
  wxString value;
  unsigned char *data = m_original_value.data();

  if (nullptr != data) {
    int i, num_bytes = m_original_value.size() / 8;
    for (i = 0; i < num_bytes; ++i)
      value += wxString::Format(wxT("%02x"), data[i]);
  }

  return value;
}

wxString
he_bit_value_page_c::get_current_value_as_string() {
  return m_tc_text->GetValue();
}

void
he_bit_value_page_c::reset_value() {
  m_tc_text->SetValue(get_original_value_as_string());
}

bool
he_bit_value_page_c::validate_value() {
  try {
    bitvalue_c bit_value(wxMB(m_tc_text->GetValue()), m_bit_length);
  } catch (...) {
    return false;
  }

  return true;
}

void
he_bit_value_page_c::copy_value_to_element() {
  bitvalue_c bit_value(wxMB(m_tc_text->GetValue()), m_bit_length);
  static_cast<EbmlBinary *>(m_element)->CopyBuffer(bit_value.data(), m_bit_length / 8);
}
