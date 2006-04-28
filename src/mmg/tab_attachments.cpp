/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   "attachments" tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "wx/wxprec.h"

#include "wx/wx.h"
#include "wx/dnd.h"
#include "wx/listctrl.h"
#include "wx/notebook.h"
#include "wx/statline.h"

#include "common.h"
#include "extern_data.h"
#include "mmg.h"
#include "tab_attachments.h"

vector<mmg_attachment_t> attachments;

class attachments_drop_target_c: public wxFileDropTarget {
private:
  tab_attachments *owner;
public:
  attachments_drop_target_c(tab_attachments *n_owner):
    owner(n_owner) {};
  virtual bool OnDropFiles(wxCoord x, wxCoord y, const wxArrayString &files) {
    int i;

    for (i = 0; i < files.Count(); i++)
      owner->add_attachment(files[i]);

    return true;
  }
};

tab_attachments::tab_attachments(wxWindow *parent):
  wxPanel(parent, -1, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL) {
  uint32_t i;
  wxStaticBox *sb_top;
  wxStaticBoxSizer *siz_box_top, *siz_box_bottom;
  wxFlexGridSizer *siz_ddlists;
  wxBoxSizer *siz_buttons, *siz_all;

  sb_top = new wxStaticBox(this, wxID_STATIC, wxT("Attachments"));
  siz_box_top = new wxStaticBoxSizer(sb_top,  wxHORIZONTAL);

  lb_attachments = new wxListBox(this, ID_LB_ATTACHMENTS);
  siz_box_top->Add(lb_attachments, 1, wxGROW | wxALL, 5);

  siz_buttons = new wxBoxSizer(wxVERTICAL);
  b_add_attachment = new wxButton(this, ID_B_ADDATTACHMENT, wxT("add"));
  siz_buttons->Add(b_add_attachment, 0, wxALL, 5);
  b_remove_attachment = new wxButton(this, ID_B_REMOVEATTACHMENT,
                                     wxT("remove"));
  b_remove_attachment->Enable(false);
  siz_buttons->Add(b_remove_attachment, 0, wxALL, 5);
  siz_buttons->Add(5, 5, 0, wxGROW | wxALL, 5);

  siz_box_top->Add(siz_buttons);

  sb_options = new wxStaticBox(this, wxID_STATIC, wxT("Attachment options"));
  siz_box_bottom = new wxStaticBoxSizer(sb_options, wxVERTICAL);

  st_name = new wxStaticText(this, wxID_STATIC, wxT("Name:"));
  siz_box_bottom->Add(st_name, 0, wxALIGN_LEFT | wxLEFT, 5);
  tc_name = new wxTextCtrl(this, ID_TC_ATTACHMENTNAME, wxT(""));
  tc_name->SetToolTip(TIP("This is the name that will be stored in the "
                          "output file for this attachment. It defaults "
                          "to the file name of the original file but can "
                          "be changed."));
  siz_box_bottom->Add(tc_name, 0, wxGROW | wxALL, 5);

  st_description = new wxStaticText(this, wxID_STATIC, wxT("Description:"));
  siz_box_bottom->Add(st_description, 0, wxALIGN_LEFT | wxLEFT, 5);
  tc_description =
    new wxTextCtrl(this, ID_TC_DESCRIPTION, wxT(""), wxDefaultPosition,
                   wxDefaultSize, wxTE_MULTILINE | wxTE_WORDWRAP);
  siz_box_bottom->Add(tc_description, 1, wxGROW | wxALL, 5);

  siz_ddlists = new wxFlexGridSizer(2, 3, 0, 0);
  siz_ddlists->AddGrowableCol(0);
  siz_ddlists->AddGrowableCol(2);
  st_mimetype = new wxStaticText(this, wxID_STATIC, wxT("MIME type:"));
  siz_ddlists->Add(st_mimetype, 1, wxALIGN_LEFT | wxLEFT | wxRIGHT, 5);
  siz_ddlists->Add(5, 5, 0, 0, 0);
  st_style = new wxStaticText(this, wxID_STATIC, wxT("Attachment style:"));
  siz_ddlists->Add(st_style, 1, wxALIGN_LEFT | wxRIGHT, 5);

  cob_mimetype =
    new wxComboBox(this, ID_CB_MIMETYPE, wxT(""), wxDefaultPosition,
                   wxDefaultSize, 0, NULL, wxCB_DROPDOWN);
  cob_mimetype->SetToolTip(TIP("MIME type for this track. Select one of the "
                               "pre-defined MIME types or enter one "
                               "yourself."));
  cob_mimetype->Append(wxT(""));
  for (i = 0; mime_types[i].name != NULL; i++)
    cob_mimetype->Append(wxU(mime_types[i].name));
  siz_ddlists->Add(cob_mimetype, 1, wxGROW | wxLEFT | wxRIGHT, 5);
  siz_ddlists->Add(0, 0, 0, 0, 0);

  cob_style =
    new wxComboBox(this, ID_CB_ATTACHMENTSTYLE, wxT(""), wxDefaultPosition,
                   wxDefaultSize, 0, NULL, wxCB_READONLY | wxCB_DROPDOWN);
  cob_style->Append(wxT("To all files"));
  cob_style->Append(wxT("Only to the first"));
  cob_style->SetToolTip(TIP("If splitting is a file can be attached either to "
                            "all files created or only to the first file. Has "
                            "no effect if no splitting is used."));
  siz_ddlists->Add(cob_style, 1, wxGROW | wxRIGHT, 5);

  siz_box_bottom->Add(siz_ddlists, 0, wxBOTTOM | wxGROW, 5);

  siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->Add(siz_box_top, 1, wxGROW | wxALL, 5);
  siz_all->Add(siz_box_bottom, 1, wxGROW | wxALL, 5);
  SetSizer(siz_all);

  enable(false);
  selected_attachment = -1;

  t_get_entries.SetOwner(this, ID_T_ATTACHMENTVALUES);
  t_get_entries.Start(333);

  SetDropTarget(new attachments_drop_target_c(this));
}

