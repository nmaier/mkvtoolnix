/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_vobsub.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief VobSub stream reader
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "mm_io.h"
#include "p_vobsub.h"
#include "r_vobsub.h"

using namespace std;

#define hexvalue(c) (isdigit(c) ? (c) - '0' : \
                     tolower(c) == 'a' ? 10 : \
                     tolower(c) == 'b' ? 11 : \
                     tolower(c) == 'c' ? 12 : \
                     tolower(c) == 'd' ? 13 : \
                     tolower(c) == 'e' ? 14 : 15)
#define istimecodestr(s)       (!strncmp(s, "timecode: ", 10))
#define istimestampstr(s)      (!strncmp(s, "timestamp: ", 11))
#define iscommafileposstr(s)   (!strncmp(s, ", filepos: ", 11))
#define iscolon(s)             (*(s) == ':')
#define istwodigits(s)         (isdigit(*(s)) && isdigit(*(s + 1)))
#define isthreedigits(s)       (isdigit(*(s)) && isdigit(*(s + 1)) && \
                                isdigit(*(s + 2)))
#define istwodigitscolon(s)    (istwodigits(s) && iscolon(s + 2))
#define istimecode(s)         (istwodigitscolon(s) && \
                                istwodigitscolon(s + 3) && \
                                istwodigitscolon(s + 6) && \
                                isthreedigits(s + 9))
#define ishexdigit(s)          (isdigit(s) || \
                                (strchr("abcdefABCDEF", s) != NULL))
#define isfilepos(s)           (ishexdigit(*(s)) && ishexdigit(*(s + 1)) && \
                                ishexdigit(*(s + 2)) && \
                                ishexdigit(*(s + 3)) && \
                                ishexdigit(*(s + 4)) && \
                                ishexdigit(*(s + 5)) && \
                                ishexdigit(*(s + 6)) && \
                                ishexdigit(*(s + 7)) && \
                                ishexdigit(*(s + 8)))
// timestamp: 00:01:43:603, filepos: 000000000
#define isvobsubline_v3(s)     ((strlen(s) >= 43) && \
                                istimestampstr(s) && istimecode(s + 11) && \
                                iscommafileposstr(s + 23) && \
                                isfilepos(s + 34))
#define isvobsubline_v7(s)     ((strlen(s) >= 42) && \
                                istimecodestr(s) && istimecode(s + 10) && \
                                iscommafileposstr(s + 22) && \
                                isfilepos(s + 33))

#define PFX "vobsub_reader: "

int vobsub_reader_c::probe_file(mm_io_c *mm_io, int64_t size) {
  char chunk[80];

  try {
    mm_io->setFilePointer(0, seek_beginning);
    if (mm_io->read(chunk, 80) != 80)
      return 0;
    if (strncasecmp(chunk, "# VobSub index file, v",
                    strlen("# VobSub index file, v")))
      return 0;
    mm_io->setFilePointer(0, seek_beginning);
  } catch (...) {
    return 0;
  }
  return 1;
}

vobsub_reader_c::vobsub_reader_c(track_info_t *nti) throw (error_c):
  generic_reader_c(nti) {
  mm_io_c *ifo_file;
  string sub_name, ifo_name, line;
  int len;

  try {
    idx_file = new mm_text_io_c(ti->fname);
  } catch (...) {
    throw error_c(PFX "Cound not open the source file.");
  }

  sub_name = ti->fname;
  len = sub_name.rfind(".");
  if (len >= 0)
    sub_name.erase(len);
  sub_name += ".sub";

  try {
    sub_file = new mm_io_c(sub_name.c_str(), MODE_READ);
  } catch (...) {
    string emsg = PFX "Could not open the sub file '";
    emsg += sub_name;
    emsg += "'.";
    throw error_c(emsg.c_str());
  }

  ifo_name = ti->fname;
  len = ifo_name.rfind(".");
  if (len >= 0)
    ifo_name.erase(len);
  ifo_name += ".ifo";

  ifo_file = NULL;
  try {
    ifo_file = new mm_io_c(ifo_name.c_str(), MODE_READ);
    ifo_file->setFilePointer(0, seek_end);
    ifo_data_size = ifo_file->getFilePointer();
    ifo_file->setFilePointer(0);
    ifo_data = (unsigned char *)safemalloc(ifo_data_size);
    if (ifo_file->read(ifo_data, ifo_data_size) != ifo_data_size)
      mxerror(PFX "Could not read the IFO file.\n");
    delete ifo_file;
  } catch (...) {
    if (ifo_file != NULL)
      delete ifo_file;
    ifo_data = NULL;
    ifo_data_size = 0;
  }

  idx_data = "";
  last_filepos = -1;
  last_timestamp = -1;
  act_wchar = 0;
  done = false;

  len = strlen("# VobSub index file, v");
  if (!idx_file->getline2(line) ||
      strncasecmp(line.c_str(), "# VobSub index file, v", len) ||
      (line.length() < (len + 1)))
    mxerror(PFX "No version number found.\n");

  version = line[len] - '0';
  len++;
  while ((len < line.length()) && isdigit(line[len])) {
    version = version * 10 + line[len] - '0';
    len++;
  }
  mxverb(2, PFX "Version: %u\n", version);

  if ((version != 3) && (version != 7))
    mxerror(PFX "Unsupported file type version %d.\n", version);

  if (!parse_headers())
    throw error_c(PFX "The input file is not a valid VobSub file.");

  packetizer = new vobsub_packetizer_c(this, idx_data.c_str(),
                                       idx_data.length(), ifo_data,
                                       ifo_data_size, COMPRESSION_LZO,
                                       COMPRESSION_NONE, ti);

  if (verbose) {
    mxinfo("Using VobSub subtitle reader for '%s'/'%s'. ", ti->fname,
           sub_name.c_str());
    if (ifo_data_size != 0)
      mxinfo("Using IFO file '%s'.", ifo_name.c_str());
    else
      mxinfo("No IFO file found.");
    mxinfo("\n+-> Using VobSub subtitle output module for subtitles.\n");
  }
}

