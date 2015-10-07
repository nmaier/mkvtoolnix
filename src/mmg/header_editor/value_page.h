/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header editor: value page base class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_HE_VALUE_PAGE_H
#define MTX_HE_VALUE_PAGE_H

#include "common/common_pch.h"

#include <wx/wx.h>

#include "mmg/header_editor/frame.h"

class he_value_page_c: public he_page_base_c {
  DECLARE_CLASS(he_value_page_c);
  DECLARE_EVENT_TABLE();
public:
  enum value_type_e {
    vt_ascii_string,
    vt_string,
    vt_unsigned_integer,
    vt_signed_integer,
    vt_float,
    vt_binary,
    vt_bool,
  };

public:
  EbmlMaster *m_master;
  const EbmlCallbacks &m_callbacks;
  const EbmlCallbacks *m_sub_master_callbacks;

  translatable_string_c m_description;

  value_type_e m_value_type;

  bool m_present;

  wxCheckBox *m_cb_add_or_remove;
  wxControl *m_input;
  wxButton *m_b_reset;
  wxStaticText *m_st_status, *m_st_description_label, *m_st_description, *m_st_original_value, *m_st_add_or_remove, *m_st_value_label, *m_st_type_label, *m_st_type;

  EbmlElement *m_element;

  he_page_base_c *m_toplevel_page;

public:
  he_value_page_c(header_editor_frame_c *parent, he_page_base_c *toplevel_page, EbmlMaster *master, const EbmlCallbacks &callbacks,
                  const value_type_e value_type, const translatable_string_c &title, const translatable_string_c &description);
  virtual ~he_value_page_c();

  void init();
  void on_reset_clicked(wxCommandEvent &evt);
  void on_add_or_remove_checked(wxCommandEvent &evt);


  virtual wxControl *create_input_control() = 0;
  virtual wxString get_original_value_as_string() = 0;
  virtual wxString get_current_value_as_string() = 0;
  virtual void reset_value() = 0;
  virtual bool validate_value() = 0;
  virtual void copy_value_to_element() = 0;
  virtual void translate_ui();

  virtual bool has_this_been_modified();
  virtual void modify_this();
  virtual bool validate_this();
  virtual void set_sub_master_callbacks(const EbmlCallbacks &callbacks);
};

#endif // MTX_HE_VALUE_PAGE_H
