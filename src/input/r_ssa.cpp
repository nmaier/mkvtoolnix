/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * SSA/ASS subtitle parser
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <algorithm>

#include "mkvmerge.h"
#include "pr_generic.h"
#include "r_ssa.h"
#include "subtitles.h"
#include "matroska.h"

using namespace std;

class ssa_line_c {
public:
  char *line;
  int64_t start, end;
  int num;

  bool operator < (const ssa_line_c &cmp) const;
};

bool
ssa_line_c::operator < (const ssa_line_c &cmp)
  const {
  return start < cmp.start;
}

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
    mm_io = new mm_text_io_c(ti->fname);

    if (!ssa_reader_c::probe_file(mm_io, 0))
      throw error_c("ssa_reader: Source is not a valid SSA/ASS file.");

    sub_charset_found = false;
    for (i = 0; i < ti->sub_charsets->size(); i++)
      if ((*ti->sub_charsets)[i].id == 0) {
        sub_charset_found = true;
        cc_utf8 = utf8_init((*ti->sub_charsets)[i].language);
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

      // Now just append the current line and some DOS style newlines.
      // But not if we've already encountered the [Events] section.
      if (section != 'e') {
        global += "\r\n";
        global += line;
      }
    }

    if (format.size() == 0)
      throw error_c("ssa_reader: Invalid format. Could not find the "
                    "\"Format\" line in the \"[Events]\" section.");

  } catch (exception &ex) {
    throw error_c("ssa_reader: Could not open the source file.");
  }
  if (verbose)
    mxinfo(FMT_FN "Using the SSA/ASS subtitle reader.\n", ti->fname);
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
  mxinfo(FMT_TID "Using the text subtitle output module.\n", ti->fname,
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
  if (!parse_int(s.c_str(), th))
    return -1;
  stime.erase(0, pos + 1);

  pos = stime.find(':');
  if (pos < 0)
    return -1;

  s = stime.substr(0, pos);
  if (!parse_int(s.c_str(), tm))
    return -1;
  stime.erase(0, pos + 1);

  pos = stime.find('.');
  if (pos < 0)
    return -1;

  s = stime.substr(0, pos);
  if (!parse_int(s.c_str(), ts))
    return -1;
  stime.erase(0, pos + 1);

  if (!parse_int(stime.c_str(), tds))
    return -1;

  return (tds * 10 + ts * 1000 + tm * 60 * 1000 + th * 60 * 60 * 1000) *
    1000000;
}

string
ssa_reader_c::recode_text(vector<string> &fields) {
  char *s;
  string res;

  // TODO: Handle \fe encoding changes.
  res = get_element("Text", fields);
  s = to_utf8(cc_utf8, res.c_str());
  res = s;
  safefree(s);

  return res;
}

int
ssa_reader_c::read(generic_packetizer_c *,
                   bool) {
  string line, stime, orig_line, comma;
  int i, num;
  int64_t start, end;
  vector<ssa_line_c> clines;
  vector<string> fields;
  ssa_line_c cline;

  num = 1;

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
    line = comma + get_element("Layer", fields) + comma +
      get_element("Style", fields) + comma +
      get_element("Name", fields) + comma + 
      get_element("MarginL", fields) + comma +
      get_element("MarginR", fields) + comma +
      get_element("MarginV", fields) + comma +
      get_element("Effect", fields) + comma +
      recode_text(fields);

    cline.line = safestrdup(line.c_str());
    cline.start = start;
    cline.end = end;
    cline.num = num;
    num++;

    clines.push_back(cline);
  } while (!mm_io->eof());

  stable_sort(clines.begin(), clines.end());

  for (i = 0; i < clines.size(); i++) {
    char buffer[20];
    // Let the packetizer handle this line.
    mxprints(buffer, "%d", clines[i].num);
    line = string(buffer) + string(clines[i].line);
    memory_c mem((unsigned char *)line.c_str(), 0, false);
    PTZR0->process(mem, clines[i].start, clines[i].end - clines[i].start);
    safefree(clines[i].line);
  }

  PTZR0->flush();

  return 0;
}

void
ssa_reader_c::identify() {
  mxinfo("File '%s': container: SSA/ASS\nTrack ID 0: subtitles "
         "(SSA/ASS)\n", ti->fname);
}
