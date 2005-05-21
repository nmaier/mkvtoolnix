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

#include <errno.h>
#include <expat.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "mm_io.h"
#include "output_control.h"
#include "pr_generic.h"
#include "r_usf.h"

using namespace std;

static void
usf_xml_find_root_cb(void *user_data,
                     const char *name,
                     const char **atts) {
  string &root_element = *(string *)user_data;

  if (root_element == "")
    root_element = name;
}

static void
usf_xml_start_cb(void *user_data,
                 const char *name,
                 const char **atts) {
  ((usf_reader_c *)user_data)->start_cb(name, atts);
}

static void
usf_xml_end_cb(void *user_data,
               const char *name) {
  ((usf_reader_c *)user_data)->end_cb(name);
}

static void
usf_xml_add_data_cb(void *user_data,
                    const XML_Char *s,
                    int len) {
  ((usf_reader_c *)user_data)->add_data_cb(s, len);
}

int
usf_reader_c::probe_file(mm_text_io_c *io,
                         int64_t) {
  XML_Parser parser;
  string root_element, line;

  parser = XML_ParserCreate(NULL);
  XML_SetUserData(parser, &root_element);
  XML_SetElementHandler(parser, usf_xml_find_root_cb, NULL);

  try {
    io->setFilePointer(0);

    while (io->getline2(line)) {
      if ((XML_Parse(parser, line.c_str(), line.length(), 0) == 0) ||
          (root_element != ""))
        break;
    }

  } catch(...) {
  }

  XML_ParserFree(parser);

  return (root_element == "USFSubtitles" ? 1 : 0);
}

usf_reader_c::usf_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti),
  m_parser(NULL), m_copy_depth(0), m_strip(false) {

  try {
    mm_text_io_c io(new mm_file_io_c(ti.fname));
    XML_Parser parser;
    string line;

    if (!usf_reader_c::probe_file(&io, 0))
      throw error_c("usf_reader: Source is not a valid USF file.");

    parser = XML_ParserCreate(NULL);
    XML_SetUserData(parser, this);
    XML_SetElementHandler(parser, usf_xml_start_cb, usf_xml_end_cb);
    XML_SetCharacterDataHandler(parser, usf_xml_add_data_cb);

    io.setFilePointer(0);

    while (io.getline2(line)) {
      if (XML_Parse(parser, line.c_str(), line.length(), io.eof()) == 0) {
        XML_Error xerror;
        string error;

        xerror = XML_GetErrorCode(parser);
        error = mxsprintf("XML parser error at line %d of '%s': %s. ",
                          XML_GetCurrentLineNumber(parser), ti.fname.c_str(),
                          XML_ErrorString(xerror));
        if (xerror == XML_ERROR_INVALID_TOKEN)
          error += "Remember that special characters like &, <, > and \" "
            "must be escaped in the usual HTML way: &amp; for '&', "
            "&lt; for '<', &gt; for '>' and &quot; for '\"'.";
        XML_ParserFree(parser);
        throw error_c(error);
      }
    }

  } catch (mm_io_error_c &error) {
    throw error_c("usf_reader: Could not open the source file.");
  }

  if (verbose)
    mxinfo(FMT_FN "Using the USF subtitle reader.\n", ti.fname.c_str());
}

usf_reader_c::~usf_reader_c() {
}

string
usf_reader_c::create_node_name(const char *name,
                               const char **atts) {
  int i;
  string node_name;

  node_name = string("<") + name;
  for (i = 0; (NULL != atts[i]) && (NULL != atts[i + 1]); i += 2)
    node_name += string(" ") + atts[i] + "=\"" +
      escape_xml(atts[i + 1], true) + "\"";
  node_name += ">";

  return node_name;
}

