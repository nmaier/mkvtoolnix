/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: unsigned integer value page class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __HE_UNSIGNED_INTEGER_VALUE_PAGE_H
#define __HE_UNSIGNED_INTEGER_VALUE_PAGE_H

#include "os.h"

#include "he_value_page.h"

class he_unsigned_integer_value_page_c: public he_value_page_c {
public:
  wxTextCtrl *m_tc_text;
  uint64_t m_original_value;

public:
  he_unsigned_integer_value_page_c(wxTreebook *parent, EbmlMaster *master, const EbmlId &id, const wxString &title, const wxString &description);
  virtual ~he_unsigned_integer_value_page_c();

  virtual wxControl *create_input_control();
  virtual wxString get_original_value_as_string();
  virtual wxString get_current_value_as_string();
  virtual bool validate_value();
  virtual void reset_value();
};

#endif // __HE_UNSIGNED_INTEGER_VALUE_PAGE_H
