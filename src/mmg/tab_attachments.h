/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes
  
   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html
  
   $Id$
  
   declarations for the attachments tab
  
   Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#ifndef __TAB_ATTACHMENTS_H
#define __TAB_ATTACHMENTS_H

#include "os.h"

#include "wx/config.h"

#define ID_B_ADDATTACHMENT                 12001
#define ID_B_REMOVEATTACHMENT              12002
#define ID_CB_MIMETYPE                     12003
#define ID_TC_DESCRIPTION                  12004
#define ID_CB_ATTACHMENTSTYLE              12005
#define ID_LB_ATTACHMENTS                  12006
#define ID_T_ATTACHMENTVALUES 10058

class tab_attachments: public wxPanel {
  DECLARE_CLASS(tab_attachments);
  DECLARE_EVENT_TABLE();
protected:
  wxListBox *lb_attachments;
  wxButton *b_add_attachment, *b_remove_attachment;
  wxComboBox *cob_mimetype, *cob_style;
  wxTextCtrl *tc_description;
  wxStaticText *st_description, *st_mimetype, *st_style;
  wxStaticBox *sb_options;

  int selected_attachment;

  wxTimer t_get_entries;

public:
  tab_attachments(wxWindow *parent);

  void on_add_attachment(wxCommandEvent &evt);
  void add_attachment(const wxString &file_name);
  void on_remove_attachment(wxCommandEvent &evt);
  void on_attachment_selected(wxCommandEvent &evt);
  void on_description_changed(wxCommandEvent &evt);
  void on_mimetype_changed(wxTimerEvent &evt);
  void on_style_changed(wxCommandEvent &evt);

  void enable(bool e);

  void save(wxConfigBase *cfg);
  void load(wxConfigBase *cfg);
  bool validate_settings();
};

#endif // __TAB_ATTACHMENTS_H
