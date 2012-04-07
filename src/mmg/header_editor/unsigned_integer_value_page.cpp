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
#include <wx/valtext.h>

#include <ebml/EbmlUInteger.h>

#include "common/strings/parsing.h"
#include "common/wx.h"
#include "mmg/header_editor/unsigned_integer_value_page.h"

using namespace libebml;

he_unsigned_integer_value_page_c::he_unsigned_integer_value_page_c(header_editor_frame_c *parent,
                                                                   he_page_base_c *toplevel_page,
                                                                   EbmlMaster *master,
                                                                   const EbmlCallbacks &callbacks,
                                                                   const translatable_string_c &title,
                                                                   const translatable_string_c &description)
  : he_value_page_c(parent, toplevel_page, master, callbacks, vt_unsigned_integer, title, description)
  , m_tc_text(nullptr)
  , m_original_value(0)
{
}

he_unsigned_integer_value_page_c::~he_unsigned_integer_value_page_c() {
}

wxControl *
he_unsigned_integer_value_page_c::create_input_control() {
  if (nullptr != m_element)
    m_original_value = uint64(*static_cast<EbmlUInteger *>(m_element));

  m_tc_text = new wxTextCtrl(this, wxID_ANY, get_original_value_as_string());
  m_tc_text->SetValidator(wxTextValidator(wxFILTER_NUMERIC));

  return m_tc_text;
}

wxString
he_unsigned_integer_value_page_c::get_original_value_as_string() {
  if (nullptr != m_element)
    return wxString::Format(wxU("%") + wxU(wxLongLongFmtSpec) + wxU("u"), m_original_value);

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
  uint64_t value;
  return parse_number(wxMB(m_tc_text->GetValue()), value);
}

void
he_unsigned_integer_value_page_c::copy_value_to_element() {
  uint64_t value;
  parse_number(wxMB(m_tc_text->GetValue()), value);
  *static_cast<EbmlUInteger *>(m_element) = value;
}
