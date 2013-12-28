/*
   mkvmerge GUI -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   scanning_for_playlists_dlg

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <wx/sizer.h>
#include <wx/stattext.h>

#include "common/fs_sys_helpers.h"
#include "common/strings/formatting.h"
#include "common/strings/parsing.h"
#include "mmg/mmg_dialog.h"
#include "mmg/tabs/scan_directory_thread.h"
#include "mmg/tabs/scanning_for_playlists_dlg.h"

scanning_for_playlists_dlg::scanning_for_playlists_dlg(wxWindow *parent,
                                                       wxString const &original_file_name,
                                                       wxArrayString const &original_output,
                                                       std::vector<wxString> const &other_file_names)
  : wxDialog{parent, wxID_ANY, Z("Scanning directory")}
#if defined(SYS_WINDOWS)
  , m_taskbar_progress{}
#endif  // SYS_WINDOWS
  , m_scanner{new scan_directory_thread_c{this, other_file_names}}
  , m_aborted{}
  , m_progress{}
  , m_max_progress{other_file_names.size()}
  , m_start_time{}
  , m_next_remaining_time_update{}
{
	m_st_progress           = new wxStaticText(this, wxID_ANY,    wxU(boost::format(Y("%1% of %2% file(s) processed")) % 0 % other_file_names.size()));
	m_g_progress            = new wxGauge(     this, wxID_ANY,    m_max_progress);
  auto st_remaining_label = new wxStaticText(this, wxID_ANY,    Z("Remaining time:"));
	m_st_remaining_time     = new wxStaticText(this, wxID_ANY,    Z("is being estimated"));
	m_b_abort               = new wxButton(    this, wxID_CANCEL, Z("&Abort"));

#if defined(SYS_WINDOWS)
  if (get_windows_version() >= WINDOWS_VERSION_7) {
    m_taskbar_progress = new taskbar_progress_c(mdlg);
    m_taskbar_progress->set_state(TBPF_NORMAL);
    m_taskbar_progress->set_value(0, other_file_names.size());
  }
#endif  // SYS_WINDOWS

  auto siz_remaining_time = new wxBoxSizer(wxHORIZONTAL);
  siz_remaining_time->Add(st_remaining_label,  0, wxALL, 5);
  siz_remaining_time->Add(m_st_remaining_time, 0, wxALL, 5);

	auto siz_button = new wxBoxSizer(wxHORIZONTAL);
	siz_button->AddStretchSpacer();
	siz_button->Add(m_b_abort, 0, wxALL, 5);
	siz_button->AddStretchSpacer();

	auto siz_all = new wxBoxSizer(wxVERTICAL);
	siz_all->Add(m_st_progress,      0, wxALL,            5);
	siz_all->Add(m_g_progress,       0, wxALL | wxEXPAND, 5);
	siz_all->Add(siz_remaining_time, 0, wxALL,            5);
	siz_all->Add(siz_button,         1, wxEXPAND,         5);

	SetSizerAndFit(siz_all);
  auto min_size = GetMinSize();
  SetSize(wxSize{min_size.GetWidth() + 50, min_size.GetHeight()});
	Layout();

	Centre(wxBOTH);

  parse_output(original_file_name, original_output);
}

scanning_for_playlists_dlg::~scanning_for_playlists_dlg()
{
  m_scanner->Wait();
  delete m_scanner;

#if defined(SYS_WINDOWS)
  if (m_taskbar_progress)
    m_taskbar_progress->set_state(TBPF_NOPROGRESS);
  delete m_taskbar_progress;
#endif  // SYS_WINDOWS
}

int
scanning_for_playlists_dlg::scan() {
  m_start_time                 = get_current_time_millis();
  m_next_remaining_time_update = m_start_time + 8000;

  m_scanner->Create();
  m_scanner->Run();
  ShowModal();

  if (m_aborted)
    return wxID_CANCEL;

  for (auto &pair : m_scanner->get_output())
    parse_output(pair.first, pair.second);

  brng::sort(m_playlists, [](playlist_file_cptr const &a, playlist_file_cptr const &b) { return a->file_name < b->file_name; });

  return wxID_OK;
}

void
scanning_for_playlists_dlg::on_abort(wxCommandEvent &) {
  m_aborted = true;
  m_scanner->abort();
}

void
scanning_for_playlists_dlg::on_scanner_finished(wxCommandEvent &) {
  EndModal(m_aborted ? wxID_CANCEL : wxID_OK);
}

void
scanning_for_playlists_dlg::on_progress_changed(wxCommandEvent &evt) {
  update_gauge(evt.GetInt());

  uint64_t now = get_current_time_millis();

  if ((0 == m_progress) || (now < m_next_remaining_time_update))
    return;

  int64_t total_time     = (now - m_start_time) * m_max_progress / m_progress;
  int64_t remaining_time = total_time - now + m_start_time;
  m_st_remaining_time->SetLabel(wxU(create_minutes_seconds_time_string(static_cast<unsigned int>(remaining_time / 1000))));

  m_next_remaining_time_update = now + 500;
}

void
scanning_for_playlists_dlg::update_gauge(size_t progress) {
  m_progress = progress;
  m_g_progress->SetValue(m_progress);
  m_st_progress->SetLabel(wxU(boost::format(Y("%1% of %2% file(s) processed")) % m_progress % m_max_progress));
#if defined(SYS_WINDOWS)
  if (m_taskbar_progress)
    m_taskbar_progress->set_value(m_progress, m_max_progress);
#endif  // SYS_WINDOWS
}

void
scanning_for_playlists_dlg::parse_output(wxString const &file_name,
                                         wxArrayString const &output) {
  uint64_t min_duration = mdlg->options.min_playlist_duration * 1000000000ull;
  auto playlist         = std::make_shared<playlist_file_t>(file_name, output);

  // mxinfo(boost::format("------------------------------------------------------------\n"));

  for (size_t idx = 0, count = output.GetCount(); idx < count; ++idx) {
    auto &line = output[idx];
    // mxinfo(boost::format("line: %1%\n") % to_utf8(line));
    if (line.StartsWith(wxT("File"))) {
      auto properties = parse_properties(line, wxT("container:"));

      // mxinfo(boost::format("  FILE! num pro: %1% num fi: %2%\n") % properties.first.size() % properties.second.size());

      if (!parse_number(to_utf8(properties.first[wxT("playlist_size")]), playlist->size))
        playlist->size = 0;

      if (!parse_number(to_utf8(properties.first[wxT("playlist_duration")]), playlist->duration))
        playlist->duration = 0;

      if (!parse_number(to_utf8(properties.first[wxT("playlist_chapters")]), playlist->chapters))
        playlist->chapters = 0;

      playlist->files = properties.second;

    } else if (line.StartsWith(wxT("Track"))) {
      auto properties = parse_properties(line, wxT("Track ID"));
      auto type       = line.AfterFirst(wxT(':')).Mid(1).BeforeFirst(wxT(' '));

      playlist->tracks.push_back(playlist_file_track_t{
          type == wxT("audio") ? Z("audio") : type == wxT("video") ? Z("video") : type == wxT("subtitles") ? Z("subtitles") : Z("unknown"),
          line.AfterFirst(wxT('(')).BeforeFirst(wxT(')')),
          properties.first[wxT("language")]
        });
    }
  }

  if (playlist->duration >= min_duration)
    m_playlists.push_back(playlist);
}

std::pair< std::map<wxString, wxString>,
           std::vector<wxString> >
scanning_for_playlists_dlg::parse_properties(wxString const &line,
                                             wxString const &wanted_content)
  const {
  std::map<wxString, wxString> properties;
  std::vector<wxString> files;
  wxString info;
  int pos_wanted, pos_properties;

  if (wxNOT_FOUND == (pos_wanted = line.Find(wanted_content)))
    return std::make_pair(properties, files);

  auto rest = line.Mid(pos_wanted + wanted_content.Length());
  if (wxNOT_FOUND != (pos_properties = rest.Find(wxT("["))))
    info = rest.Mid(pos_properties + 1).BeforeLast(wxT(']'));

  if (info.IsEmpty())
    return std::make_pair(properties, files);

  for (auto &arg : split(info, wxU(" "))) {
    auto pair = split(arg, wxU(":"), 2);
    if (pair[0] == wxT("playlist_file"))
      files.push_back(pair[1]);
    else
      properties[pair[0]] = unescape(pair[1]);
  }

  return std::make_pair(properties, files);
}

std::vector<playlist_file_cptr> const &
scanning_for_playlists_dlg::get_playlists()
  const {
  return m_playlists;
}

IMPLEMENT_CLASS(scanning_for_playlists_dlg, wxDialog);
BEGIN_EVENT_TABLE(scanning_for_playlists_dlg, wxDialog)
  EVT_BUTTON(wxID_CANCEL, scanning_for_playlists_dlg::on_abort)
  EVT_COMMAND(scan_directory_thread_c::progress_changed, scan_directory_thread_c::ms_event, scanning_for_playlists_dlg::on_progress_changed)
  EVT_COMMAND(scan_directory_thread_c::thread_finished,  scan_directory_thread_c::ms_event, scanning_for_playlists_dlg::on_scanner_finished)
END_EVENT_TABLE();
