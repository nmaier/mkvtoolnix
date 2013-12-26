/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "attachments" tab

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/wx.h>
#include <wx/dnd.h>
#include <wx/filename.h>
#include <wx/listctrl.h>
#include <wx/notebook.h>
#include <wx/statline.h>

#include "common/extern_data.h"
#include "mmg/mmg.h"
#include "mmg/tabs/attachments.h"

std::vector<mmg_attachment_cptr> attachments;
std::vector<mmg_attached_file_cptr> attached_files;

class attachments_drop_target_c: public wxFileDropTarget {
private:
  tab_attachments *owner;
public:
  attachments_drop_target_c(tab_attachments *n_owner):
    owner(n_owner) {};
  virtual bool OnDropFiles(wxCoord /* x */, wxCoord /* y */, const wxArrayString &dropped_files) {
    size_t i;

    for (i = 0; i < dropped_files.Count(); i++)
      owner->add_attachment(dropped_files[i]);

    return true;
  }
};

tab_attachments::tab_attachments(wxWindow *parent):
  wxPanel(parent, -1, wxDefaultPosition, wxDefaultSize, wxTAB_TRAVERSAL) {

  // Create all elements
  sb_attached_files  = new wxStaticBox(this, wxID_STATIC, wxEmptyString);
  clb_attached_files = new wxCheckListBox(this, ID_CLB_ATTACHED_FILES);

  b_enable_all       = new wxButton(this, ID_B_ENABLEALLATTACHED);
  b_disable_all      = new wxButton(this, ID_B_DISABLEALLATTACHED);
  b_enable_all->Enable(false);
  b_disable_all->Enable(false);

  sb_attachments      = new wxStaticBox(this, wxID_STATIC, wxEmptyString);
  lb_attachments      = new wxListBox(this, ID_LB_ATTACHMENTS);

  b_add_attachment    = new wxButton(this, ID_B_ADDATTACHMENT);
  b_remove_attachment = new wxButton(this, ID_B_REMOVEATTACHMENT);
  b_remove_attachment->Enable(false);

  wxStaticLine *sl_options = new wxStaticLine(this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLI_HORIZONTAL);

  st_name = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  tc_name = new wxTextCtrl(this, ID_TC_ATTACHMENTNAME);
  tc_name->SetSizeHints(0, -1);

  st_description = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  tc_description = new wxTextCtrl(this, ID_TC_DESCRIPTION);
  tc_description->SetSizeHints(0, -1);

  st_mimetype    = new wxStaticText(this, wxID_STATIC, wxEmptyString);
  cob_mimetype   = new wxMTX_COMBOBOX_TYPE(this, ID_CB_MIMETYPE, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_DROPDOWN);
  auto entries   = wxArrayString{};
  entries.Alloc(mime_types.size() + 1);
  entries.Add(wxEmptyString);
  for (auto &mime_type : mime_types)
    entries.Add(wxU(mime_type.name));
  cob_mimetype->Append(entries);
  cob_mimetype->SetSizeHints(0, -1);

  st_style  = new wxStaticText(this, wxID_STATIC, wxEmptyString);

  cob_style = new wxMTX_COMBOBOX_TYPE(this, ID_CB_ATTACHMENTSTYLE, wxEmptyString, wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY | wxCB_DROPDOWN);
  cob_style->Append(wxEmptyString);
  cob_style->Append(wxEmptyString);
  cob_style->SetSizeHints(0, -1);

  // Create the layout.
  wxBoxSizer *siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(clb_attached_files, 1, wxGROW | wxALL, 5);

  wxBoxSizer *siz_buttons = new wxBoxSizer(wxVERTICAL);
  siz_buttons->Add(b_enable_all,  0, wxGROW | wxALL, 5);
  siz_buttons->Add(b_disable_all, 0, wxGROW | wxALL, 5);
  siz_buttons->Add(5, 5,          0, wxGROW | wxALL, 5);

  siz_line->Add(siz_buttons, 0, wxGROW, 0);

  wxStaticBoxSizer *siz_box_attached_files = new wxStaticBoxSizer(sb_attached_files, wxHORIZONTAL);
  siz_box_attached_files->Add(siz_line, 1, wxGROW, 0);

  siz_line = new wxBoxSizer(wxHORIZONTAL);
  siz_line->Add(lb_attachments, 1, wxGROW | wxALL, 5);

  siz_buttons = new wxBoxSizer(wxVERTICAL);
  siz_buttons->Add(b_add_attachment,    0, wxGROW | wxALL, 5);
  siz_buttons->Add(b_remove_attachment, 0, wxGROW | wxALL, 5);
  siz_buttons->Add(5, 5,                0, wxGROW | wxALL, 5);

  siz_line->Add(siz_buttons, 0, wxGROW, 0);

  wxStaticBoxSizer *siz_box_attachments = new wxStaticBoxSizer(sb_attachments, wxVERTICAL);
  siz_box_attachments->Add(siz_line, 1, wxGROW, 0);

  siz_box_attachments->Add(sl_options, 0, wxGROW | wxALL, 5);

  wxFlexGridSizer *siz_fg = new wxFlexGridSizer(4, 5, 5);
  siz_fg->AddGrowableCol(1);
  siz_fg->AddGrowableCol(3);

  siz_fg->Add(st_name,        0, wxALIGN_CENTER_VERTICAL,          0);
  siz_fg->Add(tc_name,        1, wxALIGN_CENTER_VERTICAL | wxGROW, 0);
  siz_fg->Add(st_description, 0, wxALIGN_CENTER_VERTICAL,          0);
  siz_fg->Add(tc_description, 1, wxALIGN_CENTER_VERTICAL | wxGROW, 0);
  siz_fg->Add(st_mimetype,    0, wxALIGN_CENTER_VERTICAL,          0);
  siz_fg->Add(cob_mimetype,   1, wxALIGN_CENTER_VERTICAL | wxGROW, 0);
  siz_fg->Add(st_style,       0, wxALIGN_CENTER_VERTICAL,          0);
  siz_fg->Add(cob_style,      1, wxALIGN_CENTER_VERTICAL | wxGROW, 0);

  siz_box_attachments->Add(siz_fg, 0, wxALL | wxGROW, 5);

  wxBoxSizer *siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->Add(siz_box_attached_files, 1, wxALL | wxGROW, 5);
  siz_all->Add(siz_box_attachments,    1, wxALL | wxGROW, 5);

  SetSizerAndFit(siz_all);

  enable(false);
  selected_attachment = -1;

  t_get_entries.SetOwner(this, ID_T_ATTACHMENTVALUES);
  t_get_entries.Start(333);

  SetDropTarget(new attachments_drop_target_c(this));
}

