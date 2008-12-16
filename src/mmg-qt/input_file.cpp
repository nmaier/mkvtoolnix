#include "input_file.h"

input_track_c::input_track_c()
  : m_tid(0)
  , m_enabled(true)
{
}

input_track_c::~input_track_c() {
}

void
input_track_c::save_settings(QSettings &settings) {
}

void
input_track_c::load_settings(QSettings &settings) {
}

// ----------------------------------------------------------------------

input_file_c::input_file_c()
  : m_no_attachments(false)
  , m_no_chapters(false)
  , m_no_tags(false)
{
}

input_file_c::~input_file_c() {
}

void
input_file_c::save_settings(QSettings &settings) {
  int i;
  for (i = 0; m_tracks.size() > i; ++i)
    m_tracks[i]->save_settings(settings);
}

void
input_file_c::load_settings(QSettings &settings) {
  int i;
  for (i = 0; m_tracks.size() > i; ++i)
    m_tracks[i]->load_settings(settings);
}
