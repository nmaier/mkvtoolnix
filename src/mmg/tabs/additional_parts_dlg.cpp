/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   "additional parts" dialog

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/wx.h>
#include <wx/statline.h>

#include "common/sorting.h"
#include "common/strings/formatting.h"
#include "mmg/mmg.h"
#include "mmg/tabs/additional_parts_dlg.h"

bool
operator <(wxFileName const &a,
           wxFileName const &b) {
  return a.GetFullPath() < b.GetFullPath();
}

inline std::wstring
to_wide(wxFileName const &source) {
  return to_wide(source.GetFullPath());
}

additional_parts_dialog::additional_parts_dialog(wxWindow *parent,
                                                 mmg_file_t const &file)
  : wxDialog(parent, -1, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
  , m_file{&file}
  , m_primary_file_name{m_file->file_name}
  , m_files{ m_file->is_playlist ? m_file->playlist_files : m_file->other_files }
{
  SetTitle(m_file->is_playlist ? Z("View files in playlist") : Z("Additional source file parts"));

  // Create controls
  m_lv_files                 = new wxListView(this, ID_ADDPARTS_LV_FILES, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxSUNKEN_BORDER);

  m_b_add                    = new wxButton(this, ID_ADDPARTS_B_ADD,    Z("&Add"));
  m_b_remove                 = new wxButton(this, ID_ADDPARTS_B_REMOVE, Z("&Remove"));
  m_b_up                     = new wxButton(this, ID_ADDPARTS_B_UP,     Z("&Up"));
  m_b_down                   = new wxButton(this, ID_ADDPARTS_B_DOWN,   Z("&Down"));
  m_b_sort                   = new wxButton(this, ID_ADDPARTS_B_SORT,   Z("&Sort"));
  m_b_close                  = new wxButton(this, ID_ADDPARTS_B_CLOSE,  Z("&Close"));

  auto *st_title             = new wxStaticText(this, wxID_ANY, m_file->is_playlist ? Z("View files in playlist") : Z("Edit additional source file parts"));
  auto *st_primary_file_name = new wxStaticText(this, wxID_ANY, Z("Primary file name:"));
  auto *st_directory         = new wxStaticText(this, wxID_ANY, Z("Directory:"));

  wxString text;
  if (m_file->is_playlist) {
    text = wxString( Z("The following list shows the files that make up this playlist.") )
         + wxT(" ")
         + Z("This list is for informational purposes only and cannot be changed.");

  } else {
    text = wxString( Z("The following parts are read after the primary file as if they were all part of one big file.") )
         + wxT(" ")
         + Z("Typical use cases include reading VOBs from a DVD (e.g. VTS_01_1.VOB, VTS_01_2.VOB, VTS_01_3.VOB).");
  }

  auto *st_additional_parts  = new wxStaticText(this, wxID_ANY, wxString{ format_paragraph(to_wide(text), 0, L"", L"", 80) }.Strip());

  auto *sl_title             = new wxStaticLine(this);

  auto *tc_primary_file_name = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);
  auto *tc_directory         = new wxTextCtrl(this, wxID_ANY, wxEmptyString, wxDefaultPosition, wxDefaultSize, wxTE_READONLY);

  // Setup controls

  wxListItem column_item;
  column_item.SetMask(wxLIST_MASK_TEXT);

  column_item.SetText(Z("File name"));
  m_lv_files->InsertColumn(0, column_item);
  column_item.SetText(Z("Directory"));
  m_lv_files->InsertColumn(1, column_item);

  auto dummy = m_lv_files->InsertItem(0, wxT("some long file name dummy.mkv"));
  m_lv_files->SetItem(dummy, 1, wxT("and the path is even longer but hey such is life"));
  m_lv_files->SetColumnWidth(0, wxLIST_AUTOSIZE);
  m_lv_files->SetColumnWidth(1, wxLIST_AUTOSIZE);
  m_lv_files->DeleteItem(0);

  repopulate();

  tc_primary_file_name->SetValue(m_primary_file_name.GetFullName());
  tc_directory->SetValue(m_primary_file_name.GetPath());

  enable_buttons();

  // Create layout

  auto *siz_all = new wxBoxSizer(wxVERTICAL);
  siz_all->Add(st_title, 0, wxALL,                                5);
  siz_all->Add(sl_title, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);

  auto *siz_primary_file_name = new wxFlexGridSizer(2, 2, 5, 5);
  siz_primary_file_name->AddGrowableCol(1);
  siz_primary_file_name->Add(st_primary_file_name);
  siz_primary_file_name->Add(tc_primary_file_name, 0, wxGROW);
  siz_primary_file_name->Add(st_directory);
  siz_primary_file_name->Add(tc_directory,         0, wxGROW);

  siz_all->Add(siz_primary_file_name, 0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);
  siz_all->Add(st_additional_parts,   0, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);

  auto *siz_buttons = new wxBoxSizer(wxVERTICAL);
  siz_buttons->Add(m_b_add,    0, wxBOTTOM,  5);
  siz_buttons->Add(m_b_remove, 0, wxBOTTOM, 15);
  siz_buttons->Add(m_b_up,     0, wxBOTTOM,  5);
  siz_buttons->Add(m_b_down,   0, wxBOTTOM, 15);
  siz_buttons->Add(m_b_sort,   0, wxBOTTOM,  0);
  siz_buttons->AddStretchSpacer();
  siz_buttons->Add(m_b_close);

  auto *siz_controls = new wxBoxSizer(wxHORIZONTAL);
  siz_controls->Add(m_lv_files,  1, wxGROW);
  siz_controls->Add(siz_buttons, 0, wxGROW | wxLEFT, 5);

  siz_all->Add(siz_controls, 1, wxGROW | wxLEFT | wxRIGHT | wxBOTTOM, 5);

  SetSizerAndFit(siz_all);
  siz_all->SetSizeHints(this);
  SetSize(wxSize(700, 400));

  // Run

  ShowModal();
}

