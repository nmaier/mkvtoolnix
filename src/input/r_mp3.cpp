/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   MP3 reader module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "error.h"
#include "hacks.h"
#include "r_mp3.h"
#include "p_mp3.h"

int
mp3_reader_c::probe_file(mm_io_c *io,
                         int64_t,
                         int64_t probe_range,
                         int num_headers) {
  return (find_valid_headers(io, probe_range, num_headers) != -1) ? 1 : 0;
}

mp3_reader_c::mp3_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti) {
  int pos;
  unsigned char buf[16384];

  try {
    io = new mm_file_io_c(ti.fname);
    size = io->get_size();

    pos = find_valid_headers(io, 2 * 1024 * 1024, 5);
    if (pos < 0)
      throw error_c("Could not find a valid MP3 packet.");
    io->setFilePointer(pos, seek_beginning);
    io->read(buf, 4);
    decode_mp3_header(buf, &mp3header);
    io->setFilePointer(pos, seek_beginning);
    if (verbose)
      mxinfo(FMT_FN "Using the MP2/MP3 demultiplexer.\n", ti.fname.c_str());
    if ((pos > 0) && verbose)
      mxwarn("mp3_reader: skipping %d bytes at the beginning of '%s' (no "
                 "valid MP3 header found).\n", pos, ti.fname.c_str());

    bytes_processed = 0;
    ti.id = 0;                 // ID for this track.
  } catch (...) {
    throw error_c("mp3_reader: Could not open the source file.");
  }
}

mp3_reader_c::~mp3_reader_c() {
  delete io;
}

void
mp3_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;
  add_packetizer(new mp3_packetizer_c(this, mp3header.sampling_frequency,
                                      mp3header.channels, false, ti));
  mxinfo(FMT_TID "Using the MPEG audio output module.\n", ti.fname.c_str(),
         (int64_t)0);
}

file_status_e
mp3_reader_c::read(generic_packetizer_c *,
                   bool) {
  int nread;

  nread = io->read(chunk, 16384);
  if (nread <= 0) {
    PTZR0->flush();
    return FILE_STATUS_DONE;
  }

  memory_c mem(chunk, nread, false);
  PTZR0->process(mem);
  bytes_processed += nread;

  return FILE_STATUS_MOREDATA;
}

int
mp3_reader_c::get_progress() {
  return 100 * bytes_processed / size;
}

void
mp3_reader_c::identify() {
  mxinfo("File '%s': container: MP2/MP3\nTrack ID 0: audio (MPEG-%s layer "
         "%d)\n", ti.fname.c_str(),
         mp3header.version == 1 ? "1" : mp3header.version == 2 ? "2" : "2.5",
         mp3header.layer);
}

int
mp3_reader_c::find_valid_headers(mm_io_c *io,
                                 int64_t probe_range,
                                 int num_headers) {
  unsigned char *buf;
  int pos, nread;

  try {
    io->setFilePointer(0, seek_beginning);
    buf = (unsigned char *)safemalloc(probe_range);
    nread = io->read(buf, probe_range);
    pos = find_consecutive_mp3_headers(buf, nread, num_headers);
    safefree(buf);
    io->setFilePointer(0, seek_beginning);
    return pos;
  } catch (...) {
    return -1;
  }
}