void
tab_attachments::translate_ui() {
  sb_attached_files->SetLabel(Z("Attached files"));
  b_enable_all->SetLabel(Z("enable all"));
  b_disable_all->SetLabel(Z("disable all"));
  sb_attachments->SetLabel(Z("Attachments"));
  b_add_attachment->SetLabel(Z("add"));
  b_remove_attachment->SetLabel(Z("remove"));
  st_name->SetLabel(Z("Name:"));
  tc_name->SetToolTip(TIP("This is the name that will be stored in the output file for this attachment. It defaults to the file name of the original file but can be changed."));
  st_description->SetLabel(Z("Description:"));
  st_mimetype->SetLabel(Z("MIME type:"));
  cob_mimetype->SetToolTip(TIP("MIME type for this track. Select one of the pre-defined MIME types or enter one yourself."));
  st_style->SetLabel(Z("Attachment style:"));

  int selection = cob_style->GetSelection();
  cob_style->SetString(0, Z("To all files"));
  cob_style->SetString(1, Z("Only to the first"));
  cob_style->SetToolTip(TIP("If splitting is a file can be attached either to all files created or only to the first file. Has no effect if no splitting is used."));
  cob_style->SetSelection(selection);
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
}

void
tab_attachments::enable_attached_files_buttons() {
  b_enable_all->Enable(!m_attached_files.empty());
  b_disable_all->Enable(!m_attached_files.empty());
}

void
tab_attachments::on_add_attachment(wxCommandEvent &) {
  wxFileDialog dlg(nullptr, Z("Choose an attachment file"), last_open_dir, wxEmptyString, ALLFILES, wxFD_OPEN | wxFD_MULTIPLE);

  if(dlg.ShowModal() == wxID_OK) {
    wxArrayString selected_files;
    size_t i;

    last_open_dir = dlg.GetDirectory();
    dlg.GetPaths(selected_files);
    for (i = 0; i < selected_files.Count(); i++)
      add_attachment(selected_files[i]);
  }
}

