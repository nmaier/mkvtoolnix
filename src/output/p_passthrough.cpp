/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   PASSTHROUGH output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "pr_generic.h"
#include "p_passthrough.h"
#include "matroska.h"

using namespace libmatroska;

passthrough_packetizer_c::passthrough_packetizer_c(generic_reader_c *_reader,
                                                   track_info_c &_ti)
  throw (error_c):
  generic_packetizer_c(_reader, _ti),
  packets_processed(0), bytes_processed(0), sync_to_keyframe(false) {

  timecode_factory_application_mode = TFA_FULL_QUEUEING;
}

void
passthrough_packetizer_c::set_headers() {
  generic_packetizer_c::set_headers();
}

int
passthrough_packetizer_c::process(packet_cptr packet) {
  int64_t timecode;

  debug_enter("passthrough_packetizer_c::process");

  timecode = packet->timecode;
  packets_processed++;
  bytes_processed += packet->data->get_size();
  if ((packet->duration > 0) &&
      needs_negative_displacement(packet->duration)) {
    displace(-packet->duration);
    sync_to_keyframe = true;
    return FILE_STATUS_MOREDATA;
  }
  if ((packet->duration > 0) &&
      needs_positive_displacement(packet->duration) &&
      sync_complete_group) {
    // Not implemented yet.
  }
  while ((packet->duration > 0) &&
         needs_positive_displacement(packet->duration)) {
    packet_t *new_packet =
      new packet_t(packet->data->clone(),
                   (int64_t)((timecode + ti.async.displacement) *
                             ti.async.linear),
                   packet->duration, packet->bref, packet->fref);
    new_packet->duration_mandatory = packet->duration_mandatory;
    add_packet(new_packet);
    displace(packet->duration);
  }

  if (sync_to_keyframe && (packet->bref != -1))
    return FILE_STATUS_MOREDATA;
  sync_to_keyframe = false;
  packet->timecode = (int64_t)((timecode + ti.async.displacement) *
                               ti.async.linear);
  packet->duration = (int64_t)(packet->duration * ti.async.linear);
  if (packet->bref >= 0)
    packet->bref = (int64_t)((packet->bref + ti.async.displacement) *
                             ti.async.linear);
  if (packet->fref >= 0)
    packet->fref = (int64_t)((packet->fref + ti.async.displacement) *
                             ti.async.linear);
  add_packet(packet);

  debug_leave("passthrough_packetizer_c::process");
  return FILE_STATUS_MOREDATA;
}

void
passthrough_packetizer_c::always_sync_complete_group(bool sync) {
  sync_complete_group = sync;
}

void
passthrough_packetizer_c::dump_debug_info() {
  mxdebug("passthrough_packetizer_c: packets processed: " LLD ", "
          "bytes processed: " LLD ", packets in queue: %u\n",
          packets_processed, bytes_processed,
          (unsigned int)packet_queue.size());
}

connection_result_e
passthrough_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                         string &error_message) {
  passthrough_packetizer_c *psrc;

  psrc = dynamic_cast<passthrough_packetizer_c *>(src);
  if (psrc == NULL)
    return CAN_CONNECT_NO_FORMAT;
  connect_check_codec_id(hcodec_id, psrc->hcodec_id);
  return CAN_CONNECT_NO_PARAMETERS;
}
