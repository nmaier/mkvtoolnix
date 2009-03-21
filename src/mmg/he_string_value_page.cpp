/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: string value page class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <ebml/EbmlUnicodeString.h>

#include "he_string_value_page.h"

using namespace libebml;

he_string_value_page_c::he_string_value_page_c(wxTreebook *parent,
                                               he_page_base_c *toplevel_page,
                                               EbmlMaster *master,
                                               const EbmlCallbacks &callbacks,
                                               const wxString &title,
                                               const wxString &description)
  : he_value_page_c(parent, toplevel_page, master, callbacks, vt_string, title, description)
  , m_tc_text(NULL)
{
}

he_string_value_page_c::~he_string_value_page_c() {
}

wxControl *
he_string_value_page_c::create_input_control() {
  if (NULL != m_element)
    m_original_value = UTFstring(*static_cast<EbmlUnicodeString *>(m_element)).c_str();

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
  *static_cast<EbmlUnicodeString *>(m_element) = UTFstring(m_tc_text->GetValue().c_str());
}