vobsub_reader_c::~vobsub_reader_c() {
  delete sub_file;
  delete idx_file;
  safefree(ifo_data);
  delete packetizer;
}

bool vobsub_reader_c::parse_headers() {
  int64_t pos;
  string line;
  const char *sline;

  while (1) {
    pos = idx_file->getFilePointer();

    if (!idx_file->getline2(line))
      return false;

    if ((line.length() == 0) || (line[0] == '#')) {
      idx_data += line;
      idx_data += "\n";
      continue;
    }

    sline = line.c_str();
    if (((version == 3) && isvobsubline_v3(sline)) ||
        ((version == 7) && isvobsubline_v7(sline))) {
      idx_file->setFilePointer(pos);
      return true;
    }

    idx_data += line;
    idx_data += "\n";
  }
}

int vobsub_reader_c::read(generic_packetizer_c *) {
  string line;
  const char *s;
  int64_t filepos, timestamp;
  int hour, minute, second, msecond, timestamp_offset, filepos_offset, idx;
  unsigned char *buffer;
  int size;

  if (done)
    return 0;

  if (!idx_file->getline2(line)) {
    if (last_filepos != -1) {
      sub_file->setFilePointer(0, seek_end);
      size = sub_file->getFilePointer() - last_filepos;
      sub_file->setFilePointer(last_filepos);
      buffer = (unsigned char *)safemalloc(size);
      if (sub_file->read(buffer, size) != size)
        mxerror(PFX "Could not read %u bytes from the VobSub file.\n", size);
      packetizer->process(buffer, size, last_timestamp, 1000);
      safefree(buffer);
    }

    done = true;
    return 0;
  }

  s = line.c_str();

  if ((version == 3) && !isvobsubline_v3(s))
    return EMOREDATA;
  if ((version == 7) && !isvobsubline_v7(s))
    return EMOREDATA;

  if (version == 3) {
// timestamp: 00:01:43:603, filepos: 000000000
    timestamp_offset = 11;
    filepos_offset = 34;
  } else if (version == 7) {
    die(PFX "Version 7 has not been implemented yet.");
  } else
    die(PFX "Unknown version. Should not have happened.");

  sscanf(&s[timestamp_offset], "%02d:%02d:%02d:%03d", &hour, &minute, &second,
         &msecond);
  timestamp = (int64_t)hour * 60 * 60 * 1000 +
    (int64_t)minute * 60 * 1000 + (int64_t)second * 1000 + (int64_t)msecond;

  idx = filepos_offset;
  filepos = hexvalue(s[idx]);
  idx++;
  while ((idx < line.length()) && ishexdigit(s[idx])) {
    filepos = filepos * 16 + hexvalue(s[idx]);
    idx++;
  }

  mxverb(3, PFX "Timestamp: %lld, file pos: %lld\n", timestamp, filepos);

  if (last_filepos == -1) {
    last_filepos = filepos;
    last_timestamp = timestamp;
    return EMOREDATA;
  }

  // Now process the stuff...
  sub_file->setFilePointer(last_filepos);
  size = filepos - last_filepos;
  buffer = (unsigned char *)safemalloc(size);
  if (sub_file->read(buffer, size) != size)
    mxerror(PFX "Could not read %u bytes from the VobSub file.\n", size);
  packetizer->process(buffer, size, last_timestamp,
                      timestamp - last_timestamp);
  safefree(buffer);
  last_timestamp = timestamp;
  last_filepos = filepos;

  return EMOREDATA;
}

int vobsub_reader_c::display_priority() {
  return DISPLAYPRIORITY_LOW;
}

static char wchar[] = "-\\|/-\\|/-";

void vobsub_reader_c::display_progress(bool final) {
  mxinfo("working... %c\r", wchar[act_wchar]);
  act_wchar++;
  if (act_wchar == strlen(wchar))
    act_wchar = 0;
}

void vobsub_reader_c::identify() {
  mxinfo("File '%s': container: VobSub\n", ti->fname);
  mxinfo("Track ID 0: subtitles (VobSub)\n");
}

void vobsub_reader_c::set_headers() {
  packetizer->set_headers();
}
