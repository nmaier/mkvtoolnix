/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: unsigned integer value page class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <ebml/EbmlUInteger.h>

#include "he_unsigned_integer_value_page.h"
#include "wxcommon.h"

using namespace libebml;

he_unsigned_integer_value_page_c::he_unsigned_integer_value_page_c(wxTreebook *parent,
                                                                   EbmlMaster *master,
                                                                   const EbmlCallbacks &callbacks,
                                                                   const wxString &title,
                                                                   const wxString &description)
  : he_value_page_c(parent, master, callbacks, vt_string, title, description)
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
  m_tc_text->SetValidator(wxTextValidator(wxFILTER_NUMERIC));

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
  uint64_t dummy;
  return parse_uint(wxMB(m_tc_text->GetValue()), dummy);
}
