/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   AC3 demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

extern "C" {
#include <avilib.h>
}

#include "common.h"
#include "error.h"
#include "r_ac3.h"
#include "p_ac3.h"

int
ac3_reader_c::probe_file(mm_io_c *io,
                         int64_t size,
                         int64_t probe_size,
                         int num_headers) {
  return (find_valid_headers(io, probe_size, num_headers) != -1) ? 1 : 0;
}

ac3_reader_c::ac3_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti) {
  int pos;

  try {
    io = new mm_file_io_c(ti.fname);
    size = io->get_size();
    chunk = (unsigned char *)safemalloc(4096);
    if (io->read(chunk, 4096) != 4096)
      throw error_c("ac3_reader: Could not read 4096 bytes.");
    io->setFilePointer(0, seek_beginning);
  } catch (...) {
    throw error_c("ac3_reader: Could not open the source file.");
  }
  pos = find_ac3_header(chunk, 4096, &ac3header);
  if (pos < 0)
    throw error_c("ac3_reader: No valid AC3 packet found in the first "
                  "4096 bytes.\n");
  bytes_processed = 0;
  ti.id = 0;                   // ID for this track.
  if (verbose)
    mxinfo(FMT_FN "Using the AC3 demultiplexer.\n", ti.fname.c_str());
}

ac3_reader_c::~ac3_reader_c() {
  delete io;
  safefree(chunk);
}

void
ac3_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;
  add_packetizer(new ac3_packetizer_c(this, ac3header.sample_rate,
                                      ac3header.channels, ac3header.bsid, ti));
  mxinfo(FMT_TID "Using the AC3 output module.\n", ti.fname.c_str(),
         (int64_t)0);
}

file_status_e
ac3_reader_c::read(generic_packetizer_c *,
                   bool) {
  int nread;

  nread = io->read(chunk, 4096);
  if (nread <= 0) {
    PTZR0->flush();
    return FILE_STATUS_DONE;
  }

  PTZR0->process(new packet_t(new memory_c(chunk, nread, false)));
  bytes_processed += nread;

  return FILE_STATUS_MOREDATA;
}

int
ac3_reader_c::get_progress() {
  return 100 * bytes_processed / size;
}

void
ac3_reader_c::identify() {
  mxinfo("File '%s': container: AC3\nTrack ID 0: audio (AC3)\n",
         ti.fname.c_str());
}

int
ac3_reader_c::find_valid_headers(mm_io_c *io,
                                 int64_t probe_range,
                                 int num_headers) {
  unsigned char *buf;
  int pos, nread;

  try {
    io->setFilePointer(0, seek_beginning);
    buf = (unsigned char *)safemalloc(probe_range);
    nread = io->read(buf, probe_range);
    pos = find_consecutive_ac3_headers(buf, nread, num_headers);
    safefree(buf);
    io->setFilePointer(0, seek_beginning);
    return pos;
  } catch (...) {
    return -1;
  }
}
