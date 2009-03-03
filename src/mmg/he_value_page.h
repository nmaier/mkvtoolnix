/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: value page base class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __HE_VALUE_PAGE_H
#define __HE_VALUE_PAGE_H

#include "os.h"

#include "header_editor_frame.h"

class he_value_page_c: public he_page_base_c {
  DECLARE_CLASS(he_value_page_c);
  DECLARE_EVENT_TABLE();
public:
  enum value_type_e {
    vt_string,
    vt_integer,
    vt_float,
  };

public:
  EbmlMaster *m_master;
  const EbmlCallbacks &m_callbacks;

  wxString m_title, m_description;

  value_type_e m_value_type;

  bool m_present;

  wxCheckBox *m_cb_add_or_remove;
  wxControl *m_input;
  wxButton *m_b_reset;

  EbmlElement *m_element;

public:
  he_value_page_c(wxTreebook *parent, EbmlMaster *master, const EbmlCallbacks &callbacks, const value_type_e value_type, const wxString &title, const wxString &description);
  virtual ~he_value_page_c();

  void init();
  void on_reset_clicked(wxCommandEvent &evt);
  void on_add_or_remove_checked(wxCommandEvent &evt);

  virtual bool has_been_modified();

  virtual wxControl *create_input_control() = 0;
  virtual wxString get_original_value_as_string() = 0;
  virtual wxString get_current_value_as_string() = 0;
  virtual void reset_value() = 0;
  virtual bool validate_value() = 0;

  virtual bool validate();
};

#endif // __HE_VALUE_PAGE_H
