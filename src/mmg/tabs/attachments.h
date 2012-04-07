/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   declarations for the attachments tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __TAB_ATTACHMENTS_H
#define __TAB_ATTACHMENTS_H

#include "common/common_pch.h"

#include <wx/wx.h>

#define ID_B_ADDATTACHMENT                 12001
#define ID_B_REMOVEATTACHMENT              12002
#define ID_CB_MIMETYPE                     12003
#define ID_TC_DESCRIPTION                  12004
#define ID_CB_ATTACHMENTSTYLE              12005
#define ID_LB_ATTACHMENTS                  12006
#define ID_TC_ATTACHMENTNAME               12007
#define ID_CLB_ATTACHED_FILES              12008
#define ID_B_ENABLEALLATTACHED             12009
#define ID_B_DISABLEALLATTACHED            12010
#define ID_T_ATTACHMENTVALUES              10058

class tab_attachments: public wxPanel {
  DECLARE_CLASS(tab_attachments);
  DECLARE_EVENT_TABLE();
protected:
  wxStaticBox *sb_attached_files, *sb_attachments;
  wxCheckListBox *clb_attached_files;
  wxListBox *lb_attachments;
  wxButton *b_enable_all, *b_disable_all, *b_add_attachment, *b_remove_attachment;
  wxMTX_COMBOBOX_TYPE *cob_mimetype, *cob_style;
  wxTextCtrl *tc_description, *tc_name;
  wxStaticText *st_name, *st_description, *st_mimetype, *st_style;

  int selected_attachment;

  wxTimer t_get_entries;

  std::vector<mmg_attached_file_cptr> m_attached_files;

public:
  tab_attachments(wxWindow *parent);

  void on_add_attachment(wxCommandEvent &evt);
  void add_attachment(const wxString &file_name);
  void on_remove_attachment(wxCommandEvent &evt);
  void on_attachment_selected(wxCommandEvent &evt);
  void on_name_changed(wxCommandEvent &evt);
  void on_description_changed(wxCommandEvent &evt);
  void on_mimetype_changed(wxTimerEvent &evt);
  void on_style_changed(wxCommandEvent &evt);
  void on_attached_file_enabled(wxCommandEvent &evt);
  void on_enable_all(wxCommandEvent &evt);
  void on_disable_all(wxCommandEvent &evt);

  void enable(bool e);
  void enable_attached_files_buttons();

  wxString derive_stored_name_from_file_name(const wxString &src);

  void save(wxConfigBase *cfg);
  void load(wxConfigBase *cfg, int version);
  bool validate_settings();

  void add_attached_file(mmg_attached_file_cptr &a);
  void remove_attached_files_for(mmg_file_cptr &f);
  void remove_all_attached_files();

  void translate_ui();
};

#endif // __TAB_ATTACHMENTS_H
