/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   VobBtn packetizer

   Written by Steve Lhomme <steve.lhomme@free.fr>.
   Modified by Moritz Bunkus <moritz@bunkus.org>.
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
#include "p_vobbtn.h"

using namespace libmatroska;

vobbtn_packetizer_c::vobbtn_packetizer_c(generic_reader_c *nreader,
                                         int nwidth,
                                         int nheight,
                                         track_info_c &_ti)
  throw (error_c):
  generic_packetizer_c(nreader, _ti),
  previous_timecode(0),
  width(nwidth),
  height(nheight) {

  set_track_type(track_buttons);
  set_default_compression_method(COMPRESSION_ZLIB);
}

vobbtn_packetizer_c::~vobbtn_packetizer_c() {
}

void
vobbtn_packetizer_c::set_headers() {
  set_codec_id(MKV_B_VOBBTN);

  set_video_pixel_width(width);
  set_video_pixel_height(height);

  generic_packetizer_c::set_headers();

  track_entry->EnableLacing(false);
}

int
vobbtn_packetizer_c::process(memory_c &mem,
                             int64_t timecode,
                             int64_t duration,
                             int64_t,
                             int64_t) {

  int32_t vobu_start, vobu_end;

  vobu_start = get_uint32_be(mem.data + 0x0d);
  vobu_end = get_uint32_be(mem.data + 0x11);

  duration = (int64_t)(100000.0 * (float)(vobu_end - vobu_start) / 9);
  if (timecode == -1) {
    timecode = previous_timecode;
    previous_timecode += duration;
  } else
    timecode = timecode + ti.async.displacement;

  timecode = (int64_t)(timecode * ti.async.linear);
  add_packet(mem, timecode, duration, true);

  return FILE_STATUS_MOREDATA;
}

void
vobbtn_packetizer_c::dump_debug_info() {
  mxdebug("vobbtn_packetizer_c: queue: %d\n", packet_queue.size());
}

connection_result_e
vobbtn_packetizer_c::can_connect_to(generic_packetizer_c *src) {
  vobbtn_packetizer_c *vsrc;

  vsrc = dynamic_cast<vobbtn_packetizer_c *>(src);
  if (vsrc == NULL)
    return CAN_CONNECT_NO_FORMAT;
  if ((width != vsrc->width) || (height != vsrc->height))
    return CAN_CONNECT_NO_PARAMETERS;
  return CAN_CONNECT_YES;
}
