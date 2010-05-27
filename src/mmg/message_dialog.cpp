/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   message dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/wx.h>
#include <wx/statline.h>

#include "mmg/mmg.h"
#include "mmg/message_dialog.h"

message_dialog::message_dialog(wxWindow *parent,
                               const wxString &title,
                               const wxString &heading,
                               const wxString &message)
  : wxDialog(parent, -1, title, wxDefaultPosition, wxSize(700, 560), wxDEFAULT_FRAME_STYLE)
{
  wxStaticText *st_heading = new wxStaticText(this, wxID_ANY, heading);
  wxStaticLine *sl_line1   = new wxStaticLine(this);
  wxTextCtrl *tc_message   = new wxTextCtrl(this,   wxID_ANY, message, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);
  wxButton *b_ok           = new wxButton(this,     wxID_OK,  Z("Ok"));

  b_ok->SetDefault();

  wxBoxSizer *siz_buttons = new wxBoxSizer(wxHORIZONTAL);
  siz_buttons->AddStretchSpacer();
  siz_buttons->Add(b_ok);

  wxBoxSizer *siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->AddSpacer(5);
  siz_all->Add(st_heading,  0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);
  siz_all->Add(sl_line1,    0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);
  siz_all->Add(tc_message,  1, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);
  siz_all->Add(siz_buttons, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);

  SetSizer(siz_all);
}

message_dialog::~message_dialog() {
}

void
message_dialog::on_ok(wxCommandEvent &evt) {
  EndModal(wxOK);
}

int
message_dialog::show(wxWindow *parent,
                     const wxString &title,
                     const wxString &heading,
                     const wxString &message) {
  message_dialog dlg(parent, title, heading, message);
  return dlg.ShowModal();
}

IMPLEMENT_CLASS(message_dialog, wxDialog);
BEGIN_EVENT_TABLE(message_dialog, wxDialog)
  EVT_BUTTON(wxID_OK, message_dialog::on_ok)
END_EVENT_TABLE();
