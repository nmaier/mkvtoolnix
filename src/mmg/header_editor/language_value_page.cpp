/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: unsigned integer value page class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <wx/wx.h>

#include <ebml/EbmlString.h>

#include "mmg/header_editor/language_value_page.h"
#include "mmg/mmg.h"
#include "common/wx.h"

using namespace libebml;

he_language_value_page_c::he_language_value_page_c(header_editor_frame_c *parent,
                                                   he_page_base_c *toplevel_page,
                                                   EbmlMaster *master,
                                                   const EbmlCallbacks &callbacks,
                                                   const wxString &title,
                                                   const wxString &description)
  : he_value_page_c(parent, toplevel_page, master, callbacks, vt_string, title, description)
  , m_cb_language(NULL)
{
}

he_language_value_page_c::~he_language_value_page_c() {
}

wxControl *
he_language_value_page_c::create_input_control() {
  if (NULL != m_element)
    m_original_value = wxU(std::string(*static_cast<EbmlString *>(m_element)).c_str());

  m_cb_language = new wxMTX_COMBOBOX_TYPE(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, NULL, wxCB_DROPDOWN | wxCB_READONLY);

  int i;
  for (i = 0; i < sorted_iso_codes.Count(); i++)
    m_cb_language->Append(sorted_iso_codes[i]);

  reset_value();

  return m_cb_language;
}

wxString
he_language_value_page_c::get_original_value_as_string() {
  return m_original_value;
}

wxString
he_language_value_page_c::get_current_value_as_string() {
  return extract_language_code(m_cb_language->GetValue());
}

void
he_language_value_page_c::reset_value() {
  int i;
  for (i = 0; sorted_iso_codes.size() > i; ++i)
    if (extract_language_code(sorted_iso_codes[i]) == m_original_value) {
      set_combobox_selection(m_cb_language, sorted_iso_codes[i]);
      return;
    }

  set_combobox_selection(m_cb_language, sorted_iso_codes[0]);
}

bool
he_language_value_page_c::validate_value() {
  wxString selected_language_code = extract_language_code(m_cb_language->GetValue());
  return !selected_language_code.IsEmpty() && (selected_language_code != wxT("---"));
}

void
he_language_value_page_c::copy_value_to_element() {
  *static_cast<EbmlString *>(m_element) = std::string(wxMB(get_current_value_as_string()));
}