void
usf_reader_c::start_cb(const char *name,
                       const char **atts) {
  int i;
  string node;

  m_previous_start = name;
  m_parents.push_back(name);

  if (1 >= m_parents.size())
    // Nothing to do for the root element.
    return;

  if (0 != m_copy_depth) {
    // Just copy the data.
    if (m_strip)
      strip(m_data_buffer);
    m_copy_buffer += m_data_buffer + create_node_name(name, atts);
    ++m_copy_depth;
    m_data_buffer = "";

    return;
  }

  // Generate the full path to this node.
  for (i = 0; m_parents.size() > i; ++i) {
    if (!node.empty())
      node += ".";
    node += m_parents[i];
  }

  mxinfo("start: %s\n", node.c_str());

  if (node == "USFSubtitles.metadata.language") {
    for (i = 0; (NULL != atts[i]) && (NULL != atts[i + 1]); i += 2)
      if (!strcmp(atts[i], "code")) {
        m_default_language = atts[i + 1];
        break;
      }

  } else if (node == "USFSubtitles.subtitles") {
    usf_track_t new_track;
    m_tracks.push_back(new_track);

  } else if (node == "USFSubtitles.subtitles.language") {
    for (i = 0; (NULL != atts[i]) && (NULL != atts[i + 1]); i += 2)
      if (!strcmp(atts[i], "code")) {
        m_tracks[m_tracks.size() - 1].m_language = atts[i + 1];
        break;
      }
  } else if (node == "USFSubtitles.subtitles.subtitle") {
    usf_entry_t entry;

    m_copy_buffer = "<subtitle";
    for (i = 0; (NULL != atts[i]) && (NULL != atts[i + 1]); i += 2)
      if (!strcmp(atts[i], "start"))
        entry.m_start = parse_timecode(atts[i + 1]);
      else if (!strcmp(atts[i], "stop"))
        entry.m_end = parse_timecode(atts[i + 1]);
      else
        m_copy_buffer += string(" ") + atts[i] + "=\"" +
          escape_xml(atts[i + 1], true) + "\"";
    m_copy_buffer += ">";
    m_copy_depth = 1;
    m_strip = true;

    m_tracks[m_tracks.size() - 1].m_entries.push_back(entry);

  } else if (m_parents.size() == 2) {
    m_copy_depth = 1;
    strip(m_data_buffer);
    m_copy_buffer = m_data_buffer + create_node_name(name, atts);
    m_strip = true;

  }

  m_data_buffer = "";
}

void
usf_reader_c::end_cb(const char *name) {
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
      m_copy_buffer += m_data_buffer + "</" + name + ">";
    --m_copy_depth;

    if (0 == m_copy_depth) {
      if (node == "USFSubtitles.subtitles.subtitle") {
        usf_track_t &track = m_tracks[m_tracks.size() - 1];

        track.m_entries[track.m_entries.size() - 1].m_text = m_copy_buffer;

      } else
        m_private_data += m_copy_buffer;

      m_copy_buffer = "";
      m_strip = false;
    }
  }

  m_data_buffer = "";

  mxinfo("end: %s\n", node.c_str());
}

void
usf_reader_c::add_data_cb(const XML_Char *s,
                          int len) {
  m_data_buffer.append((const char *)s, len);
}

void
usf_reader_c::create_packetizer(int64_t) {
}

void
usf_reader_c::parse_file() {
}

file_status_e
usf_reader_c::read(generic_packetizer_c *,
                   bool) {
  return FILE_STATUS_DONE;
}

int
usf_reader_c::get_progress() {
  return 0;
}

int64_t
usf_reader_c::parse_timecode(const char *s) {
  return -2;
}

void
usf_reader_c::identify() {
  int i, k;

  mxinfo("File '%s': container: USF\n", ti.fname.c_str());
  mxinfo("  private data: %s\n", m_private_data.c_str());
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

    for (k = 0; track.m_entries.size() > k; ++k) {
      usf_entry_t &entry = track.m_entries[k];
      mxinfo("  entry %d from %lld to %lld: %s\n", k, entry.m_start,
             entry.m_end, entry.m_text.c_str());
    }
  }
}
