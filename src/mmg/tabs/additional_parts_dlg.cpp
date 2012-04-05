/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "additional parts" dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include <wx/wx.h>

#include "common/common_pch.h"

#include <wx/statline.h>

#include "mmg/tabs/additional_parts_dlg.h"

#include "mmg/mmg.h"

additional_parts_dialog::additional_parts_dialog(wxWindow *parent,
                                                 wxFileName const &primary_file_name,
                                                 std::vector<wxFileName> const &files)
  : wxDialog(parent, -1, Z("Additional source file parts"), wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
  , m_primary_file_name(primary_file_name)
  , m_files(files)
{
  // Create controls
  m_lv_files = new wxListView(this, ID_ADDPARTS_LV_FILES, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxSUNKEN_BORDER);

  m_b_add    = new wxButton(this, ID_ADDPARTS_B_ADD,    Z("&Add"));
  m_b_remove = new wxButton(this, ID_ADDPARTS_B_REMOVE, Z("&Remove"));
  m_b_up     = new wxButton(this, ID_ADDPARTS_B_UP,     Z("&Up"));
  m_b_down   = new wxButton(this, ID_ADDPARTS_B_DOWN,   Z("&Down"));
  m_b_down   = new wxButton(this, ID_ADDPARTS_B_SORT,   Z("&Sort"));
  m_b_close  = new wxButton(this, ID_ADDPARTS_B_CLOSE,  Z("&Close"));

  wxStaticText *st_title             = new wxStaticText(this, wxID_ANY, Z("Edit additional source file parts"));
  wxStaticText *st_primary_file_name = new wxStaticText(this, wxID_ANY, Z("Primary file name:"));
  wxStaticText *st_directory         = new wxStaticText(this, wxID_ANY, Z("Directory:"));

  wxStaticLine *sl_title             = new wxStaticLine(this);

  wxTextCtrl *tc_primary_file_name   = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
  wxTextCtrl *tc_directory           = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);

  // Setup controls

  wxListItem column_item;
  column_item.SetMask(wxLIST_MASK_TEXT);

  column_item.SetText(Z("File name"));
  m_lv_files->InsertColumn(0, column_item);
  column_item.SetText(Z("Directory"));
  m_lv_files->InsertColumn(1, column_item);

  // for (auto file_name : files | brng::indexed(0)) {
  //   auto index = m_lv_files->InsertItem(file_name.index(), file_name->GetFullName());
  //   m_lv_files->SetItem(index, 1, file_name->GetPath());
  // }

  tc_primary_file_name->SetValue(m_primary_file_name.GetFullName());
  tc_directory->SetValue(m_primary_file_name.GetPath());

  // Create layout

  wxBoxSizer *siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->Add(st_title, 0, wxALL, 5);
  siz_all->Add(sl_title, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);

  wxGridSizer *siz_primary_file_name = new wxGridSizer(2, 2, 5, 5);
  siz_primary_file_name->Add(st_primary_file_name);
  siz_primary_file_name->Add(tc_primary_file_name, 1, wxGROW);
  siz_primary_file_name->Add(st_directory);
  siz_primary_file_name->Add(tc_directory,         1, wxGROW);

  siz_all->Add(siz_primary_file_name, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);

  wxBoxSizer *siz_buttons = new wxBoxSizer(wxVERTICAL);
  siz_buttons->Add(m_b_add,    0, wxBOTTOM,  5);
  siz_buttons->Add(m_b_remove, 0, wxBOTTOM, 15);
  siz_buttons->Add(m_b_up,     0, wxBOTTOM,  5);
  siz_buttons->Add(m_b_down,   0, wxBOTTOM, 15);
  siz_buttons->Add(m_b_sort,   0, wxBOTTOM,  0);
  siz_buttons->AddStretchSpacer();
  siz_buttons->Add(m_b_close);

  wxBoxSizer *siz_controls = new wxBoxSizer(wxHORIZONTAL);
  siz_controls->Add(m_lv_files,  1, wxGROW);
  siz_controls->Add(siz_buttons, 0, wxLEFT, 5);

  siz_all->Add(siz_controls, 1, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);

  // Run

  SetSizer(siz_all);
  SetSize(wxSize(800, 500));

  enable_buttons();

  ShowModal();
}

void
additional_parts_dialog::on_add(wxCommandEvent &) {
}

void
additional_parts_dialog::on_remove(wxCommandEvent &) {
}

void
additional_parts_dialog::on_up(wxCommandEvent &) {
}

void
additional_parts_dialog::on_down(wxCommandEvent &) {
}

void
additional_parts_dialog::on_sort(wxCommandEvent &) {
}

void
additional_parts_dialog::on_item_selected(wxListEvent &) {
  enable_buttons();
}

void
additional_parts_dialog::on_close(wxCommandEvent &) {
  EndModal(0);
}

void
additional_parts_dialog::enable_buttons() {
  bool some_selected = 0 < m_lv_files->GetSelectedItemCount();

  m_b_remove->Enable(some_selected);
  m_b_up->Enable(some_selected);
  m_b_down->Enable(some_selected);
  m_b_sort->Enable(!m_files.empty());
}

std::vector<wxFileName>
additional_parts_dialog::get_file_names() {
  return std::vector<wxFileName>{};
}

IMPLEMENT_CLASS(additional_parts_dialog, wxDialog);
BEGIN_EVENT_TABLE(additional_parts_dialog, wxDialog)
  EVT_BUTTON(ID_ADDPARTS_B_ADD,                  additional_parts_dialog::on_add)
  EVT_BUTTON(ID_ADDPARTS_B_REMOVE,               additional_parts_dialog::on_remove)
  EVT_BUTTON(ID_ADDPARTS_B_UP,                   additional_parts_dialog::on_up)
  EVT_BUTTON(ID_ADDPARTS_B_DOWN,                 additional_parts_dialog::on_down)
  EVT_BUTTON(ID_ADDPARTS_B_SORT,                 additional_parts_dialog::on_sort)
  EVT_BUTTON(ID_ADDPARTS_B_CLOSE,                additional_parts_dialog::on_close)
  EVT_LIST_ITEM_SELECTED(ID_ADDPARTS_LV_FILES,   additional_parts_dialog::on_item_selected)
  EVT_LIST_ITEM_DESELECTED(ID_ADDPARTS_LV_FILES, additional_parts_dialog::on_item_selected)
END_EVENT_TABLE();
