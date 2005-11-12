/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   USF subtitle reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <errno.h>
#include <expat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include <algorithm>

#include "iso639.h"
#include "matroska.h"
#include "mm_io.h"
#include "output_control.h"
#include "pr_generic.h"
#include "p_textsubs.h"
#include "r_usf.h"

using namespace std;

class usf_xml_find_root_c: public xml_parser_c {
public:
  string m_root_element;

public:
  usf_xml_find_root_c(mm_text_io_c *io):
    xml_parser_c(io) {
  }

  virtual void start_element_cb(const char *name,
                                const char **atts) {
    if (m_root_element == "")
      m_root_element = name;
  }
};

int
usf_reader_c::probe_file(mm_text_io_c *io,
                         int64_t) {
  try {
    usf_xml_find_root_c root_finder(io);

    io->setFilePointer(0);
    while (root_finder.parse_one_xml_line() &&
           (root_finder.m_root_element == ""))
      ;

    return (root_finder.m_root_element == "USFSubtitles" ? 1 : 0);

  } catch(...) {
  }

  return 0;
}

usf_reader_c::usf_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti),
  m_copy_depth(0), m_longest_track(-1), m_strip(false) {

  try {
    m_xml_source = new mm_text_io_c(new mm_file_io_c(ti.fname));
    string line;
    int i;

    if (!usf_reader_c::probe_file(m_xml_source, 0))
      throw error_c("usf_reader: Source is not a valid USF file.");

    parse_xml_file();
    m_private_data += "</USFSubtitles>";

    delete m_xml_source;

    for (i = 0; m_tracks.size() > i; ++i) {
      stable_sort(m_tracks[i].m_entries.begin(), m_tracks[i].m_entries.end());
      m_tracks[i].m_current_entry = m_tracks[i].m_entries.begin();
      if ((-1 == m_longest_track) ||
          (m_tracks[m_longest_track].m_entries.size() <
           m_tracks[i].m_entries.size()))
        m_longest_track = i;

      if ((m_default_language != "") && (m_tracks[i].m_language == ""))
        m_tracks[i].m_language = m_default_language;
    }

  } catch (xml_parser_error_c &error) {
    throw error_c(error.get_error());

  } catch (mm_io_error_c &error) {
    throw error_c("usf_reader: Could not open the source file.");
  }

  if (verbose)
    mxinfo(FMT_FN "Using the USF subtitle reader.\n", ti.fname.c_str());
}

usf_reader_c::~usf_reader_c() {
}

void
usf_reader_c::start_element_cb(const char *name,
                               const char **atts) {
  int i;
  string node;

  m_previous_start = name;
  m_parents.push_back(name);

  // Generate the full path to this node.
  for (i = 0; m_parents.size() > i; ++i) {
    if (!node.empty())
      node += ".";
    node += m_parents[i];
  }

  if (node == "USFSubtitles.metadata.language") {
    for (i = 0; (NULL != atts[i]) && (NULL != atts[i + 1]); i += 2)
      if (!strcmp(atts[i], "code") && (0 != atts[i + 1][0])) {
        if (is_valid_iso639_2_code(atts[i + 1]))
          m_default_language = atts[i + 1];
        else if (!identifying)
          mxwarn(FMT_FN "The default language code '%s' is not a valid "
                 "ISO639-2 language code and will be ignored.\n",
                 ti.fname.c_str(), atts[i + 1]);
        break;
      }
  }

  if (0 != m_copy_depth) {
    // Just copy the data.
    if (m_strip)
      strip(m_data_buffer);
    m_copy_buffer += escape_xml(m_data_buffer) +
      create_xml_node_name(name, atts);
    ++m_copy_depth;
    m_data_buffer = "";

    return;
  }

  if (node == "USFSubtitles")
    m_private_data += create_xml_node_name(name, atts);

  else if (node == "USFSubtitles.subtitles") {
    usf_track_t new_track;
    m_tracks.push_back(new_track);

  } else if (node == "USFSubtitles.subtitles.language") {
    for (i = 0; (NULL != atts[i]) && (NULL != atts[i + 1]); i += 2)
      if (!strcmp(atts[i], "code") && (0 != atts[i + 1][0])) {
        if (is_valid_iso639_2_code(atts[i + 1]))
          m_tracks[m_tracks.size() - 1].m_language = atts[i + 1];
        else if (!identifying)
          mxwarn(FMT_TID "The language code '%s' is not a valid "
                 "ISO639-2 language code and will be ignored.\n",
                 ti.fname.c_str(), (int64_t)m_tracks.size(), atts[i + 1]);
        break;
      }

  } else if (node == "USFSubtitles.subtitles.subtitle") {
    usf_entry_t entry;
    int64_t duration;

    duration = -1;
    for (i = 0; (NULL != atts[i]) && (NULL != atts[i + 1]); i += 2)
      if (!strcmp(atts[i], "start"))
        entry.m_start = try_to_parse_timecode(atts[i + 1]);
      else if (!strcmp(atts[i], "stop"))
        entry.m_end = try_to_parse_timecode(atts[i + 1]);
      else if (!strcmp(atts[i], "duration"))
        duration = try_to_parse_timecode(atts[i + 1]);
    if ((-1 == entry.m_end) && (-1 != entry.m_start) && (-1 != duration))
      entry.m_end = entry.m_start + duration;
    m_copy_buffer = "";
    m_copy_depth = 1;

    m_tracks[m_tracks.size() - 1].m_entries.push_back(entry);

  } else if (m_parents.size() == 2) {
    m_copy_depth = 1;
    strip(m_data_buffer);
    m_copy_buffer = escape_xml(m_data_buffer) +
      create_xml_node_name(name, atts);
    m_strip = true;

  }

  m_data_buffer = "";
}

