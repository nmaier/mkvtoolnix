/*
  mkvmerge GUI -- utility for splicing together matroska files
      from component media subtypes

  tab_attachments.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>
  Parts of this code were written by Florian Wager <root@sirelvis.de>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief "attachments" tab
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include "wx/wxprec.h"

#include "wx/wx.h"
#include "wx/notebook.h"
#include "wx/listctrl.h"
#include "wx/statline.h"

#include "mmg.h"
#include "common.h"
#include "extern_data.h"

vector<mmg_attachment_t> attachments;

tab_attachments::tab_attachments(wxWindow *parent):
  wxPanel(parent, -1, wxDefaultPosition, wxSize(100, 400), wxSUNKEN_BORDER |
          wxTAB_TRAVERSAL) {
  wxArrayString sorted_mime_types;
  uint32_t i;

  new wxStaticText(this, wxID_STATIC, _("Attachments:"), wxPoint(5, 5),
                   wxDefaultSize, 0);
  lb_attachments =
    new wxListBox(this, ID_LB_ATTACHMENTS, wxPoint(5, 24), wxSize(435, 120),
                  0);

  b_add_attachment =
    new wxButton(this, ID_B_ADDATTACHMENT, _("+"), wxPoint(450, 24),
                 wxSize(24, -1), 0);
  b_remove_attachment =
    new wxButton(this, ID_B_REMOVEATTACHMENT, _("-"), wxPoint(450, 56),
                 wxSize(24, -1), 0);
  b_remove_attachment->Enable(false);
  new wxStaticText(this, wxID_STATIC, _("Attachment options:"),
                   wxPoint(5, 150), wxDefaultSize, 0);

  new wxStaticText(this, wxID_STATIC, _("Description:"), wxPoint(10, 175),
                   wxDefaultSize, 0);
  tc_description =
    new wxTextCtrl(this, ID_TC_DESCRIPTION, _(""), wxPoint(5, 195),
                   wxSize(480, 160), wxTE_MULTILINE | wxTE_WORDWRAP);


  new wxStaticText(this, wxID_STATIC, _("MIME type:"), wxPoint(5, 365),
                   wxDefaultSize, 0);

  for (i = 0; mime_types[i] != NULL; i++)
    sorted_mime_types.Add(mime_types[i]);
  sorted_mime_types.Sort();

  cob_mimetype =
    new wxComboBox(this, ID_CB_MIMETYPE, _(""), wxPoint(5, 385),
                   wxSize(250, -1), 0, NULL, wxCB_DROPDOWN);
  cob_mimetype->SetToolTip(_("MIME type for this track. Select one of the "
                             "pre-defined MIME types or enter one yourself."));
  cob_mimetype->Append(_(""));
  for (i = 0; i < sorted_mime_types.Count(); i++)
    cob_mimetype->Append(sorted_mime_types[i]);

  new wxStaticText(this, wxID_STATIC, _("Attachment style:"),
                   wxPoint(275, 365), wxDefaultSize, 0);
  cob_style =
    new wxComboBox(this, ID_CB_ATTACHMENTSTYLE, _(""), wxPoint(275, 385),
                   wxSize(205, -1), 0, NULL, wxCB_READONLY | wxCB_DROPDOWN);
  cob_style->Append(_("To all files"));
  cob_style->Append(_("Only to the first"));
  cob_style->SetToolTip(_("If splitting is a file can be attached either to "
                          "all files created or only to the first file. Has "
                          "no effect if no splitting is used."));

  enable(false);
  selected_attachment = -1;

  t_get_entries.SetOwner(this, ID_T_ATTACHMENTVALUES);
  t_get_entries.Start(333);
}

void tab_attachments::enable(bool e) {
  tc_description->Enable(e);
  cob_mimetype->Enable(e);
  cob_style->Enable(e);
}

void tab_attachments::on_add_attachment(wxCommandEvent &evt) {
  mmg_attachment_t attch;
  wxString name;

  wxFileDialog dlg(NULL, "Choose an attachment file", "", last_open_dir,
                   _T(ALLFILES), wxOPEN);

  if(dlg.ShowModal() == wxID_OK) {
    last_open_dir = dlg.GetDirectory();
    attch.file_name = new wxString(dlg.GetPath());
    name = dlg.GetFilename();
    name += " (";
    name += last_open_dir;
    name += ")";
    lb_attachments->Append(name);
    attch.mime_type = new wxString("");
    attch.description = new wxString("");
    attch.style = 0;

    attachments.push_back(attch);
  }
}

void tab_attachments::on_remove_attachment(wxCommandEvent &evt) {
  mmg_attachment_t *a;
  vector<mmg_attachment_t>::iterator eit;

  if (selected_attachment == -1)
    return;

  a = &attachments[selected_attachment];
  delete a->file_name;
  delete a->description;
  delete a->mime_type;
  eit = attachments.begin();
  eit += selected_attachment;
  attachments.erase(eit);
  lb_attachments->Delete(selected_attachment);
  enable(false);
  b_remove_attachment->Enable(false);
  selected_attachment = -1;
}

void tab_attachments::on_attachment_selected(wxCommandEvent &evt) {
  mmg_attachment_t *a;
  int new_sel;

  selected_attachment = -1;
  new_sel = lb_attachments->GetSelection();
  a = &attachments[new_sel];
  tc_description->SetValue(*a->description);
  cob_mimetype->SetValue(*a->mime_type);
  cob_style->SetSelection(a->style);
  enable(true);
  selected_attachment = new_sel;
  b_remove_attachment->Enable(true);
}

void tab_attachments::on_description_changed(wxCommandEvent &evt) {
  if (selected_attachment == -1)
    return;

  *attachments[selected_attachment].description =
    tc_description->GetValue();
}

void tab_attachments::on_mimetype_changed(wxTimerEvent &evt) {
  if (selected_attachment == -1)
    return;

  *attachments[selected_attachment].mime_type =
    cob_mimetype->GetValue();
}

void tab_attachments::on_style_changed(wxCommandEvent &evt) {
  if (selected_attachment == -1)
    return;

  attachments[selected_attachment].style =
    cob_style->GetStringSelection().Find("Only") >= 0 ? 1 : 0;
}

void tab_attachments::save(wxConfigBase *cfg) {
  mmg_attachment_t *a;
  uint32_t i, j;
  wxString s;

  cfg->SetPath("/attachments");
  cfg->Write("number_of_attachments", (int)attachments.size());
  for (i = 0; i < attachments.size(); i++) {
    a = &attachments[i];
    s.Printf("attachment %u", i);
    cfg->SetPath(s);
    cfg->Write("file_name", *a->file_name);
    s = "";
    for (j = 0; j < a->description->Length(); j++)
      if ((*a->description)[j] == '\n')
        s += "!\\N!";
      else
        s += (*a->description)[j];
    cfg->Write("description", s);
    cfg->Write("mime_type", *a->mime_type);
    cfg->Write("style", a->style);

    cfg->SetPath("..");
  }
}

void tab_attachments::load(wxConfigBase *cfg) {
  mmg_attachment_t *ap, a;
  uint32_t i;
  int num, pos;
  wxString s, c;

  enable(false);
  selected_attachment = -1;
  lb_attachments->Clear();
  b_remove_attachment->Enable(false);
  for (i = 0; i < attachments.size(); i++) {
    ap = &attachments[i];
    delete ap->file_name;
    delete ap->description;
    delete ap->mime_type;
  }
  attachments.clear();

  cfg->SetPath("/attachments");
  if (!cfg->Read("number_of_attachments", &num) || (num < 0))
    return;

  for (i = 0; i < (uint32_t)num; i++) {
    s.Printf("attachment %d", i);
    cfg->SetPath(s);
    a.file_name = new wxString;
    a.description = new wxString;
    a.mime_type = new wxString;
    cfg->Read("file_name", a.file_name);
    cfg->Read("description", &s);
    cfg->Read("mime_type", a.mime_type);
    cfg->Read("style", &a.style);
    if ((a.style != 0) && (a.style != 1))
      a.style = 0;
    pos = s.Find("!\\N!");
    while (pos >= 0) {
      c = s.Mid(0, pos);
      s.Remove(0, pos + 4);
      *a.description += c + "\n";
      pos = s.Find("!\\N!");
    }
    *a.description += s;

    s = a.file_name->BeforeLast(PSEP);
    c = a.file_name->AfterLast(PSEP);
    lb_attachments->Append(c + " (" + s + ")");
    attachments.push_back(a);

    cfg->SetPath("..");
  }
}

IMPLEMENT_CLASS(tab_attachments, wxPanel);
BEGIN_EVENT_TABLE(tab_attachments, wxPanel)
  EVT_BUTTON(ID_B_ADDATTACHMENT, tab_attachments::on_add_attachment)
  EVT_BUTTON(ID_B_REMOVEATTACHMENT, tab_attachments::on_remove_attachment)
  EVT_LISTBOX(ID_LB_ATTACHMENTS, tab_attachments::on_attachment_selected)
  EVT_TEXT(ID_TC_DESCRIPTION, tab_attachments::on_description_changed)
  EVT_TIMER(ID_T_ATTACHMENTVALUES, tab_attachments::on_mimetype_changed)
  EVT_COMBOBOX(ID_CB_ATTACHMENTSTYLE, tab_attachments::on_style_changed)
END_EVENT_TABLE();
