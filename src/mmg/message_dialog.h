/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   message dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MESSAGE_DIALOG_H
#define __MESSAGE_DIALOG_H

#include "common/common_pch.h"

#include <wx/dialog.h>
#include <wx/process.h>

class message_dialog: public wxDialog {
  DECLARE_CLASS(message_dialog);
  DECLARE_EVENT_TABLE();
public:

  message_dialog(wxWindow *parent, const wxString &title, const wxString &heading, const wxString &message);
  ~message_dialog();

  void on_ok(wxCommandEvent &evt);

  static int show(wxWindow *parent, const wxString &title, const wxString &heading, const wxString &message);
};

#endif // __MESSAGE_DIALOG_H
