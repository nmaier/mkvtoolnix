/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   select_scanned_file_dlg

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/wx.h>
#include <wx/filename.h>
#include <wx/statline.h>

#include "common/strings/formatting.h"
#include "common/wx.h"
#include "mmg/tabs/select_scanned_file_dlg.h"

select_scanned_file_dlg::select_scanned_file_dlg(wxWindow *parent,
                                                 std::vector<playlist_file_cptr> const &playlists,
                                                 wxString const &orig_file_name)
  : wxDialog{parent, wxID_ANY, Z("Select file to add"), wxDefaultPosition, wxSize{1000, 650}, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER | wxMINIMIZE_BOX | wxMAXIMIZE_BOX}
  , m_playlists{playlists}
  , m_selected_playlist_idx{0}
  , m_geometry_saver{this, "select_scanned_file_dlg"}
{
  // controls left column
	auto st_scanned_files = new wxStaticText(this, wxID_ANY, Z("Scanned files"));
	auto sl_left          = new wxStaticLine(this);
	m_lc_files            = new wxListCtrl(  this, ID_LC_PLAYLIST_FILE, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);

  // controls right column
	auto st_details        = new wxStaticText(this, wxID_ANY, Z("Details"));
	auto sl_right          = new wxStaticLine(this);
	auto st_duration       = new wxStaticText(this, wxID_ANY, Z("Duration:"));
	m_st_duration          = new wxStaticText(this, wxID_ANY, wxEmptyString);
	auto st_size           = new wxStaticText(this, wxID_ANY, Z("Size:"));
	m_st_size              = new wxStaticText(this, wxID_ANY, wxEmptyString);
	auto st_chapters       = new wxStaticText(this, wxID_ANY, Z("Number of chapters:"));
	m_st_chapters          = new wxStaticText(this, wxID_ANY, wxEmptyString);
	auto st_tracks         = new wxStaticText(this, wxID_ANY, Z("Tracks:"));
	m_lc_tracks            = new wxListCtrl(  this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
	auto st_playlist_items = new wxStaticText(this, wxID_ANY, Z("Playlist items:"));
	m_lc_items             = new wxListCtrl(  this, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);

  // button controls
	auto b_add    = new wxButton(this, wxID_OK,     Z("&Add"));
	auto b_cancel = new wxButton(this, wxID_CANCEL, Z("&Cancel"));
	b_add->SetDefault();

  // list controls
  wxListItem item;

  item.SetText(Z("File name"));
  m_lc_files->InsertColumn(0, item);
  item.SetText(Z("Duration"));
  m_lc_files->InsertColumn(1, item);
  item.SetText(Z("Size"));
  m_lc_files->InsertColumn(2, item);

  item.SetText(Z("Type"));
  m_lc_tracks->InsertColumn(0, item);
  item.SetText(Z("Codec"));
  m_lc_tracks->InsertColumn(1, item);
  item.SetText(Z("Language"));
  m_lc_tracks->InsertColumn(2, item);

  item.SetText(Z("File name"));
  m_lc_items->InsertColumn(0, item);
  item.SetText(Z("Directory"));
  m_lc_items->InsertColumn(1, item);

  // fill "files" with data
  m_lc_files->Hide();
  long idx = 0;
  for (auto &playlist : m_playlists) {
    auto id = m_lc_files->InsertItem(idx, wxFileName{playlist->file_name}.GetFullName());
    m_lc_files->SetItem(id, 1, wxU(format_timecode(playlist->duration, 0)));
    m_lc_files->SetItem(id, 2, wxU(format_file_size(playlist->size)));
    m_lc_files->SetItemData(id, idx);

    if (orig_file_name == playlist->file_name) {
      m_lc_files->SetItemState(id, wxLIST_STATE_SELECTED, wxLIST_STATE_SELECTED);
      m_selected_playlist_idx = idx;
    }

    ++idx;
  }

  m_lc_files->SetColumnWidth(0, wxLIST_AUTOSIZE);
  m_lc_files->SetColumnWidth(1, wxLIST_AUTOSIZE);
  m_lc_files->SetColumnWidth(2, wxLIST_AUTOSIZE);

  m_lc_files->Show();

  // layout
	auto siz_left_column = new wxBoxSizer(wxVERTICAL);
	siz_left_column->Add(st_scanned_files, 0, wxALL,            5);
	siz_left_column->Add(sl_left,          0, wxALL | wxEXPAND, 5);
	siz_left_column->Add(m_lc_files,       1, wxALL | wxEXPAND, 5);

	auto siz_info = new wxGridSizer(3, 2, 0, 0);
	siz_info->Add(st_duration,   0, wxALL, 5);
	siz_info->Add(m_st_duration, 0, wxALL, 5);
	siz_info->Add(st_size,       0, wxALL, 5);
	siz_info->Add(m_st_size,     0, wxALL, 5);
	siz_info->Add(st_chapters,   0, wxALL, 5);
	siz_info->Add(m_st_chapters, 0, wxALL, 5);

	auto siz_right_column = new wxBoxSizer(wxVERTICAL);
	siz_right_column->Add(st_details,        0, wxALL,            5);
	siz_right_column->Add(sl_right,          0, wxEXPAND | wxALL, 5);
	siz_right_column->Add(siz_info,          0, wxEXPAND,         5);
	siz_right_column->Add(st_tracks,         0, wxALL,            5);
	siz_right_column->Add(m_lc_tracks,       1, wxALL | wxEXPAND, 5);
	siz_right_column->Add(st_playlist_items, 0, wxALL,            5);
	siz_right_column->Add(m_lc_items,        1, wxALL | wxEXPAND, 5);

	auto siz_columns = new wxBoxSizer(wxHORIZONTAL);
	siz_columns->Add(siz_left_column,  1, wxEXPAND, 5);
	siz_columns->Add(siz_right_column, 1, wxEXPAND, 5);

	auto siz_buttons = new wxBoxSizer(wxHORIZONTAL);
	siz_buttons->AddStretchSpacer();
	siz_buttons->Add(b_add,    0, wxALL, 5);
	siz_buttons->AddStretchSpacer();
	siz_buttons->Add(b_cancel, 0, wxALL, 5);
	siz_buttons->AddStretchSpacer();

	auto siz_all = new wxBoxSizer(wxVERTICAL);
	siz_all->Add(siz_columns, 1, wxEXPAND, 5);
	siz_all->Add(siz_buttons, 0, wxEXPAND, 5);

	SetSizerAndFit(siz_all);
	Layout();

	Centre(wxBOTH);

  m_geometry_saver.set_default_size(1000, 650, true).restore();

  update_info();
}

select_scanned_file_dlg::~select_scanned_file_dlg() {
}

void
select_scanned_file_dlg::on_ok(wxCommandEvent &) {
  EndModal(wxID_OK);
}

void
select_scanned_file_dlg::on_cancel(wxCommandEvent &) {
  EndModal(wxID_CANCEL);
}

void
select_scanned_file_dlg::on_file_selected(wxListEvent &) {
  auto selected = m_lc_files->GetNextItem(-1, wxLIST_NEXT_ALL, wxLIST_STATE_SELECTED);
  if (-1 == selected)
    return;

  m_selected_playlist_idx = m_lc_files->GetItemData(selected);
  update_info();
}

void
select_scanned_file_dlg::on_file_activated(wxListEvent &evt) {
  on_file_selected(evt);
  EndModal(wxID_OK);
}

int
select_scanned_file_dlg::get_selected_playlist_idx()
  const {
  return m_selected_playlist_idx;
}

void
select_scanned_file_dlg::update_info() {
  wxListItem item;

  auto &playlist = *m_playlists[m_selected_playlist_idx];

  m_st_duration->SetLabel(wxU(format_timecode(playlist.duration, 0)));
  m_st_size->SetLabel(wxU(format_file_size(playlist.size)));
  m_st_chapters->SetLabel(wxU(boost::format("%1%") % playlist.chapters));

  m_lc_tracks->Hide();
  m_lc_tracks->DeleteAllItems();
  long idx = 0;
  for (auto &track : playlist.tracks) {
    auto id = m_lc_tracks->InsertItem(idx, track.type);
    m_lc_tracks->SetItem(id, 1, track.codec);
    m_lc_tracks->SetItem(id, 2, wxU(boost::format("%|1$ -15s|") % to_utf8(track.language)));
    ++idx;
  }

  m_lc_tracks->SetColumnWidth(0, wxLIST_AUTOSIZE);
  m_lc_tracks->SetColumnWidth(1, wxLIST_AUTOSIZE);
  m_lc_tracks->SetColumnWidth(2, wxLIST_AUTOSIZE);

  m_lc_tracks->Show();

  m_lc_items->Hide();
  m_lc_items->DeleteAllItems();
  idx = 0;
  for (auto &file : playlist.files) {
    auto id = m_lc_items->InsertItem(idx, wxFileName{file}.GetFullName());
    m_lc_items->SetItem(id, 1, wxFileName{file}.GetPath());
  }

  m_lc_items->SetColumnWidth(0, wxLIST_AUTOSIZE);
  m_lc_items->SetColumnWidth(1, wxLIST_AUTOSIZE);

  m_lc_items->Show();
}

IMPLEMENT_CLASS(select_scanned_file_dlg, wxDialog);
BEGIN_EVENT_TABLE(select_scanned_file_dlg, wxDialog)
  EVT_BUTTON(wxID_OK,     select_scanned_file_dlg::on_ok)
  EVT_BUTTON(wxID_CANCEL, select_scanned_file_dlg::on_cancel)
  EVT_LIST_ITEM_SELECTED( ID_LC_PLAYLIST_FILE, select_scanned_file_dlg::on_file_selected)
  EVT_LIST_ITEM_ACTIVATED(ID_LC_PLAYLIST_FILE, select_scanned_file_dlg::on_file_activated)
END_EVENT_TABLE();
