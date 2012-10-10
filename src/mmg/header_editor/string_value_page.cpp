/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: string value page class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/textctrl.h>

#include <ebml/EbmlUnicodeString.h>

#include "common/wx.h"
#include "mmg/header_editor/string_value_page.h"

using namespace libebml;

he_string_value_page_c::he_string_value_page_c(header_editor_frame_c *parent,
                                               he_page_base_c *toplevel_page,
                                               EbmlMaster *master,
                                               const EbmlCallbacks &callbacks,
                                               const translatable_string_c &title,
                                               const translatable_string_c &description)
  : he_value_page_c(parent, toplevel_page, master, callbacks, vt_string, title, description)
  , m_tc_text(nullptr)
{
}

he_string_value_page_c::~he_string_value_page_c() {
}

wxControl *
he_string_value_page_c::create_input_control() {
  if (m_element)
    m_original_value = static_cast<EbmlUnicodeString *>(m_element)->GetValue();

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

void
he_string_value_page_c::copy_value_to_element() {
  static_cast<EbmlUnicodeString *>(m_element)->SetValue(to_wide(m_tc_text->GetValue()));
}