void
tab_attachments::enable(bool e) {
  st_name->Enable(e);
  tc_name->Enable(e);
  st_description->Enable(e);
  tc_description->Enable(e);
  st_mimetype->Enable(e);
  cob_mimetype->Enable(e);
  st_style->Enable(e);
  cob_style->Enable(e);
  sb_options->Enable(e);
}

void
tab_attachments::on_add_attachment(wxCommandEvent &evt) {
  wxFileDialog dlg(NULL, wxT("Choose an attachment file"), last_open_dir,
                   wxT(""), wxT(ALLFILES), wxOPEN | wxMULTIPLE);

  if(dlg.ShowModal() == wxID_OK) {
    wxArrayString files;
    int i;

    last_open_dir = dlg.GetDirectory();
    dlg.GetPaths(files);
    for (i = 0; i < files.Count(); i++)
      add_attachment(files[i]);
  }
}

void
tab_attachments::add_attachment(const wxString &file_name) {
  mmg_attachment_t attch;
  wxString name, ext;

  attch.file_name = file_name;
  name = file_name.AfterLast(wxT(PSEP));
  ext = name.AfterLast(wxT('.'));
  name += wxString(wxT(" (")) + file_name.BeforeLast(wxT(PSEP)) + wxT(")");
  lb_attachments->Append(name);
  if (ext.Length() > 0)
    attch.mime_type = wxU(guess_mime_type(wxMB(file_name), true).c_str());
  attch.style = 0;
  attch.stored_name = derive_stored_name_from_file_name(attch.file_name);

  attachments.push_back(attch);
}

void
tab_attachments::on_remove_attachment(wxCommandEvent &evt) {
  if (selected_attachment == -1)
    return;

  attachments.erase(attachments.begin() + selected_attachment);
  lb_attachments->Delete(selected_attachment);
  enable(false);
  b_remove_attachment->Enable(false);
  selected_attachment = -1;
}

void
tab_attachments::on_attachment_selected(wxCommandEvent &evt) {
  mmg_attachment_t *a;
  int new_sel;

  selected_attachment = -1;
  new_sel = lb_attachments->GetSelection();
  if (0 > new_sel)
    return;
  a = &attachments[new_sel];
  tc_name->SetValue(a->stored_name);
  tc_description->SetValue(a->description);
  cob_mimetype->SetValue(a->mime_type);
  cob_style->SetSelection(a->style);
  enable(true);
  selected_attachment = new_sel;
  b_remove_attachment->Enable(true);
}

void
tab_attachments::on_name_changed(wxCommandEvent &evt) {
  if (selected_attachment == -1)
    return;

  attachments[selected_attachment].stored_name = tc_name->GetValue();
}

void
tab_attachments::on_description_changed(wxCommandEvent &evt) {
  if (selected_attachment == -1)
    return;

  attachments[selected_attachment].description = tc_description->GetValue();
}

void
tab_attachments::on_mimetype_changed(wxTimerEvent &evt) {
  if (selected_attachment == -1)
    return;

  attachments[selected_attachment].mime_type = cob_mimetype->GetValue();
}