void
usf_reader_c::end_element_cb(const char *name) {
  int i;
  string node;

  // Generate the full path to this node.
  for (i = 0; m_parents.size() > i; ++i) {
    if (!node.empty())
      node += ".";
    node += m_parents[i];
  }

  m_parents.pop_back();

  if (m_strip)
    strip(m_data_buffer);

  if (0 != m_copy_depth) {
    if ((m_data_buffer == "") && !m_copy_buffer.empty() &&
        (m_previous_start == name)) {
      m_copy_buffer.erase(m_copy_buffer.length() - 1);
      m_copy_buffer += "/>";
    } else
      m_copy_buffer += escape_xml(m_data_buffer) + "</" + name + ">";
    --m_copy_depth;

    if (0 == m_copy_depth) {
      if (node == "USFSubtitles.subtitles.subtitle") {
        usf_track_t &track = m_tracks[m_tracks.size() - 1];

        m_copy_buffer.erase(m_copy_buffer.length() - 11);
        strip(m_copy_buffer);
        track.m_entries[track.m_entries.size() - 1].m_text = m_copy_buffer;

      } else
        m_private_data += m_copy_buffer;

      m_copy_buffer = "";
      m_strip = false;
    }
  }

  m_data_buffer = "";
  m_previous_start = "";
}

void
usf_reader_c::add_data_cb(const XML_Char *s,
                          int len) {
  m_data_buffer.append((const char *)s, len);
}

void
usf_reader_c::create_packetizer(int64_t tid) {
  if ((0 > tid) || (m_tracks.size() <= tid))
    return;

  usf_track_t &track = m_tracks[tid];

  if (!demuxing_requested('s', tid) || (-1 != track.m_ptzr))
    return;

  ti.language = track.m_language;
  track.m_ptzr =
    add_packetizer(new textsubs_packetizer_c(this, MKV_S_TEXTUSF,
                                             m_private_data.c_str(),
                                             m_private_data.length(), false,
                                             true, ti));
  mxinfo(FMT_TID "Using the text subtitle output module.\n", ti.fname.c_str(),
         tid);
}

void
usf_reader_c::create_packetizers() {
  int i;

  for (i = 0; m_tracks.size() > i; ++i)
    create_packetizer(i);
}

file_status_e
usf_reader_c::read(generic_packetizer_c *ptzr,
                   bool) {
  int i;
  usf_track_t *track;

  track = NULL;
  for (i = 0; m_tracks.size() > i; ++i)
    if ((-1 != m_tracks[i].m_ptzr) && (PTZR(m_tracks[i].m_ptzr) == ptzr)) {
      track = &m_tracks[i];
      break;
    }

  if (NULL == track)
    return FILE_STATUS_DONE;

  if (track->m_entries.end() == track->m_current_entry)
    return FILE_STATUS_DONE;

  const usf_entry_t &entry = *(track->m_current_entry);
  // A length of 0 here is OK because the text subtitle packetizer assumes
  // that the data is a zero-terminated string.
  memory_cptr mem(new memory_c((unsigned char *)entry.m_text.c_str(), 0,
                               false));
  PTZR(track->m_ptzr)->process(new packet_t(mem, entry.m_start, entry.m_end -
                                            entry.m_start));
  ++(track->m_current_entry);

  if (track->m_entries.end() == track->m_current_entry) {
    PTZR(track->m_ptzr)->flush();
    return FILE_STATUS_DONE;
  }

  return FILE_STATUS_MOREDATA;
}

int
usf_reader_c::get_progress() {
  if (-1 == m_longest_track)
    return 0;
  usf_track_t &track = m_tracks[m_longest_track];
  if (track.m_entries.size() == 0)
    return 0;
  return 100 -
    distance(track.m_current_entry,
             (vector<usf_entry_t>::const_iterator)track.m_entries.end()) *
    100 / track.m_entries.size();
}

int64_t
usf_reader_c::try_to_parse_timecode(const char *s) {
  int64_t timecode;

  if (!parse_timecode(s, timecode))
    throw xml_parser_error_c("Invalid start or stop timecode", m_xml_parser);

  return timecode;
}

void
usf_reader_c::identify() {
  int i;

  mxinfo("File '%s': container: USF\n", ti.fname.c_str());
  for (i = 0; m_tracks.size() > i; ++i) {
    usf_track_t &track = m_tracks[i];
    string info;

    mxinfo("Track ID %d: subtitles (USF)", i);
    if (identify_verbose) {
      info = " [";
      if (track.m_language != "")
        info += "language:" + escape(track.m_language) + " ";
      mxinfo("%s]", info.c_str());
    }
    mxinfo("\n");
  }
}