void
tab_attachments::add_attachment(const wxString &file_name) {
  mmg_attachment_cptr attch = mmg_attachment_cptr(new mmg_attachment_t);

  attch->file_name  = file_name;
  wxString name     = file_name.AfterLast(wxT(PSEP));
  wxString ext      = name.AfterLast(wxT('.'));
  name             += wxString(wxT(" (")) + file_name.BeforeLast(wxT(PSEP)) + wxT(")");
  lb_attachments->Append(name);

  if (ext.Length() > 0)
    attch->mime_type = wxU(guess_mime_type(wxMB(file_name), true));
  attch->style       = 0;
  attch->stored_name = derive_stored_name_from_file_name(attch->file_name);

  attachments.push_back(attch);
}

void
tab_attachments::on_remove_attachment(wxCommandEvent &) {
  if (selected_attachment == -1)
    return;

  attachments.erase(attachments.begin() + selected_attachment);
  lb_attachments->Delete(selected_attachment);
  enable(false);
  b_remove_attachment->Enable(false);
  selected_attachment = -1;
}

void
tab_attachments::on_attachment_selected(wxCommandEvent &) {
  int new_sel;

  selected_attachment = -1;
  new_sel = lb_attachments->GetSelection();
  if (0 > new_sel)
    return;

  mmg_attachment_cptr &a = attachments[new_sel];
  tc_name->SetValue(a->stored_name);
  tc_description->SetValue(a->description);
  cob_mimetype->SetValue(a->mime_type);
  cob_style->SetSelection(a->style);
  enable(true);
  selected_attachment = new_sel;
  b_remove_attachment->Enable(true);
}

void
tab_attachments::on_name_changed(wxCommandEvent &) {
  if (selected_attachment == -1)
    return;

  attachments[selected_attachment]->stored_name = tc_name->GetValue();
}

void
tab_attachments::on_description_changed(wxCommandEvent &) {
  if (selected_attachment == -1)
    return;

  attachments[selected_attachment]->description = tc_description->GetValue();
}

void
tab_attachments::on_mimetype_changed(wxTimerEvent &) {
  if (selected_attachment == -1)
    return;

  attachments[selected_attachment]->mime_type = cob_mimetype->GetValue();
}

void
tab_attachments::on_style_changed(wxCommandEvent &) {
  if (selected_attachment == -1)
    return;

  attachments[selected_attachment]->style = cob_style->GetStringSelection().Find(wxT("Only")) >= 0 ? 1 : 0;
}

