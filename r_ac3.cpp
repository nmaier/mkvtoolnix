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
    \version \$Id: r_ac3.cpp,v 1.25 2003/06/03 14:29:14 mosu Exp $
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

int ac3_reader_c::probe_file(mm_io_c *mm_io, int64_t size) {
  char buf[4096];
  int pos;
  ac3_header_t ac3header;

  if (size < 4096)
    return 0;
  try {
    mm_io->setFilePointer(0, seek_beginning);
    if (mm_io->read(buf, 4096) != 4096)
      return 0;
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    return 0;
  }

  pos = find_ac3_header((unsigned char *)buf, 4096, &ac3header);
  if (pos != 0)
    return 0;

  return 1;
}

ac3_reader_c::ac3_reader_c(track_info_t *nti) throw (error_c):
  generic_reader_c(nti) {
  int pos;
  ac3_header_t ac3header;

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
    throw error_c("ac3_reader: No valid AC3 packet found in the first " \
                  "4096 bytes.\n");
  bytes_processed = 0;
  ac3packetizer = new ac3_packetizer_c(this, ac3header.sample_rate,
                                       ac3header.channels, ti);
  if (verbose)
    fprintf(stdout, "Using AC3 demultiplexer for %s.\n+-> Using " \
            "AC3 output module for audio stream.\n", ti->fname);
}

ac3_reader_c::~ac3_reader_c() {
  delete mm_io;
  if (chunk != NULL)
    safefree(chunk);
  if (ac3packetizer != NULL)
    delete ac3packetizer;
}

int ac3_reader_c::read() {
  int nread;

  nread = mm_io->read(chunk, 4096);
  if (nread <= 0)
    return 0;

  ac3packetizer->process(chunk, nread);
  bytes_processed += nread;

  return EMOREDATA;
}

packet_t *ac3_reader_c::get_packet() {
  return ac3packetizer->get_packet();
}

int ac3_reader_c::display_priority() {
  return DISPLAYPRIORITY_HIGH - 1;
}

void ac3_reader_c::display_progress() {
  fprintf(stdout, "progress: %lld/%lld bytes (%d%%)\r",
          bytes_processed, size,
          (int)(bytes_processed * 100L / size));
  fflush(stdout);
}

void ac3_reader_c::set_headers() {
  ac3packetizer->set_headers();
}
