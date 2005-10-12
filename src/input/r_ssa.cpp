/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   SSA/ASS subtitle parser

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <algorithm>

#include "base64.h"
#include "extern_data.h"
#include "pr_generic.h"
#include "r_ssa.h"
#include "matroska.h"
#include "output_control.h"

using namespace std;

int
ssa_reader_c::probe_file(mm_text_io_c *io,
                         int64_t size) {
  string line;

  try {
    io->setFilePointer(0, seek_beginning);
    line = io->getline();
    if (strcasecmp(line.c_str(), "[script info]"))
      return 0;
    io->setFilePointer(0, seek_beginning);
  } catch (...) {
    return 0;
  }
  return 1;
}

ssa_reader_c::ssa_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti),
  m_cc_utf8(0),
  m_is_ass(false) {

  auto_ptr<mm_text_io_c> io;

  try {
    io = auto_ptr<mm_text_io_c>(new mm_text_io_c(new mm_file_io_c(ti.fname)));
  } catch (...) {
    throw error_c("ssa_reader: Could not open the source file.");
  }

  if (!ssa_reader_c::probe_file(io.get(), 0))
    throw error_c("ssa_reader: Source is not a valid SSA/ASS file.");

  if (map_has_key(ti.sub_charsets, 0))
    m_cc_utf8 = utf8_init(ti.sub_charsets[0]);
  else if (map_has_key(ti.sub_charsets, -1))
    m_cc_utf8 = utf8_init(ti.sub_charsets[-1]);
  else if (io->get_byte_order() != BO_NONE)
    m_cc_utf8 = utf8_init("UTF-8");
  else
    m_cc_utf8 = cc_local_utf8;

  parse_file(io.get());

  if (verbose)
    mxinfo(FMT_FN "Using the SSA/ASS subtitle reader.\n", ti.fname.c_str());
}

ssa_reader_c::~ssa_reader_c() {
}

void
ssa_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;

  add_packetizer(new textsubs_packetizer_c(this, m_is_ass ?  MKV_S_TEXTASS :
                                           MKV_S_TEXTSSA, m_global.c_str(),
                                           m_global.length(), false, false,
                                           ti));
  mxinfo(FMT_TID "Using the text subtitle output module.\n", ti.fname.c_str(),
         (int64_t)0);
}

string
ssa_reader_c::get_element(const char *index,
                          vector<string> &fields) {
  int i;

  for (i = 0; i < m_format.size(); i++)
    if (m_format[i] == index)
      return fields[i];

  return string("");
}

int64_t
ssa_reader_c::parse_time(string &stime) {
  int64_t th, tm, ts, tds;
  int pos;
  string s;

  pos = stime.find(':');
  if (pos < 0)
    return -1;

  s = stime.substr(0, pos);
  if (!parse_int(s, th))
    return -1;
  stime.erase(0, pos + 1);

  pos = stime.find(':');
  if (pos < 0)
    return -1;

  s = stime.substr(0, pos);
  if (!parse_int(s, tm))
    return -1;
  stime.erase(0, pos + 1);

  pos = stime.find('.');
  if (pos < 0)
    return -1;

  s = stime.substr(0, pos);
  if (!parse_int(s, ts))
    return -1;
  stime.erase(0, pos + 1);

  if (!parse_int(stime, tds))
    return -1;

  return (tds * 10 + ts * 1000 + tm * 60 * 1000 + th * 60 * 60 * 1000) *
    1000000;
}

string
ssa_reader_c::recode_text(vector<string> &fields) {
  // TODO: Handle \fe encoding changes.
  return to_utf8(m_cc_utf8, get_element("Text", fields));
}