void
tab_attachments::save(wxConfigBase *cfg) {
  unsigned int i;
  wxString s;

  cfg->SetPath(wxT("/attachments"));
  cfg->Write(wxT("number_of_attachments"), (int)attachments.size());
  for (i = 0; i < attachments.size(); i++) {
    mmg_attachment_cptr &a = attachments[i];
    s.Printf(wxT("attachment %u"), i);
    cfg->SetPath(s);
    cfg->Write(wxT("stored_name"), a->stored_name);
    cfg->Write(wxT("file_name"), a->file_name);
    s = wxEmptyString;
    size_t j;
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
                      int) {
  size_t i;
  int num;

  enable(false);
  selected_attachment = -1;
  lb_attachments->Clear();
  b_remove_attachment->Enable(false);
  attachments.clear();
  clb_attached_files->Clear();
  m_attached_files.clear();

  unsigned int fidx;
  for (fidx = 0; files.size() > fidx; ++fidx) {
    mmg_file_cptr &f = files[fidx];

    unsigned int aidx;
    for (aidx = 0; f->attached_files.size() > aidx; ++aidx)
      add_attached_file(f->attached_files[aidx]);
  }

  enable_attached_files_buttons();

  cfg->SetPath(wxT("/attachments"));
  if (!cfg->Read(wxT("number_of_attachments"), &num) || (num <= 0))
    return;

  for (i = 0; i < (uint32_t)num; i++) {
    mmg_attachment_cptr a = mmg_attachment_cptr(new mmg_attachment_t);
    wxString s, c;
    int pos;

    s.Printf(wxT("attachment %d"), i);
    cfg->SetPath(s);
    cfg->Read(wxT("file_name"), &a->file_name);
    if (!cfg->Read(wxT("stored_name"), &a->stored_name) ||
        (a->stored_name == wxEmptyString))
      a->stored_name = derive_stored_name_from_file_name(a->file_name);
    cfg->Read(wxT("description"), &s);
    cfg->Read(wxT("mime_type"), &a->mime_type);
    cfg->Read(wxT("style"), &a->style);
    if ((a->style != 0) && (a->style != 1))
      a->style = 0;
    pos = s.Find(wxT("!\\N!"));
    while (pos >= 0) {
      c = s.Mid(0, pos);
      s.Remove(0, pos + 4);
      a->description += c + wxT("\n");
      pos = s.Find(wxT("!\\N!"));
    }
    a->description += s;

    s = a->file_name.BeforeLast(PSEP);
    c = a->file_name.AfterLast(PSEP);
    lb_attachments->Append(c + wxT(" (") + s + wxT(")"));
    attachments.push_back(a);

    cfg->SetPath(wxT(".."));
  }
}

bool
tab_attachments::validate_settings() {
  unsigned int i;
  for (i = 0; i < attachments.size(); i++) {
    mmg_attachment_cptr &a = attachments[i];
    if (a->mime_type.Length() == 0) {
      wxMessageBox(wxString::Format(Z("No MIME type has been selected for the attachment '%s'."), a->file_name.c_str()), Z("Missing input"), wxOK | wxCENTER | wxICON_ERROR);
      return false;
    }
  }

  return true;
}

void
tab_attachments::add_attached_file(mmg_attached_file_cptr &a) {
  wxFileName file_name(a->source->file_name);
  clb_attached_files->Append(wxString::Format(Z("%s (MIME type %s, size %ld) from %s (%s)"),
                                              a->name.c_str(), a->mime_type.c_str(), a->size, file_name.GetFullName().c_str(), file_name.GetPath().c_str()));
  clb_attached_files->Check(m_attached_files.size(), a->enabled);
  m_attached_files.push_back(a);

  enable_attached_files_buttons();
}

void
tab_attachments::remove_attached_files_for(mmg_file_cptr &f) {
  int i;
  for (i = m_attached_files.size() - 1; 0 <= i; --i) {
    if (m_attached_files[i]->source == f.get()) {
      clb_attached_files->Delete(i);
      m_attached_files.erase(m_attached_files.begin() + i);
    }
  }

  enable_attached_files_buttons();
}

void
tab_attachments::remove_all_attached_files() {
  clb_attached_files->Clear();
  m_attached_files.clear();

  enable_attached_files_buttons();
}

void
tab_attachments::on_attached_file_enabled(wxCommandEvent &evt) {
  size_t idx = evt.GetSelection();
  if (m_attached_files.size() > idx)
    m_attached_files[idx]->enabled = clb_attached_files->IsChecked(idx);
}

void
tab_attachments::on_enable_all(wxCommandEvent &) {
  size_t idx;
  for (idx = 0; m_attached_files.size() > idx; ++idx) {
    clb_attached_files->Check(idx, true);
    m_attached_files[idx]->enabled = true;
  }
}

void
tab_attachments::on_disable_all(wxCommandEvent &) {
  size_t idx;
  for (idx = 0; m_attached_files.size() > idx; ++idx) {
    clb_attached_files->Check(idx, false);
    m_attached_files[idx]->enabled = false;
  }
}

IMPLEMENT_CLASS(tab_attachments, wxPanel);
BEGIN_EVENT_TABLE(tab_attachments, wxPanel)
  EVT_BUTTON(ID_B_ADDATTACHMENT,          tab_attachments::on_add_attachment)
  EVT_BUTTON(ID_B_REMOVEATTACHMENT,       tab_attachments::on_remove_attachment)
  EVT_LISTBOX(ID_LB_ATTACHMENTS,          tab_attachments::on_attachment_selected)
  EVT_TEXT(ID_TC_ATTACHMENTNAME,          tab_attachments::on_name_changed)
  EVT_TEXT(ID_TC_DESCRIPTION,             tab_attachments::on_description_changed)
  EVT_TIMER(ID_T_ATTACHMENTVALUES,        tab_attachments::on_mimetype_changed)
  EVT_COMBOBOX(ID_CB_ATTACHMENTSTYLE,     tab_attachments::on_style_changed)
  EVT_CHECKLISTBOX(ID_CLB_ATTACHED_FILES, tab_attachments::on_attached_file_enabled)
  EVT_BUTTON(ID_B_ENABLEALLATTACHED,      tab_attachments::on_enable_all)
  EVT_BUTTON(ID_B_DISABLEALLATTACHED,     tab_attachments::on_disable_all)
END_EVENT_TABLE();
