/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_tta.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file r_tta.cpp
    \version $Id$
    \brief TTA demultiplexer module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "mkvmerge.h"
#include "common.h"
#include "error.h"
#include "p_tta.h"
#include "r_tta.h"

int
tta_reader_c::probe_file(mm_io_c *mm_io,
                         int64_t size) {
  unsigned char buf[4];

  if (size < 26)
    return 0;
  try {
    mm_io->setFilePointer(0, seek_beginning);
    if (mm_io->read(buf, 4) != 4)
      mm_io->setFilePointer(0, seek_beginning);
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    return 0;
  }
  if (!strncmp((char *)buf, "TTA1", 4))
    return 1;
  return 0;
}

tta_reader_c::tta_reader_c(track_info_c *nti)
  throw (error_c):
  generic_reader_c(nti) {
  uint32_t seek_point;
  int64_t seek_sum;

  try {
    mm_io = new mm_io_c(ti->fname, MODE_READ);
    size = mm_io->get_size();
    mm_io->setFilePointer(6, seek_beginning);

    if (identifying)
      return;

    channels = mm_io->read_uint16();
    bits_per_sample = mm_io->read_uint16();
    sample_rate = mm_io->read_uint32();
    data_length = mm_io->read_uint32();
    mm_io->skip(4);

    seek_sum = mm_io->getFilePointer() + 4;

    do {
      seek_point = mm_io->read_uint32();
      seek_sum += seek_point + 4;
      seek_points.push_back(seek_point);
    } while (seek_sum < size);

    if (seek_sum != size)
      mxerror(FMT_FN "The seek table in this TTA file seems to be broken.\n",
              ti->fname);

    mm_io->skip(4);

    mxverb(2, "tta: ch %u bps %u sr %u dl %u seek_sum %lld size %lld num %u\n",
           channels, bits_per_sample, sample_rate, data_length, seek_sum, size,
           seek_points.size());

    bytes_processed = 0;
    pos = 0;
    ti->id = 0;                 // ID for this track.

  } catch (exception &ex) {
    throw error_c("tta_reader: Could not open the file.");
  }
  if (verbose)
    mxinfo(FMT_FN "Using the TTA demultiplexer.\n", ti->fname);
}

tta_reader_c::~tta_reader_c() {
  delete mm_io;
}

void
tta_reader_c::create_packetizer(int64_t) {
  generic_packetizer_c *ttapacketizer;

  if (NPTZR() != 0)
    return;
  ttapacketizer = new tta_packetizer_c(this, channels, bits_per_sample,
                                       sample_rate, ti);
  add_packetizer(ttapacketizer);
  mxinfo(FMT_TID "Using the TTA output module.\n", ti->fname, (int64_t)0);
}

int
tta_reader_c::read(generic_packetizer_c *) {
  unsigned char *buf;
  int nread;

  if (pos >= seek_points.size())
    return 0;

  buf = (unsigned char *)safemalloc(seek_points[pos]);
  nread = mm_io->read(buf, seek_points[pos]);
  if (nread <= 0) {
    PTZR0->flush();
    return 0;
  }

  memory_c mem(buf, nread, true);
  PTZR0->process(mem);
  bytes_processed += nread;
  pos++;

  return EMOREDATA;
}

int
tta_reader_c::display_priority() {
  return DISPLAYPRIORITY_HIGH - 1;
}

void
tta_reader_c::display_progress(bool final) {
  if (final)
    mxinfo("progress: %lld/%lld bytes (100%%)\r", size, size);
  else
    mxinfo("progress: %lld/%lld bytes (%d%%)\r", bytes_processed, size,
           (int)(bytes_processed * 100L / size));
}

void
tta_reader_c::identify() {
  mxinfo("File '%s': container: TTA\nTrack ID 0: audio (TTA)\n", ti->fname);
}