void
tab_attachments::on_style_changed(wxCommandEvent &evt) {
  if (selected_attachment == -1)
    return;

  attachments[selected_attachment].style =
    cob_style->GetStringSelection().Find(wxT("Only")) >= 0 ? 1 : 0;
}

void
tab_attachments::save(wxConfigBase *cfg) {
  mmg_attachment_t *a;
  uint32_t i, j;
  wxString s;

  cfg->SetPath(wxT("/attachments"));
  cfg->Write(wxT("number_of_attachments"), (int)attachments.size());
  for (i = 0; i < attachments.size(); i++) {
    a = &attachments[i];
    s.Printf(wxT("attachment %u"), i);
    cfg->SetPath(s);
    cfg->Write(wxT("stored_name"), a->stored_name);
    cfg->Write(wxT("file_name"), a->file_name);
    s = wxT("");
    for (j = 0; j < a->description.Length(); j++)
      if (a->description[j] == wxT('\n'))
        s += wxT("!\\N!");
      else
        s += a->description[j];
    cfg->Write(wxT("description"), s);
    cfg->Write(wxT("mime_type"), a->mime_type);
    cfg->Write(wxT("style"), a->style);

    cfg->SetPath(wxT(".."));
  }
}

wxString
tab_attachments::derive_stored_name_from_file_name(const wxString &file_name) {
  return file_name.AfterLast(wxT('/')).AfterLast(wxT('\\'));
}

void
tab_attachments::load(wxConfigBase *cfg,
                      int version) {
  int num, i;

  enable(false);
  selected_attachment = -1;
  lb_attachments->Clear();
  b_remove_attachment->Enable(false);
  attachments.clear();

  cfg->SetPath(wxT("/attachments"));
  if (!cfg->Read(wxT("number_of_attachments"), &num) || (num < 0))
    return;

  for (i = 0; i < (uint32_t)num; i++) {
    mmg_attachment_t a;
    wxString s, c;
    int pos;

    s.Printf(wxT("attachment %d"), i);
    cfg->SetPath(s);
    cfg->Read(wxT("file_name"), &a.file_name);
    if (!cfg->Read(wxT("stored_name"), &a.stored_name) ||
        (a.stored_name == wxT("")))
      a.stored_name = derive_stored_name_from_file_name(a.file_name);
    cfg->Read(wxT("description"), &s);
    cfg->Read(wxT("mime_type"), &a.mime_type);
    cfg->Read(wxT("style"), &a.style);
    if ((a.style != 0) && (a.style != 1))
      a.style = 0;
    pos = s.Find(wxT("!\\N!"));
    while (pos >= 0) {
      c = s.Mid(0, pos);
      s.Remove(0, pos + 4);
      a.description += c + wxT("\n");
      pos = s.Find(wxT("!\\N!"));
    }
    a.description += s;

    s = a.file_name.BeforeLast(PSEP);
    c = a.file_name.AfterLast(PSEP);
    lb_attachments->Append(c + wxT(" (") + s + wxT(")"));
    attachments.push_back(a);

    cfg->SetPath(wxT(".."));
  }
}

bool
tab_attachments::validate_settings() {
  uint32_t i;
  mmg_attachment_t *a;

  for (i = 0; i < attachments.size(); i++) {
    a = &attachments[i];
    if (a->mime_type.Length() == 0) {
      wxMessageBox(wxT("No MIME type has been selected for the attachment '") +
                       a->file_name + wxT("'."), wxT("Missing input"),
                   wxOK | wxCENTER | wxICON_ERROR);
      return false;
    }
  }

  return true;
}

IMPLEMENT_CLASS(tab_attachments, wxPanel);
BEGIN_EVENT_TABLE(tab_attachments, wxPanel)
  EVT_BUTTON(ID_B_ADDATTACHMENT, tab_attachments::on_add_attachment)
  EVT_BUTTON(ID_B_REMOVEATTACHMENT, tab_attachments::on_remove_attachment)
  EVT_LISTBOX(ID_LB_ATTACHMENTS, tab_attachments::on_attachment_selected)
  EVT_TEXT(ID_TC_ATTACHMENTNAME, tab_attachments::on_name_changed)
  EVT_TEXT(ID_TC_DESCRIPTION, tab_attachments::on_description_changed)
  EVT_TIMER(ID_T_ATTACHMENTVALUES, tab_attachments::on_mimetype_changed)
  EVT_COMBOBOX(ID_CB_ATTACHMENTSTYLE, tab_attachments::on_style_changed)
END_EVENT_TABLE();