void
ssa_reader_c::parse_file(mm_text_io_c *io) {
  string line, stime, orig_line, comma, name_field;
  string attachment_name, attachment_data_uu;
  int num, i;
  int64_t start, end;
  vector<string> fields;
  ssa_section_e section, previous_section;
  bool add_to_global;

  num = 0;

  section = SSA_SECTION_NONE;
  previous_section = SSA_SECTION_NONE;
  ti.id = 0;                 // ID for this track.
  name_field = "Name";
  while (!io->eof()) {
    if (!io->getline2(line))
      break;

    add_to_global = true;

    // A normal line. Let's see if this file is ASS and not SSA.
    if (!strcasecmp(line.c_str(), "ScriptType: v4.00+"))
      m_is_ass = true;

    else if (!strcasecmp(line.c_str(), "[V4+ Styles]")) {
      m_is_ass = true;
      section = SSA_SECTION_V4STYLES;

    } else if (!strcasecmp(line.c_str(), "[V4 Styles]"))
      section = SSA_SECTION_V4STYLES;

    else if (!strcasecmp(line.c_str(), "[Script Info]"))
      section = SSA_SECTION_INFO;

    else if (!strcasecmp(line.c_str(), "[Events]"))
      section = SSA_SECTION_EVENTS;

    else if (!strcasecmp(line.c_str(), "[Graphics]")) {
      section = SSA_SECTION_GRAPHICS;
      add_to_global = false;

    } else if (!strcasecmp(line.c_str(), "[Fonts]")) {
      section = SSA_SECTION_FONTS;
      add_to_global = false;

    } else if (SSA_SECTION_EVENTS == section) {
      if (starts_with_case(line, "Format: ")) {
        // Analyze the format string.
        m_format = split(&line.c_str()[strlen("Format: ")]);
        strip(m_format);

        // Let's see if "Actor" is used in the format instead of "Name".
        for (i = 0; m_format.size() > i; ++i)
          if (downcase(m_format[i]) == "actor") {
            name_field = "Actor";
            break;
          }

      } else if (starts_with_case(line, "Dialogue: ")) {
        if (m_format.size() == 0)
          throw error_c("ssa_reader: Invalid format. Could not find the "
                        "\"Format\" line in the \"[Events]\" section.");

        orig_line = line;

        line.erase(0, strlen("Dialogue: ")); // Trim the start.

        // Split the line into fields.
        fields = split(line.c_str(), ",", m_format.size());
        while (fields.size() < m_format.size())
          fields.push_back(string(""));

        // Parse the start time.
        stime = get_element("Start", fields);
        start = parse_time(stime);
        if (start < 0) {
          mxwarn("ssa_reader: Malformed line? (%s)\n", orig_line.c_str());
          continue;
        }

        // Parse the end time.
        stime = get_element("End", fields);
        end = parse_time(stime);
        if (end < 0) {
          mxwarn("ssa_reader: Malformed line? (%s)\n", orig_line.c_str());
          continue;
        }
        if (end < start) {
          mxwarn("ssa_reader: Malformed line? (%s)\n", orig_line.c_str());
          continue;
        }

        // Specs say that the following fields are to put into the block:
        // ReadOrder, Layer, Style, Name, MarginL, MarginR, MarginV, Effect,
        //   Text

        comma = ",";
        line = to_string(num) + comma + get_element("Layer", fields) + comma +
          get_element("Style", fields) + comma +
          get_element(name_field.c_str(), fields) + comma +
          get_element("MarginL", fields) + comma +
          get_element("MarginR", fields) + comma +
          get_element("MarginV", fields) + comma +
          get_element("Effect", fields) + comma +
          recode_text(fields);

        m_subs.add(start, end, line);
        num++;

        add_to_global = false;
      }

    } else if ((SSA_SECTION_FONTS == section) ||
               (SSA_SECTION_GRAPHICS == section)) {
      if (starts_with_case(line, "fontname:")) {
        add_attachment_maybe(attachment_name, attachment_data_uu,
                             section);

        line.erase(0, strlen("fontname:"));
        strip(line, true);
        attachment_name = line;

      } else {
        strip(line, true);
        attachment_data_uu += line;
      }

      add_to_global = false;
    }

    if (add_to_global) {
      m_global += line;
      m_global += "\r\n";
    }

    if (previous_section != section)
      add_attachment_maybe(attachment_name, attachment_data_uu,
                           previous_section);
    previous_section = section;
  }

  m_subs.sort();
}

