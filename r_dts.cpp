/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_dts.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief DTS demultiplexer module
    \author Peter Niemayer <niemayer@isg.de>
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "mkvmerge.h"
#include "common.h"
#include "error.h"
#include "r_dts.h"
#include "p_dts.h"

int dts_reader_c::probe_file(mm_io_c *mm_io, int64_t size) {
  char buf[max_dts_packet_size];
  int pos;
  dts_header_t dtsheader;

  if (size < max_dts_packet_size)
    return 0;
  try {
    mm_io->setFilePointer(0, seek_beginning);
    if (mm_io->read(buf, max_dts_packet_size) != max_dts_packet_size)
      return 0;
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    return 0;
  }

  pos = find_dts_header((unsigned char *)buf, max_dts_packet_size, &dtsheader);
  if (pos < 0)
    return 0;

  return 1;
}

dts_reader_c::dts_reader_c(track_info_t *nti) throw (error_c):
  generic_reader_c(nti) {
  int pos;
  dts_header_t dtsheader;

  try {
    mm_io = new mm_io_c(ti->fname, MODE_READ);
    mm_io->setFilePointer(0, seek_end);
    size = mm_io->getFilePointer();
    mm_io->setFilePointer(0, seek_beginning);
    chunk = (unsigned char *)safemalloc(max_dts_packet_size);
    if (mm_io->read(chunk, max_dts_packet_size) != max_dts_packet_size)
      throw error_c("dts_reader: Could not read max_dts_packet_size bytes.");
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    throw error_c("dts_reader: Could not open the source file.");
  }

  pos = find_dts_header(chunk, max_dts_packet_size, &dtsheader);

  if (pos < 0)
    throw error_c("dts_reader: No valid DTS packet found in the first "
                  "max_dts_packet_size bytes.\n");
  bytes_processed = 0;
  ti->id = 0;                   // ID for this track.
  dtspacketizer = new dts_packetizer_c(this, dtsheader, ti);

  if (verbose) {
    mxprint(stdout, "Using DTS demultiplexer for %s.\n+-> Using "
            "DTS output module for audio stream.\n", ti->fname);

    print_dts_header(&dtsheader);
  }
}

dts_reader_c::~dts_reader_c() {
  delete mm_io;
  safefree(chunk);
  if (dtspacketizer != NULL)
    delete dtspacketizer;
}

int dts_reader_c::read() {
  int nread;

  nread = mm_io->read(chunk, max_dts_packet_size);
  if (nread <= 0)
    return 0;

  dtspacketizer->process(chunk, nread);
  bytes_processed += nread;

  return EMOREDATA;
}

packet_t *dts_reader_c::get_packet() {
  return dtspacketizer->get_packet();
}

int dts_reader_c::display_priority() {
  return DISPLAYPRIORITY_HIGH - 1;
}

void dts_reader_c::display_progress() {
  mxprint(stdout, "progress: %lld/%lld bytes (%d%%)\r",
          bytes_processed, size,
          (int)(bytes_processed * 100L / size));
  fflush(stdout);
}

void dts_reader_c::set_headers() {
  dtspacketizer->set_headers();
}

void dts_reader_c::identify() {
  mxprint(stdout, "File '%s': container: DTS\nTrack ID 0: audio (DTS)\n",
          ti->fname);
}
