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

#include <ebml/EbmlString.h>

#include "he_ascii_string_value_page.h"
#include "wxcommon.h"

using namespace libebml;

he_ascii_string_value_page_c::he_ascii_string_value_page_c(header_editor_frame_c *parent,
                                                           he_page_base_c *toplevel_page,
                                                           EbmlMaster *master,
                                                           const EbmlCallbacks &callbacks,
                                                           const wxString &title,
                                                           const wxString &description)
  : he_value_page_c(parent, toplevel_page, master, callbacks, vt_ascii_string, title, description)
  , m_tc_text(NULL)
{
}

he_ascii_string_value_page_c::~he_ascii_string_value_page_c() {
}

wxControl *
he_ascii_string_value_page_c::create_input_control() {
  if (NULL != m_element)
    m_original_value = wxU(std::string(*static_cast<EbmlString *>(m_element)).c_str());

  static const wxString s_valid_chars = wxT(" !\"#$%&'()*+,-./0123456789:;<=>?@ABCDEFGHIJKLMNOPQRSTUVWXYZ[\\]^_`abcdefghijklmnopqrstuvwxyz{|}~");

  wxArrayString includes;
  int i;
  for (i = 0; s_valid_chars.Length() > i; ++i)
    includes.Add(s_valid_chars.Mid(i, 1));

  wxTextValidator validator(wxFILTER_INCLUDE_CHAR_LIST);
  validator.SetIncludes(includes);

  m_tc_text = new wxTextCtrl(this, wxID_ANY, m_original_value);
  m_tc_text->SetValidator(validator);

  return m_tc_text;
}

wxString
he_ascii_string_value_page_c::get_original_value_as_string() {
  return m_original_value;
}

wxString
he_ascii_string_value_page_c::get_current_value_as_string() {
  return m_tc_text->GetValue();
}

void
he_ascii_string_value_page_c::reset_value() {
  m_tc_text->SetValue(m_original_value);
}

bool
he_ascii_string_value_page_c::validate_value() {
  return true;
}

void
he_ascii_string_value_page_c::copy_value_to_element() {
  *static_cast<EbmlString *>(m_element) = std::string(wxMB(m_tc_text->GetValue()));
}
