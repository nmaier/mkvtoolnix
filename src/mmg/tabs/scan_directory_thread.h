/*
  mkvmerge GUI -- utility for splicing together matroska files
  from component media subtypes

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html

  scan_directory_thread_c

  Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_MMG_TABS_SCAN_DIRECTORY_THREAD_H
#define MTX_MMG_TABS_SCAN_DIRECTORY_THREAD_H

#include "common/common_pch.h"

#include <wx/arrstr.h>
#include <wx/event.h>
#include <wx/string.h>
#include <wx/thread.h>

class scanning_for_playlists_dlg;

class scan_directory_thread_c: public wxThread {
private:
  scanning_for_playlists_dlg *m_dlg;
  std::vector<wxString> m_file_names;
  bool m_abort;

  std::vector< std::pair<wxString, wxArrayString> > m_output;

public:
  static wxEventType const ms_event;
  enum event_type_e {
    thread_finished = 1,
    progress_changed,
  };

public:
  scan_directory_thread_c(scanning_for_playlists_dlg *dlg, std::vector<wxString> const &file_names);

  virtual void *Entry();
  void abort();

  std::vector< std::pair<wxString, wxArrayString> > const &get_output() const;

protected:
  std::pair<int, wxArrayString> identify_file(wxString const &file_name) const;
};

#endif // MTX_MMG_TABS_SCAN_DIRECTORY_THREAD_H
