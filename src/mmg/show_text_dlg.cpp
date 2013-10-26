/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "show text" dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/wx.h>

#include "mmg/show_text_dlg.h"

show_text_dlg::show_text_dlg(wxWindow *parent,
                             const wxString &title,
                             const wxString &text)
  : wxDialog(parent, 0, title)
  , m_geometry_saver{this, "show_text_dlg"}
{
  wxBoxSizer *siz_all, *siz_line;
  wxTextCtrl *tc_message;

  siz_all = new wxBoxSizer(wxVERTICAL);

  tc_message = new wxTextCtrl(this, 0, text, wxDefaultPosition, wxDefaultSize, wxTE_MULTILINE | wxTE_READONLY);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(tc_message, 1, wxALL | wxGROW, 10);

  siz_all->Add(siz_line, 1, wxGROW, 0);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->AddStretchSpacer();
  siz_line->Add(CreateStdDialogButtonSizer(wxOK), 0, 0, 0);
  siz_line->AddSpacer(10);

  siz_all->Add(siz_line, 0, wxGROW, 0);
  siz_all->AddSpacer(10);

  SetSizerAndFit(siz_all);

  m_geometry_saver.set_default_size(400, 350, true).restore();
}

IMPLEMENT_CLASS(show_text_dlg, wxDialog);
BEGIN_EVENT_TABLE(show_text_dlg, wxDialog)
END_EVENT_TABLE();
