/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: bit value page class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __HE_BIT_VALUE_PAGE_H
#define __HE_BIT_VALUE_PAGE_H

#include "common/common_pch.h"

#include <wx/textctrl.h>

#include "common/bitvalue.h"
#include "mmg/header_editor/value_page.h"

class he_bit_value_page_c: public he_value_page_c {
public:
  wxTextCtrl *m_tc_text;
  bitvalue_c m_original_value;
  int m_bit_length;

public:
  he_bit_value_page_c(header_editor_frame_c *parent, he_page_base_c *toplevel_page, EbmlMaster *master, const EbmlCallbacks &callbacks,
                      const translatable_string_c &title, const translatable_string_c &description, int bit_length);
  virtual ~he_bit_value_page_c();

  virtual wxControl *create_input_control();
  virtual wxString get_original_value_as_string();
  virtual wxString get_current_value_as_string();
  virtual void reset_value();
  virtual bool validate_value();
  virtual void copy_value_to_element();
};

#endif // __HE_BIT_VALUE_PAGE_H
