/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_vobsub.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief VobSub packetizer
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <matroska/KaxContentEncoding.h>

#include "common.h"
#include "compression.h"
#include "matroska.h"
#include "mm_io.h"
#include "p_vobsub.h"
#include "subtitles.h"

using namespace libmatroska;

vobsub_packetizer_c::vobsub_packetizer_c(generic_reader_c *nreader,
                                         const void *nidx_data,
                                         int nidx_data_size,
                                         track_info_c *nti) throw (error_c):
  generic_packetizer_c(nreader, nti) {

  idx_data = (unsigned char *)safememdup(nidx_data, nidx_data_size);
  idx_data_size = nidx_data_size;

  set_track_type(track_subtitle);
#ifdef HAVE_ZLIB_H
  set_default_compression_method(COMPRESSION_ZLIB);
#endif
}

vobsub_packetizer_c::~vobsub_packetizer_c() {
  safefree(idx_data);
}

void
vobsub_packetizer_c::set_headers() {
  set_codec_id(MKV_S_VOBSUB);
  set_codec_private(idx_data, idx_data_size);

  generic_packetizer_c::set_headers();

  track_entry->EnableLacing(false);
}

int
vobsub_packetizer_c::process(memory_c &mem,
                             int64_t timecode,
                             int64_t duration,
                             int64_t,
                             int64_t) {
  timecode += initial_displacement;
  if (timecode < 0)
    return EMOREDATA;

  timecode = (int64_t)((float)timecode * ti->async.linear);
  add_packet(mem, timecode, duration, true);

  return EMOREDATA;
}

void
vobsub_packetizer_c::dump_debug_info() {
  mxdebug("vobsub_packetizer_c: queue: %d\n", packet_queue.size());
}

int
vobsub_packetizer_c::can_connect_to(generic_packetizer_c *src) {
  vobsub_packetizer_c *vsrc;

  vsrc = dynamic_cast<vobsub_packetizer_c *>(src);
  if (vsrc == NULL)
    return CAN_CONNECT_NO_FORMAT;
  return CAN_CONNECT_YES;
}