void
ssa_reader_c::add_attachment_maybe(string &name,
                                   string &data_uu,
                                   ssa_section_e section) {
  if (ti.no_attachments || (name == "") || (data_uu == "") ||
      ((SSA_SECTION_FONTS != section) && (SSA_SECTION_GRAPHICS != section))) {
    name = "";
    data_uu = "";
    return;
  }

  attachment_t attachment;
  string type = SSA_SECTION_FONTS == section ? "font" : "picture";
  string short_name;
  buffer_t *buffer = new buffer_t;
  const unsigned char *p;
  int pos, allocated;

  short_name = ti.fname;
  pos = short_name.rfind('/');
  if (0 < pos)
    short_name.erase(0, pos + 1);
  pos = short_name.rfind('\\');
  if (0 < pos)
    short_name.erase(0, pos + 1);
  attachment.name = to_utf8(m_cc_utf8, name);
  attachment.description = "Imported " + type + " from " + short_name;
  attachment.to_all_files = true;

//   mxinfo("att %s: %s\n", name.c_str(), data_uu.c_str());

  allocated = 1024;
  buffer = new buffer_t((unsigned char *)safemalloc(allocated), 0);

  p = (const unsigned char *)data_uu.c_str();
  for (pos = 0; data_uu.length() > (pos + 4); pos += 4)
    decode_chars(p[pos], p[pos + 1], p[pos + 2], p[pos + 3], *buffer, 3,
                 allocated);
  switch (data_uu.length() % 4) {
    case 2:
      decode_chars(p[pos], p[pos + 1], 0, 0, *buffer, 1, allocated);
      break;
    case 3:
      decode_chars(p[pos], p[pos + 1], p[pos + 2], 0, *buffer, 2, allocated);
      break;
  }

  attachment.data = counted_ptr<buffer_t>(buffer);

  pos = name.rfind('.');
  if (0 < pos) {
    name.erase(0, pos + 1);
    attachment.mime_type = guess_mime_type(name);
  }
  if (attachment.mime_type == "")
    attachment.mime_type = "application/octet-stream";

  add_attachment(attachment);

  name = "";
  data_uu = "";
}

void
ssa_reader_c::decode_chars(unsigned char c1,
                           unsigned char c2,
                           unsigned char c3,
                           unsigned char c4,
                           buffer_t &buffer,
                           int bytes_to_add,
                           int &allocated) {
  uint32_t value, i;
  unsigned char bytes[3];

  value = ((c1 - 33) << 18) + ((c2 - 33) << 12) + ((c3 - 33) << 6) + (c4 - 33);
  bytes[2] = value & 0xff;
  bytes[1] = (value & 0xff00) >> 8;
  bytes[0] = (value & 0xff0000) >> 16;

  if ((buffer.m_size + bytes_to_add) > allocated) {
    allocated += 1024;
    buffer.m_buffer = (unsigned char *)realloc(buffer.m_buffer, allocated);
  }

  for (i = 0; i < bytes_to_add; ++i)
    buffer.m_buffer[buffer.m_size + i] = bytes[i];
  buffer.m_size += bytes_to_add;
}

file_status_e
ssa_reader_c::read(generic_packetizer_c *,
                   bool) {
  if (m_subs.empty())
    return FILE_STATUS_DONE;

  m_subs.process((textsubs_packetizer_c *)PTZR0);

  if (m_subs.empty()) {
    flush_packetizers();
    return FILE_STATUS_DONE;
  }
  return FILE_STATUS_MOREDATA;
}

int
ssa_reader_c::get_progress() {
  int num_entries;

  num_entries = m_subs.get_num_entries();
  if (num_entries == 0)
    return 100;
  return 100 * m_subs.get_num_processed() / num_entries;
}

void
ssa_reader_c::identify() {
  mxinfo("File '%s': container: SSA/ASS\nTrack ID 0: subtitles "
         "(SSA/ASS)\n", ti.fname.c_str());
}
