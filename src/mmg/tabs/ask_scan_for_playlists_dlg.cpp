/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   ask_scan_for_playlists_dlg

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/sizer.h>
#include <wx/stattext.h>

#include "mmg/mmg_dialog.h"
#include "mmg/tabs/ask_scan_for_playlists_dlg.h"

ask_scan_for_playlists_dlg::ask_scan_for_playlists_dlg(wxWindow *parent,
                                                       size_t num_other_files)
  : wxDialog{parent, wxID_ANY, Z("Scan directory for other playlists")}
{
	SetSize(500, 150);

	auto st_question = new wxStaticText(this, wxID_ANY,
                                      wxU(boost::format(Y("The file you've added is a playlist. The directory it is located it contains %1% other file(s) with the same extension. "
                                                          "mmg can scan these files, present the results including duration and number of tracks of each playlist found and let you chose which one to add."))
                                          % num_other_files));
	st_question->Wrap(480);

	auto st_in_the_future = new wxStaticText(this, wxID_ANY, Z("What to do in the future:"), wxDefaultPosition, wxDefaultSize, 0);

  wxString const scan_directory_for_playlists_choices[] = {
    Z("always ask the user"),
    Z("always scan for other playlists"),
    Z("never scan for other playlists"),
  };
  m_cob_in_the_future = new wxMTX_COMBOBOX_TYPE(this, wxID_ANY, Z("always ask the user"), wxDefaultPosition, wxDefaultSize, 3, scan_directory_for_playlists_choices, wxCB_READONLY);
	m_cob_in_the_future->SetSelection(mdlg->options.scan_directory_for_playlists);


	auto b_scan      = new wxButton(this, wxID_OK,     Z("&Scan for other playlists"));
	auto b_dont_scan = new wxButton(this, wxID_CANCEL, Z("&Don't scan, just add the file"));
  b_scan->SetDefault();

	auto siz_in_the_future = new wxBoxSizer(wxHORIZONTAL);
	siz_in_the_future->Add(st_in_the_future,    0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
	siz_in_the_future->Add(m_cob_in_the_future, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

	auto siz_buttons = new wxBoxSizer(wxHORIZONTAL);
	siz_buttons->AddStretchSpacer();
	siz_buttons->Add(b_scan);
	siz_buttons->AddStretchSpacer();
	siz_buttons->Add(b_dont_scan);
	siz_buttons->AddStretchSpacer();

	auto siz_all = new wxBoxSizer(wxVERTICAL);
	siz_all->Add(st_question,       0, wxALL,    5);
	siz_all->Add(siz_in_the_future, 1, wxEXPAND, 5);
	siz_all->Add(siz_buttons,       1, wxEXPAND, 5);

	SetSizer(siz_all);
	Layout();

	Centre(wxBOTH);
}

ask_scan_for_playlists_dlg::~ask_scan_for_playlists_dlg() {
  mdlg->options.scan_directory_for_playlists = static_cast<scan_directory_for_playlists_e>(m_cob_in_the_future->GetSelection());
}
