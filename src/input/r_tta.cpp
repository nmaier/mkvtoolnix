/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 * 
 * TTA demultiplexer module
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "mkvmerge.h"
#include "common.h"
#include "error.h"
#include "id3_common.h"
#include "p_tta.h"
#include "r_tta.h"

int
tta_reader_c::probe_file(mm_io_c *mm_io,
                         int64_t size) {
  int tag_size;
  unsigned char buf[4];

  if (size < 26)
    return 0;
  try {
    mm_io->setFilePointer(0, seek_beginning);
    tag_size = skip_id3v2_tag(*mm_io);
    if (tag_size == -1)
      return 0;
    if (mm_io->read(buf, 4) != 4)
      return 0;
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
  int tag_size;

  try {
    mm_io = new mm_file_io_c(ti->fname);
    size = mm_io->get_size();

    if (identifying)
      return;

    tag_size = skip_id3v2_tag(*mm_io);
    if (tag_size < 0)
      mxerror(_("tta_reader: tag_size < 0 in the c'tor. %s\n"), BUGMSG);
    size -= tag_size;

    if (mm_io->read(&header, sizeof(tta_file_header_t)) !=
        sizeof(tta_file_header_t))
      mxerror(FMT_FN "The file header is too short.\n", ti->fname);
    seek_sum = mm_io->getFilePointer() + 4 - tag_size;

    size -= id3_tag_present_at_end(*mm_io);

    do {
      seek_point = mm_io->read_uint32();
      seek_sum += seek_point + 4;
      seek_points.push_back(seek_point);
    } while (seek_sum < size);

    mxverb(2, "tta: ch %u bps %u sr %u dl %u seek_sum %lld size %lld num %u\n",
           get_uint16(&header.channels), get_uint16(&header.bits_per_sample),
           get_uint32(&header.sample_rate), get_uint32(&header.data_length),
           seek_sum, size, seek_points.size());

    if (seek_sum != size)
      mxerror(FMT_FN "The seek table in this TTA file seems to be broken.\n",
              ti->fname);

    mm_io->skip(4);

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
  ttapacketizer = new tta_packetizer_c(this, get_uint16(&header.channels),
                                       get_uint16(&header.bits_per_sample),
                                       get_uint32(&header.sample_rate), ti);
  add_packetizer(ttapacketizer);
  mxinfo(FMT_TID "Using the TTA output module.\n", ti->fname, (int64_t)0);
}

int
tta_reader_c::read(generic_packetizer_c *,
                   bool) {
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
  pos++;

  memory_c mem(buf, nread, true);
  if (pos >= seek_points.size()) {
    double samples_left;

    samples_left = (double)get_uint32(&header.data_length) -
      (seek_points.size() - 1) * TTA_FRAME_TIME *
      get_uint32(&header.sample_rate);
    mxverb(2, "tta: samples_left %f\n", samples_left);

    PTZR0->process(mem, -1, irnd(samples_left * 1000000000.0l /
                                 get_uint32(&header.sample_rate)));
  } else
    PTZR0->process(mem);
  bytes_processed += nread;

  if (pos >= seek_points.size())
    return 0;

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
