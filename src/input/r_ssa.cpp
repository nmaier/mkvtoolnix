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

#include "pr_generic.h"
#include "r_ssa.h"
#include "matroska.h"

using namespace std;

int
ssa_reader_c::probe_file(mm_text_io_c *mm_io,
                         int64_t size) {
  string line;

  try {
    mm_io->setFilePointer(0, seek_beginning);
    line = mm_io->getline();
    if (strcasecmp(line.c_str(), "[script info]"))
      return 0;
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    return 0;
  }
  return 1;
}

ssa_reader_c::ssa_reader_c(track_info_c *nti)
  throw (error_c):
  generic_reader_c(nti) {
  string line;
  int64_t old_pos;
  char section;
  bool sub_charset_found;
  int i;

  is_ass = false;
  section = 0;

  try {
    mm_io = new mm_text_io_c(new mm_file_io_c(ti->fname));

    if (!ssa_reader_c::probe_file(mm_io, 0))
      throw error_c("ssa_reader: Source is not a valid SSA/ASS file.");

    sub_charset_found = false;
    for (i = 0; i < ti->sub_charsets.size(); i++)
      if ((ti->sub_charsets[i].id == 0) || (ti->sub_charsets[i].id == -1)) {
        sub_charset_found = true;
        cc_utf8 = utf8_init(ti->sub_charsets[i].charset);
        break;
      }

    if (!sub_charset_found) {
      if (mm_io->get_byte_order() != BO_NONE)
        cc_utf8 = utf8_init("UTF-8");
      else
        cc_utf8 = utf8_init(ti->sub_charset);
    }

    ti->id = 0;                 // ID for this track.
    global = mm_io->getline();  // [Script Info]
    while (!mm_io->eof()) {
      old_pos = mm_io->getFilePointer();
      line = mm_io->getline();

      if (!strncasecmp(line.c_str(), "Dialogue: ", strlen("Dialogue: "))) {
        // End of global data - restore file position to just before this
        // line end bail out.
        mm_io->setFilePointer(old_pos);
        break;
      }

      // A normal line. Let's see if this file is ASS and not SSA.
      if (!strcasecmp(line.c_str(), "ScriptType: v4.00+") ||
          !strcasecmp(line.c_str(), "[V4+ Styles]"))
        is_ass = true;
      else if (!strcasecmp(line.c_str(), "[Events]"))
        section = 'e';
      // Analyze the format string.
      else if (!strncasecmp(line.c_str(), "Format: ", strlen("Format: ")) &&
               (section == 'e')) {
        format = split(&line.c_str()[strlen("Format: ")]);
        strip(format);
      }

      global += "\r\n";
      global += line;
    }

    if (format.size() == 0)
      throw error_c("ssa_reader: Invalid format. Could not find the "
                    "\"Format\" line in the \"[Events]\" section.");

  } catch (exception &ex) {
    throw error_c("ssa_reader: Could not open the source file.");
  }
  if (verbose)
    mxinfo(FMT_FN "Using the SSA/ASS subtitle reader.\n", ti->fname.c_str());
  parse_file();
}

ssa_reader_c::~ssa_reader_c() {
  delete mm_io;
}

void
ssa_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;

  add_packetizer(new textsubs_packetizer_c(this, is_ass ?  MKV_S_TEXTASS :
                                           MKV_S_TEXTSSA, global.c_str(),
                                           global.length(), false, false,
                                           ti));
  mxinfo(FMT_TID "Using the text subtitle output module.\n", ti->fname.c_str(),
         (int64_t)0);
}

string
ssa_reader_c::get_element(const char *index,
                          vector<string> &fields) {
  int i;

  for (i = 0; i < format.size(); i++)
    if (format[i] == index)
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
  return to_utf8(cc_utf8, get_element("Text", fields));
}

void
ssa_reader_c::parse_file() {
  string line, stime, orig_line, comma;
  int num;
  int64_t start, end;
  vector<string> fields;

  num = 0;

  do {
    line = mm_io->getline();
    orig_line = line;
    if (strncasecmp(line.c_str(), "Dialogue: ", strlen("Dialogue: ")))
      continue;

    line.erase(0, strlen("Dialogue: ")); // Trim the start.

    // Split the line into fields.
    fields = split(line.c_str(), ",", format.size());
    while (fields.size() < format.size())
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
    // ReadOrder, Layer, Style, Name, MarginL, MarginR, MarginV, Effect, Text

    comma = ",";
    line = to_string(num) + comma + get_element("Layer", fields) + comma +
      get_element("Style", fields) + comma +
      get_element("Name", fields) + comma + 
      get_element("MarginL", fields) + comma +
      get_element("MarginR", fields) + comma +
      get_element("MarginV", fields) + comma +
      get_element("Effect", fields) + comma +
      recode_text(fields);

    subs.add(start, end, line);
    num++;
  } while (!mm_io->eof());

  subs.sort();
}

file_status_e
ssa_reader_c::read(generic_packetizer_c *,
                   bool) {
  if (subs.empty())
    return FILE_STATUS_DONE;

  subs.process((textsubs_packetizer_c *)PTZR0);

  return subs.empty() ? FILE_STATUS_DONE : FILE_STATUS_MOREDATA;
}

int
ssa_reader_c::get_progress() {
  int num_entries;

  num_entries = subs.get_num_entries();
  if (num_entries == 0)
    return 100;
  return 100 * subs.get_num_processed() / num_entries;
}

void
ssa_reader_c::identify() {
  mxinfo("File '%s': container: SSA/ASS\nTrack ID 0: subtitles "
         "(SSA/ASS)\n", ti->fname.c_str());
}
