/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   VobSub packetizer

   Written by Moritz Bunkus <moritz@bunkus.org>.
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

vobsub_packetizer_c::vobsub_packetizer_c(generic_reader_c *_reader,
                                         const void *_idx_data,
                                         int _idx_data_size,
                                         track_info_c &_ti) throw (error_c):
  generic_packetizer_c(_reader, _ti),
  idx_data(safememdup(_idx_data, _idx_data_size)),
  idx_data_size(_idx_data_size) {

  set_track_type(track_subtitle);
  set_default_compression_method(COMPRESSION_ZLIB);
}

vobsub_packetizer_c::~vobsub_packetizer_c() {
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
    return FILE_STATUS_MOREDATA;

  timecode = (int64_t)((float)timecode * ti.async.linear);
  add_packet(mem, timecode, duration, true);

  return FILE_STATUS_MOREDATA;
}

void
vobsub_packetizer_c::dump_debug_info() {
  mxdebug("vobsub_packetizer_c: queue: %d\n", packet_queue.size());
}

connection_result_e
vobsub_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                    string &error_message) {
  vobsub_packetizer_c *vsrc;

  vsrc = dynamic_cast<vobsub_packetizer_c *>(src);
  if (vsrc == NULL)
    return CAN_CONNECT_NO_FORMAT;
  return CAN_CONNECT_YES;
}