void
additional_parts_dialog::repopulate() {
  m_lv_files->DeleteAllItems();

  for (size_t idx = 0; m_files.size() > idx; ++idx)
    create_item(idx);
}

void
additional_parts_dialog::create_item(size_t idx) {
  auto item = m_lv_files->InsertItem(idx, m_files[idx].GetFullName());
  m_lv_files->SetItem(item, 1, m_files[idx].GetPath());
}

void
additional_parts_dialog::on_add(wxCommandEvent &) {
  wxString wildcard = wxString::Format(wxT("*.%s|*.%s|%s"), m_primary_file_name.GetExt().c_str(), m_primary_file_name.GetExt().c_str(), wxString{ ALLFILES }.c_str());
  wxFileDialog dlg(this, Z("Add additional parts"), m_primary_file_name.GetPath(), wxEmptyString, wildcard, wxFD_OPEN | wxFD_FILE_MUST_EXIST | wxFD_MULTIPLE);
  if (wxID_OK != dlg.ShowModal())
    return;

  wxArrayString paths;
  dlg.GetPaths(paths);
  if (paths.IsEmpty())
    return;

  m_lv_files->Freeze();

  std::map<wxFileName, bool> exists{ { m_primary_file_name, true } };
  for (auto &file_name : m_files)
    exists[file_name] = true;

  std::vector<wxFileName> file_names_to_add;
  for (size_t idx = 0; paths.GetCount() > idx; ++idx) {
    wxFileName file_name{ paths[idx] };
    if (!exists[file_name])
      file_names_to_add.push_back(file_name);
  }

  mtx::sort::naturally(file_names_to_add.begin(), file_names_to_add.end());

  for (auto &file_name : file_names_to_add) {
    m_files.push_back(file_name);
    create_item(m_files.size() - 1);
  }

  m_lv_files->Thaw();

  enable_buttons();
}

void
additional_parts_dialog::on_remove(wxCommandEvent &) {
  if (m_files.empty())
    return;

  m_lv_files->Freeze();

  for (int idx = m_files.size() - 1; 0 <= idx; idx--)
    if (m_lv_files->IsSelected(idx)) {
      m_files.erase(m_files.begin() + idx, m_files.begin() + idx + 1);
      m_lv_files->DeleteItem(idx);
    }

  m_lv_files->Thaw();

  enable_buttons();
}

void
additional_parts_dialog::on_up(wxCommandEvent &) {
  if (1 >= m_files.size())
    return;

  save_selection([&]() {
      // Cannot use std::swap() on vector<bool> members due to specialization.
      std::vector<char> selected;
      for (size_t idx = 0; m_files.size() > idx; ++idx)
        selected.push_back(m_lv_files->IsSelected(idx));

      for (size_t idx = 1; m_files.size() > idx; ++idx)
        if (selected[idx] && !selected[idx - 1]) {
          std::swap(m_files[idx - 1],  m_files[idx]);
          std::swap(selected[idx - 1], selected[idx]);
        }

      repopulate();
    });
}

void
additional_parts_dialog::on_down(wxCommandEvent &) {
  if (1 >= m_files.size())
    return;

  save_selection([&]() {
      // Cannot use std::swap() on vector<bool> members due to specialization.
      std::vector<char> selected;
      for (size_t idx = 0; m_files.size() > idx; ++idx)
        selected.push_back(m_lv_files->IsSelected(idx));

      for (int idx = m_files.size() - 2; 0 <= idx; --idx)
        if (selected[idx] && !selected[idx + 1]) {
          std::swap(m_files[idx + 1],  m_files[idx]);
          std::swap(selected[idx + 1], selected[idx]);
        }

      repopulate();
    });
}

void
additional_parts_dialog::on_sort(wxCommandEvent &) {
  save_selection([&]() {
      mtx::sort::naturally(m_files.begin(), m_files.end());
      repopulate();
    });
}

void
additional_parts_dialog::on_close(wxCommandEvent &) {
  EndModal(0);
}

void
additional_parts_dialog::on_item_selected(wxListEvent &) {
  enable_buttons();
}

void
additional_parts_dialog::save_selection(std::function<void()> worker) {
  m_lv_files->Freeze();

  std::map<wxFileName, bool> selected;
  for (size_t idx = 0; m_files.size() > idx; ++idx)
    selected[m_files[idx]] = m_lv_files->IsSelected(idx);

  worker();

  for (size_t idx = 0; m_files.size() > idx; ++idx)
    m_lv_files->Select(idx, selected[m_files[idx]]);

  m_lv_files->Thaw();
}

void
additional_parts_dialog::enable_buttons() {
  bool some_selected = 0 < m_lv_files->GetSelectedItemCount();

  m_b_add->Enable(   !m_file->is_playlist);
  m_b_remove->Enable(!m_file->is_playlist && some_selected);
  m_b_up->Enable(    !m_file->is_playlist && some_selected);
  m_b_down->Enable(  !m_file->is_playlist && some_selected);
  m_b_sort->Enable(  !m_file->is_playlist && !m_files.empty());
}

std::vector<wxFileName> const &
additional_parts_dialog::get_file_names() {
  return m_files;
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
