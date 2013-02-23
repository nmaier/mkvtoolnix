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

#include "common/fs_sys_helpers.h"
#include "common/mm_io_x.h"
#include "common/strings/formatting.h"
#include "mmg/mmg.h"
#include "mmg/mmg_dialog.h"
#include "mmg/tabs/scan_directory_thread.h"
#include "mmg/tabs/scanning_for_playlists_dlg.h"

wxEventType const scan_directory_thread_c::ms_event{wxNewEventType()};

scan_directory_thread_c::scan_directory_thread_c(scanning_for_playlists_dlg *dlg,
                                                 std::vector<wxString> const &file_names)
  : wxThread{wxTHREAD_JOINABLE}
  , m_dlg{dlg}
  , m_file_names{file_names}
  , m_abort{false}
{
}

void *
scan_directory_thread_c::Entry() {
  int num_processed = 0;

  for (auto &file_name : m_file_names) {
    if (m_abort)
      break;

    auto result = identify_file(file_name);
    if (0 == result.first)
      m_output.push_back(std::make_pair(file_name, result.second));

    ++num_processed;
    wxCommandEvent evt(scan_directory_thread_c::ms_event, scan_directory_thread_c::progress_changed);
    evt.SetInt(num_processed);
    wxPostEvent(m_dlg, evt);
  }

  wxCommandEvent evt(scan_directory_thread_c::ms_event, scan_directory_thread_c::thread_finished);
  wxPostEvent(m_dlg, evt);

  return nullptr;
}

void
scan_directory_thread_c::abort() {
  m_abort = true;
}

std::vector< std::pair<wxString, wxArrayString> > const &
scan_directory_thread_c::get_output()
  const {
  return m_output;
}

std::pair<int, wxArrayString>
scan_directory_thread_c::identify_file(wxString const &file_name)
  const {
  wxArrayString output;

  auto opt_file_name = wxU(boost::format("%1%mmg-mkvmerge-options-%2%-%3%") % to_utf8(get_temp_dir()) % wxGetProcessId() % get_current_time_millis());

  try {
    const unsigned char utf8_bom[3] = {0xef, 0xbb, 0xbf};
    mm_file_io_c opt_file{to_utf8(opt_file_name), MODE_CREATE};

    opt_file.write(utf8_bom, 3);
    opt_file.write((boost::format("--output-charset\nUTF-8\n--identify-for-mmg\n%1%\n--redirect-output\n%2%-out\n") % escape(to_utf8(file_name)) % escape(to_utf8(opt_file_name))).str());

  } catch (mtx::mm_io::exception &) {
    return std::make_pair(-1, output);
  }

  auto command = wxString::Format(wxT("\"%s\" \"@%s\""), mdlg->options.mkvmerge_exe().c_str(), opt_file_name.c_str());

  // log_it(boost::format("command: %1%\n") % to_utf8(command));

  int result = mtx::system(to_utf8(command));

  try {
    mm_text_io_c in{new mm_file_io_c{to_utf8(opt_file_name + wxT("-out")), MODE_READ}};
    std::string line;
    while (in.getline2(line))
      output.Add(wxU(line));

  } catch (mtx::mm_io::exception &ex) {
    // log_it(boost::format("ex: %1%\n") % ex.what());
    result = -2;
    output.Clear();
  }

  // log_it(boost::format("result: %1%, outpu: %2%\n") % result % output.Count());

  wxRemoveFile(opt_file_name);
  wxRemoveFile(opt_file_name + wxT("-out"));

  return std::make_pair(result, output);
}
