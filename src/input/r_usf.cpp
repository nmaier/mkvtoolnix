/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   USF subtitle reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <sstream>

#include "common/iso639.h"
#include "common/matroska.h"
#include "common/mm_io.h"
#include "common/strings/parsing.h"
#include "common/xml/ebml_converter.h"
#include "input/r_usf.h"
#include "merge/output_control.h"
#include "merge/pr_generic.h"
#include "output/p_textsubs.h"

int
usf_reader_c::probe_file(mm_text_io_c *in,
                         uint64_t) {
  try {
    auto doc = mtx::xml::load_file(in->get_file_name());
    return doc && std::string{ doc->document_element().name() } == "USFSubtitles" ? 1 : 0;

  } catch(...) {
  }

  return 0;
}

usf_reader_c::usf_reader_c(const track_info_c &ti,
                           const mm_io_cptr &in)
  : generic_reader_c(ti, in)
{
}

void
usf_reader_c::read_headers() {
  try {
    auto doc = mtx::xml::load_file(m_in->get_file_name(), pugi::parse_default | pugi::parse_declaration | pugi::parse_doctype | pugi::parse_pi | pugi::parse_comments);

    parse_metadata(doc);
    parse_subtitles(doc);
    create_codec_private(doc);

    for (auto track : m_tracks) {
      brng::stable_sort(track->m_entries);
      track->m_current_entry = track->m_entries.begin();
      if (!m_longest_track || (m_longest_track->m_entries.size() < track->m_entries.size()))
        m_longest_track = track;

      if ((m_default_language != "") && (track->m_language == ""))
        track->m_language = m_default_language;
    }

  } catch (mtx::xml::exception &ex) {
    throw mtx::input::extended_x(ex.what());

  } catch (mtx::mm_io::exception &) {
    throw mtx::input::open_x();
  }

  show_demuxer_info();
}

usf_reader_c::~usf_reader_c() {
}

void
usf_reader_c::parse_metadata(mtx::xml::document_cptr &doc) {
  auto attribute = doc->document_element().child("metadata").child("language").attribute("code");
  if (attribute && !std::string{attribute.value()}.empty()) {
    int index = map_to_iso639_2_code(attribute.value());
    if (-1 != index)
      m_default_language = iso639_languages[index].iso639_2_code;
    else if (!g_identifying)
      mxwarn_fn(m_ti.m_fname, boost::format(Y("The default language code '%1%' is not a valid ISO639-2 language code and will be ignored.\n")) % attribute.value());
  }
}

void
usf_reader_c::parse_subtitles(mtx::xml::document_cptr &doc) {
  for (auto subtitles = doc->document_element().child("subtitles"); subtitles; subtitles = subtitles.next_sibling("subtitles")) {
    auto track = std::make_shared<usf_track_t>();
    m_tracks.push_back(track);

    auto attribute = subtitles.child("language").attribute("code");
    if (attribute && !std::string{attribute.value()}.empty()) {
      int index = map_to_iso639_2_code(attribute.value());
      if (-1 != index)
        track->m_language = iso639_languages[index].iso639_2_code;
      else if (!g_identifying)
        mxwarn_tid(m_ti.m_fname, m_tracks.size() - 1, boost::format(Y("The language code '%1%' is not a valid ISO639-2 language code and will be ignored.\n")) % attribute.value());
    }

    for (auto subtitle = subtitles.child("subtitle"); subtitle; subtitle = subtitle.next_sibling("subtitle")) {
      usf_entry_t entry;
      int64_t duration = -1;

      attribute = subtitle.attribute("start");
      if (attribute)
        entry.m_start = try_to_parse_timecode(attribute.value());

      attribute = subtitle.attribute("stop");
      if (attribute)
        entry.m_end = try_to_parse_timecode(attribute.value());

      attribute = subtitle.attribute("duration");
      if (attribute)
        duration = try_to_parse_timecode(attribute.value());

      if ((-1 == entry.m_end) && (-1 != entry.m_start) && (-1 != duration))
        entry.m_end = entry.m_start + duration;

      std::stringstream out;
      for (auto node : subtitle)
        node.print(out, "", pugi::format_default | pugi::format_raw);
      entry.m_text = out.str();

      track->m_entries.push_back(entry);
    }
  }
}

void
usf_reader_c::create_codec_private(mtx::xml::document_cptr &doc) {
  auto root = doc->document_element();
  while (auto subtitles = root.child("subtitles"))
    root.remove_child(subtitles);

  std::stringstream out;
  doc->save(out, "", pugi::format_default | pugi::format_raw);
  m_private_data = out.str();
}

void
usf_reader_c::create_packetizer(int64_t tid) {
  if ((0 > tid) || (m_tracks.size() <= static_cast<size_t>(tid)))
    return;

  auto track = m_tracks[tid];

  if (!demuxing_requested('s', tid) || (-1 != track->m_ptzr))
    return;

  m_ti.m_language = track->m_language;
  track->m_ptzr   = add_packetizer(new textsubs_packetizer_c(this, m_ti, MKV_S_TEXTUSF, m_private_data.c_str(), m_private_data.length(), false, true));
  show_packetizer_info(tid, PTZR(track->m_ptzr));
}

void
usf_reader_c::create_packetizers() {
  size_t i;

  for (i = 0; m_tracks.size() > i; ++i)
    create_packetizer(i);
}

file_status_e
usf_reader_c::read(generic_packetizer_c *ptzr,
                   bool) {
  auto track_itr = brng::find_if(m_tracks, [&](usf_track_cptr &tr) { return (-1 != tr->m_ptzr) && (PTZR(tr->m_ptzr) == ptzr); });
  if (track_itr == m_tracks.end())
    return FILE_STATUS_DONE;

  auto track = *track_itr;

  if (track->m_entries.end() == track->m_current_entry)
    return flush_packetizer(track->m_ptzr);

  const usf_entry_t &entry = *(track->m_current_entry);
  // A length of 0 here is OK because the text subtitle packetizer assumes
  // that the data is a zero-terminated string.
  memory_cptr mem(new memory_c((unsigned char *)entry.m_text.c_str(), 0, false));
  PTZR(track->m_ptzr)->process(new packet_t(mem, entry.m_start, entry.m_end - entry.m_start));
  ++(track->m_current_entry);

  if (track->m_entries.end() == track->m_current_entry)
    return flush_packetizer(track->m_ptzr);

  return FILE_STATUS_MOREDATA;
}

int
usf_reader_c::get_progress() {
  if (!m_longest_track || m_longest_track->m_entries.empty())
    return 0;
  return 100 - std::distance(m_longest_track->m_current_entry, std::vector<usf_entry_t>::const_iterator(m_longest_track->m_entries.end())) * 100 / m_longest_track->m_entries.size();
}

int64_t
usf_reader_c::try_to_parse_timecode(const char *s) {
  int64_t timecode;

  if (!parse_timecode(s, timecode))
    throw mtx::xml::conversion_x{Y("Invalid start or stop timecode")};

  return timecode;
}

void
usf_reader_c::identify() {
  std::vector<std::string> verbose_info;
  size_t i;

  id_result_container();

  for (i = 0; m_tracks.size() > i; ++i) {
    auto track = m_tracks[i];

    verbose_info.clear();

    if (!track->m_language.empty())
      verbose_info.push_back(std::string("language:") + escape(track->m_language));

    id_result_track(i, ID_RESULT_TRACK_SUBTITLES, "USF", verbose_info);
  }
}
