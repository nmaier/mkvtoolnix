/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_ssa.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief SSA/ASS subtitle parser
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "mkvmerge.h"
#include "pr_generic.h"
#include "r_ssa.h"
#include "subtitles.h"
#include "matroska.h"

using namespace std;

int ssa_reader_c::probe_file(mm_io_c *mm_io, int64_t size) {
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

ssa_reader_c::ssa_reader_c(track_info_t *nti) throw (error_c):
  generic_reader_c(nti) {
  string line, global;
  int64_t old_pos;

  try {
    mm_io = new mm_io_c(ti->fname, MODE_READ);
    if (!ssa_reader_c::probe_file(mm_io, 0))
      throw error_c("ssa_reader: Source is not a valid SSA/ASS file.");
    ti->id = 0;                 // ID for this track.
    global = mm_io->getline();  // [Script Info]
    while (!mm_io->eof()) {
      old_pos = mm_io->getFilePointer();
      line = mm_io->getline();
      if (line.length() == 0)   // Ignore empty lines.
        continue;
      if (line.c_str()[0] == ';') { // Just a comment.
        global += "\r\n";       // DOS style newlines
        global += line;
        continue;
      }

      if (!strncasecmp(line.c_str(), "Dialogue: ", strlen("Dialogue: "))) {
        // End of global data - restore file position to just before this
        // line end bail out.
        mm_io->setFilePointer(old_pos);
        break;
      }

      // Else just append the current line and some DOS style newlines.
      global += "\r\n";
      global += line;
    }

    textsubs_packetizer = new textsubs_packetizer_c(this, MKV_S_TEXTSSA,
                                                    global.c_str(),
                                                    global.length(), ti);
  } catch (exception &ex) {
    throw error_c("ssa_reader: Could not open the source file.");
  }
  if (verbose)
    fprintf(stdout, "Using SSA/ASS subtitle reader for %s.\n+-> Using "
            "text subtitle output module for subtitles.\n", ti->fname);
}

ssa_reader_c::~ssa_reader_c() {
  if (textsubs_packetizer != NULL)
    delete textsubs_packetizer;
}

int64_t ssa_reader_c::parse_time(string &stime) {
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

  return tds * 10 + ts * 1000 + tm * 60 * 1000 + th * 60 * 60 * 1000;
}

int ssa_reader_c::read() {
  string line, stime, orig_line;
  int pos1, pos2;
  int64_t start, end;

  do {
    line = mm_io->getline();
    orig_line = line;
    if (strncasecmp(line.c_str(), "Dialogue: ", strlen("Dialogue: ")))
      continue;

    line.erase(0, strlen("Dialogue: ")); // Trim the start.

    pos1 = line.find(',');      // Find and parse the start time.
    if (pos1 < 0) {
      fprintf(stderr, "ssa_reader: Warning: Malformed line? (%s)\n",
              orig_line.c_str());
      continue;
    }
    pos2 = line.find(',', pos1 + 1);
    if (pos2 < 0) {
      fprintf(stderr, "ssa_reader: Warning: Malformed line? (%s)\n",
              orig_line.c_str());
      continue;
    }

    stime = line.substr(pos1 + 1, pos2 - pos1 - 1);
    start = parse_time(stime);
    if (start < 0) {
      fprintf(stderr, "ssa_reader: Warning: Malformed line? (%s)\n",
              orig_line.c_str());
      continue;
    }
    line.erase(pos1, pos2 - pos1);

    pos1 = line.find(',');      // Find and parse the end time.
    if (pos1 < 0) {
      fprintf(stderr, "ssa_reader: Warning: Malformed line? (%s)\n",
              orig_line.c_str());
      continue;
    }
    pos2 = line.find(',', pos1 + 1);
    if (pos2 < 0) {
      fprintf(stderr, "ssa_reader: Warning: Malformed line? (%s)\n",
              orig_line.c_str());
      continue;
    }

    stime = line.substr(pos1 + 1, pos2 - pos1 - 1);
    end = parse_time(stime);
    if (end < 0) {
      fprintf(stderr, "ssa_reader: Warning: Malformed line? (%s)\n",
              orig_line.c_str());
      continue;
    }
    line.erase(pos1, pos2 - pos1);

    // Let the packetizer handle this line.
    textsubs_packetizer->process((unsigned char *)line.c_str(), 0, start,
                                 end - start);
  } while (!mm_io->eof());

  return 0;
}

packet_t *ssa_reader_c::get_packet() {
  return textsubs_packetizer->get_packet();
}

int ssa_reader_c::display_priority() {
  return DISPLAYPRIORITY_LOW;
}

static char wchar[] = "-\\|/-\\|/-";

void ssa_reader_c::display_progress() {
  fprintf(stdout, "working... %c\r", wchar[act_wchar]);
  act_wchar++;
  if (act_wchar == strlen(wchar))
    act_wchar = 0;
  fflush(stdout);
}

void ssa_reader_c::set_headers() {
  textsubs_packetizer->set_headers();
}

void ssa_reader_c::identify() {
  fprintf(stdout, "File '%s': container: SSA/ASS\nTrack ID 0: subtitles "
          "(SSA/ASS)\n", ti->fname);
}
