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

#define CMP(member) (member != psrc->member)

connection_result_e
passthrough_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                         string &error_message) {
  passthrough_packetizer_c *psrc;

  psrc = dynamic_cast<passthrough_packetizer_c *>(src);
  if (psrc == NULL)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_codec_id(hcodec_id, psrc->hcodec_id);

  if (CMP(htrack_type) || CMP(hcodec_id))
    return CAN_CONNECT_NO_PARAMETERS;

  if (((ti.private_data == NULL) && (psrc->ti.private_data != NULL))
      || ((ti.private_data != NULL) && (psrc->ti.private_data == NULL))
      || (ti.private_size != psrc->ti.private_size)
      || ((ti.private_data != NULL) && (psrc->ti.private_data != NULL)
          && (ti.private_size == psrc->ti.private_size)
          && memcmp(ti.private_data, psrc->ti.private_data, ti.private_size))) {
    error_message = mxsprintf("The codec's private data does not match "
                              "(lengths: %d and %d).", ti.private_size,
                              psrc->ti.private_size);
    return CAN_CONNECT_MAYBE_CODECPRIVATE;
  }

  switch (htrack_type) {
    case track_video:
      connect_check_v_width(hvideo_pixel_width, psrc->hvideo_pixel_width);
      connect_check_v_height(hvideo_pixel_height, psrc->hvideo_pixel_height);
      connect_check_v_dwidth(hvideo_display_width, psrc->hvideo_display_width);
      connect_check_v_dheight(hvideo_display_height, psrc->hvideo_display_height);
      if (CMP(htrack_min_cache) || CMP(htrack_max_cache) || CMP(htrack_default_duration))
        return CAN_CONNECT_NO_PARAMETERS;
      break;

    case track_audio:
      connect_check_a_samplerate(haudio_sampling_freq, psrc->haudio_sampling_freq);
      connect_check_a_channels(haudio_channels, psrc->haudio_channels);
      connect_check_a_bitdepth(haudio_bit_depth, psrc->haudio_bit_depth);
      if (CMP(htrack_default_duration))
        return CAN_CONNECT_NO_PARAMETERS;
      break;

    case track_subtitle:
      break;

    default:
      break;
  }

  return CAN_CONNECT_YES;
}
