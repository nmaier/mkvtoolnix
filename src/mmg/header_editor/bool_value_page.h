/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: unsigned integer value page class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __HE_BOOL_VALUE_PAGE_H
#define __HE_BOOL_VALUE_PAGE_H

#include "common/common_pch.h"

#include <wx/arrstr.h>

#include "mmg/header_editor/value_page.h"
#include "common/wx.h"

class he_bool_value_page_c: public he_value_page_c {
public:
  wxMTX_COMBOBOX_TYPE *m_cb_bool;
  bool m_original_value;
  wxArrayString m_values;

public:
  he_bool_value_page_c(header_editor_frame_c *parent, he_page_base_c *toplevel_page, EbmlMaster *master, const EbmlCallbacks &callbacks,
                       const translatable_string_c &title, const translatable_string_c &description);
  virtual ~he_bool_value_page_c();

  virtual wxControl *create_input_control();
  virtual wxString get_original_value_as_string();
  virtual wxString get_current_value_as_string();
  virtual bool validate_value();
  virtual void reset_value();
  virtual void copy_value_to_element();
  virtual void translate_ui();
};

#endif // __HE_BOOL_VALUE_PAGE_H
