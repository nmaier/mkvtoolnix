/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_ac3.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief AC3 demultiplexer module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

extern "C" {
#include <avilib.h>
}

#include "mkvmerge.h"
#include "common.h"
#include "error.h"
#include "r_ac3.h"
#include "p_ac3.h"

#define PROBESIZE 8192

int
ac3_reader_c::probe_file(mm_io_c *mm_io,
                         int64_t size) {
  unsigned char buf[PROBESIZE];
  int pos;
  ac3_header_t ac3header;

  if (size < PROBESIZE)
    return 0;
  try {
    mm_io->setFilePointer(0, seek_beginning);
    if (mm_io->read(buf, PROBESIZE) != PROBESIZE)
      return 0;
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    return 0;
  }

  pos = find_ac3_header(buf, PROBESIZE, &ac3header);
  if ((pos < 0) || ((pos + ac3header.bytes) >= PROBESIZE))
    return 0;
  pos = find_ac3_header(&buf[pos + ac3header.bytes], PROBESIZE - pos -
                        ac3header.bytes, &ac3header);
  if (pos != 0)
    return 0;

  return 1;
}

ac3_reader_c::ac3_reader_c(track_info_c *nti)
  throw (error_c):
  generic_reader_c(nti) {
  int pos;

  try {
    mm_io = new mm_io_c(ti->fname, MODE_READ);
    mm_io->setFilePointer(0, seek_end);
    size = mm_io->getFilePointer();
    mm_io->setFilePointer(0, seek_beginning);
    chunk = (unsigned char *)safemalloc(4096);
    if (mm_io->read(chunk, 4096) != 4096)
      throw error_c("ac3_reader: Could not read 4096 bytes.");
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    throw error_c("ac3_reader: Could not open the source file.");
  }
  pos = find_ac3_header(chunk, 4096, &ac3header);
  if (pos < 0)
    throw error_c("ac3_reader: No valid AC3 packet found in the first "
                  "4096 bytes.\n");
  bytes_processed = 0;
  ti->id = 0;                   // ID for this track.
  if (verbose)
    mxinfo(FMT_FN "Using the AC3 demultiplexer.\n", ti->fname);
}

ac3_reader_c::~ac3_reader_c() {
  delete mm_io;
  safefree(chunk);
}

void
ac3_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;
  add_packetizer(new ac3_packetizer_c(this, ac3header.sample_rate,
                                      ac3header.channels, ac3header.bsid, ti));
  mxinfo(FMT_TID "Using the AC3 output module.\n", ti->fname, (int64_t)0);
}

int
ac3_reader_c::read(generic_packetizer_c *) {
  int nread;

  nread = mm_io->read(chunk, 4096);
  if (nread <= 0) {
    PTZR0->flush();
    return 0;
  }

  memory_c mem(chunk, nread, false);
  PTZR0->process(mem);
  bytes_processed += nread;

  return EMOREDATA;
}

int
ac3_reader_c::display_priority() {
  return DISPLAYPRIORITY_HIGH - 1;
}

void
ac3_reader_c::display_progress(bool final) {
  if (final)
    mxinfo("progress: %lld/%lld bytes (100%%)\r", size, size);
  else
    mxinfo("progress: %lld/%lld bytes (%d%%)\r", bytes_processed, size,
           (int)(bytes_processed * 100L / size));
}

void
ac3_reader_c::identify() {
  mxinfo("File '%s': container: AC3\nTrack ID 0: audio (AC3)\n", ti->fname);
}
