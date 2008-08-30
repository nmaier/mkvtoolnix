/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   Subripper subtitle reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <boost/regex.hpp>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "pr_generic.h"
#include "r_srt.h"
#include "subtitles.h"
#include "matroska.h"

using namespace std;

#define RE_TIMECODE      "(\\d{1,2}):(\\d{1,2}):(\\d{1,2})[,\\.](\\d+)?"
#define RE_TIMECODE_LINE "^" RE_TIMECODE "\\s+-+>\\s+" RE_TIMECODE "\\s*"
#define RE_COORDINATES   "([XY]\\d+:\\d+\\s*){4}\\s*$"

int
srt_reader_c::probe_file(mm_text_io_c *io,
                         int64_t) {
  try {
    io->setFilePointer(0, seek_beginning);
    string s = io->getline();
    strip(s);
    int64_t dummy;
    if (!parse_int(s, dummy))
      return 0;
    s = io->getline();

    boost::regex timecode_re(RE_TIMECODE_LINE, boost::regex::perl);
    boost::match_results<string::const_iterator> matches;
    if (!boost::regex_search(s, timecode_re))
      return 0;
    s = io->getline();
    io->setFilePointer(0, seek_beginning);
  } catch (...) {
    return 0;
  }
  return 1;
}

srt_reader_c::srt_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti),
  m_coordinates_warning_shown(false) {

  try {
    io = new mm_text_io_c(new mm_file_io_c(ti.fname));
    if (!srt_reader_c::probe_file(io, 0))
      throw error_c("srt_reader: Source is not a valid SRT file.");
    ti.id = 0;                 // ID for this track.
  } catch (...) {
    throw error_c("srt_reader: Could not open the source file.");
  }
  if (verbose)
    mxinfo(FMT_FN "Using the SRT subtitle reader.\n", ti.fname.c_str());
  parse_file();
}

srt_reader_c::~srt_reader_c() {
  delete io;
}

void
srt_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;

  bool is_utf8 = io->get_byte_order() != BO_NONE;
  add_packetizer(new textsubs_packetizer_c(this, MKV_S_TEXTUTF8, NULL, 0, true, is_utf8, ti));

  mxinfo(FMT_TID "Using the text subtitle output module.\n", ti.fname.c_str(), (int64_t)0);
}

#define STATE_INITIAL         0
#define STATE_SUBS            1
#define STATE_SUBS_OR_NUMBER  2
#define STATE_TIME            3

void
srt_reader_c::parse_file() {
  boost::regex timecode_re(RE_TIMECODE_LINE, boost::regex::perl);
  boost::regex number_re("^\\d+$", boost::regex::perl);
  boost::regex coordinates_re(RE_COORDINATES, boost::regex::perl);

  int64_t start                 = 0;
  int64_t end                   = 0;
  int64_t previous_start        = 0;
  bool timecode_warning_printed = false;
  int state                     = STATE_INITIAL;
  int line_number               = 0;
  string subtitles;

  while (1) {
    string s;
    if (!io->getline2(s))
      break;
    line_number++;
    strip_back(s);

    if (s.empty()) {
      if ((STATE_INITIAL == state) || (STATE_TIME == state))
        continue;
      state = STATE_SUBS_OR_NUMBER;
      if (!subtitles.empty())
        subtitles += "\n";
      subtitles += "\n";
      continue;
    }

    if (STATE_INITIAL == state) {
      if (!boost::regex_match(s, number_re)) {
        mxwarn(FMT_FN "Error in line %d: expected subtitle number and found some text.\n", ti.fname.c_str(), line_number);
        break;
      }
      state = STATE_TIME;

    } else if (STATE_TIME == state) {
      boost::match_results<string::const_iterator> matches;
      if (!boost::regex_search(s, matches, timecode_re)) {
        mxwarn(FMT_FN "Error in line %d: expected a SRT timecode line but found something else. Aborting this file.\n", ti.fname.c_str(), line_number);
        break;
      }

      int s_h, s_min, s_sec, e_h, e_min, e_sec;

      parse_int(matches[1].str(), s_h);
      parse_int(matches[2].str(), s_min);
      parse_int(matches[3].str(), s_sec);
      parse_int(matches[5].str(), e_h);
      parse_int(matches[6].str(), e_min);
      parse_int(matches[7].str(), e_sec);

      string s_rest = matches[4].str();
      string e_rest = matches[8].str();

      if (boost::regex_search(s, coordinates_re) && !m_coordinates_warning_shown) {
        mxwarn(FMT_FN "This file contains coordinates in the timecode lines. Such coordinates are not supported by the Matroska SRT subtitle format. "
               "mkvmerge will remove them automatically.\n", ti.fname.c_str());
        m_coordinates_warning_shown = true;
      }

      // The previous entry is done now. Append it to the list of subtitles.
      if (!subtitles.empty()) {
        strip_back(subtitles, true);
        subs.add(start, end, subtitles.c_str());
      }

      // Calculate the start and end time in ns precision for the following
      // entry.
      start  = (int64_t)s_h * 60 * 60 + s_min * 60 + s_sec;
      end    = (int64_t)e_h * 60 * 60 + e_min * 60 + e_sec;

      start *= 1000000000ll;
      end   *= 1000000000ll;

      while (s_rest.length() < 9)
        s_rest += "0";
      if (s_rest.length() > 9)
        s_rest.erase(9);
      start += atol(s_rest.c_str());

      while (e_rest.length() < 9)
        e_rest += "0";
      if (e_rest.length() > 9)
        e_rest.erase(9);
      end += atol(e_rest.c_str());

      // There are files for which start timecodes overlap. Matroska requires
      // blocks to be sorted by their timecode. mkvmerge does this at the end
      // of this function, but warn the user that the original order is being
      // changed.
      if (!timecode_warning_printed && (start < previous_start)) {
        mxwarn(FMT_FN "Warning in line %d: The start timecode is smaller than that of the previous entry. All entries from this file will be sorted by their start time.\n",
               ti.fname.c_str(), line_number);
        timecode_warning_printed = true;
      }

      previous_start = start;
      subtitles      = "";
      state          = STATE_SUBS;

    } else if (STATE_SUBS == state) {
      if (!subtitles.empty())
        subtitles += "\n";
      subtitles += s;

    } else if (boost::regex_match(s, number_re))
      state = STATE_TIME;

    else {
      if (!subtitles.empty())
        subtitles += "\n";
      subtitles += s;
    }
  }

  if (!subtitles.empty()) {
    strip_back(subtitles, true);
    subs.add(start, end, subtitles.c_str());
  }

  subs.sort();
}

file_status_e
srt_reader_c::read(generic_packetizer_c *,
                   bool) {
  if (subs.empty())
    return FILE_STATUS_DONE;

  subs.process(PTZR0);

  if (subs.empty()) {
    flush_packetizers();
    return FILE_STATUS_DONE;
  }
  return FILE_STATUS_MOREDATA;
}

int
srt_reader_c::get_progress() {
  int num_entries;

  num_entries = subs.get_num_entries();
  if (0 == num_entries)
    return 100;
  return 100 * subs.get_num_processed() / num_entries;
}

void
srt_reader_c::identify() {
  id_result_container("SRT");
  id_result_track(0, ID_RESULT_TRACK_SUBTITLES, "SRT");
}
