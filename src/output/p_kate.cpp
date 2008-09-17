/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   Kate packetizer

   Written by ogg.k.ogg.k <ogg.k.ogg.k@googlemail.com>.
   Adapted from code by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <ogg/ogg.h>

#include "common.h"
#include "kate_common.h"
#include "pr_generic.h"
#include "p_kate.h"
#include "matroska.h"

using namespace libmatroska;

kate_packetizer_c::kate_packetizer_c(generic_reader_c *_reader,
                                     const void *_global_data,
                                     int _global_size,
                                     track_info_c &_ti)
  throw (error_c):
  generic_packetizer_c(_reader, _ti),
  global_data(new memory_c((unsigned char *)safememdup(_global_data ? _global_data : ti.private_data, _global_data ? _global_size : ti.private_size),
                           _global_data ? _global_size : ti.private_size, true)) {
  int i, nheaders;

  set_track_type(track_subtitle);

  // the number of headers to expect is stored in the first header
  memory_cptr temp(new memory_c((unsigned char *)global_data->get(), global_data->get_size(), false));
  vector<memory_cptr> blocks = unlace_memory_xiph(temp);
  kate_parse_identification_header(blocks[0]->get(), blocks[0]->get_size(), kate_id);
  if (blocks.size() != kate_id.nheaders)
    throw false;

  set_language((const char*)kate_id.language);
  nheaders = kate_id.nheaders;
  for (i = 0; i < nheaders; ++i)
    headers.push_back(memory_cptr(new memory_c((unsigned char *)safememdup(blocks[i]->get(), blocks[i]->get_size()), blocks[i]->get_size(), true)));

  last_timecode = 0;
}

kate_packetizer_c::~kate_packetizer_c() {
}

void
kate_packetizer_c::set_headers() {
  set_codec_id(MKV_S_KATE);

  memory_cptr codecprivate;
  codecprivate = lace_memory_xiph(headers);
  set_codec_private(codecprivate->get(), codecprivate->get_size());

  generic_packetizer_c::set_headers();
}

int
kate_packetizer_c::process(packet_cptr packet) {

  if (packet->data->get_size() < (1 + 3 * sizeof(int64_t))) {
    /* end packet is 1 byte long and has type 0x7f */
    if ((packet->data->get_size() == 1) && (packet->data->get()[0]==0x7f)) {
      packet->timecode           = last_timecode;
      packet->duration           = 1;
      packet->duration_mandatory = true;
      add_packet(packet);
    } else
      mxwarn(_(FMT_TID "Kate packet is too small and is being skipped.\n"), ti.fname.c_str(), (int64_t)ti.id);

    return FILE_STATUS_MOREDATA;
  }

  int64_t start_time         = get_uint64_le(packet->data->get() + 1);
  int64_t duration           = get_uint64_le(packet->data->get() + 1 + sizeof(int64_t));

  double scaled_timecode     = start_time * (double)kate_id.gden / (double)kate_id.gnum;
  double scaled_duration     = duration   * (double)kate_id.gden / (double)kate_id.gnum;

  packet->timecode           = (int64_t)(scaled_timecode * 1000000000ll);
  packet->duration           = (int64_t)(scaled_duration * 1000000000ll);
  packet->duration_mandatory = true;
  packet->gap_following      = true;

  int64_t end                = packet->timecode + packet->duration;
  if (end > last_timecode)
    last_timecode = end;

  add_packet(packet);

  return FILE_STATUS_MOREDATA;
}

connection_result_e
kate_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                  string &error_message) {
  kate_packetizer_c *psrc;

  psrc = dynamic_cast<kate_packetizer_c *>(src);
  if (psrc == NULL)
    return CAN_CONNECT_NO_FORMAT;

  if (((ti.private_data == NULL) && (src->ti.private_data != NULL)) ||
      ((ti.private_data != NULL) && (src->ti.private_data == NULL)) ||
      (ti.private_size != src->ti.private_size)) {
    error_message = mxsprintf("The codec's private data does not match (lengths: %d and %d).", ti.private_size, src->ti.private_size);
    return CAN_CONNECT_MAYBE_CODECPRIVATE;
  }

  return CAN_CONNECT_YES;
}
