/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   WAVPACK output module

   Written by Steve Lhomme <steve.lhomme@free.fr>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "matroska.h"
#include "p_wavpack.h"

using namespace libmatroska;

wavpack_packetizer_c::wavpack_packetizer_c(generic_reader_c *_reader,
                                           wavpack_meta_t &meta,
                                           track_info_c &_ti)
  throw (error_c):
  generic_packetizer_c(_reader, _ti),
  channels(meta.channel_count), sample_rate(meta.sample_rate),
  bits_per_sample(meta.bits_per_sample),
  samples_per_block(meta.samples_per_block),
  samples_output(0) {

  has_correction = meta.has_correction && (htrack_max_add_block_ids != 0);

  set_track_type(track_audio);
}

void
wavpack_packetizer_c::set_headers() {
  set_codec_id(MKV_A_WAVPACK4);
  set_codec_private(ti.private_data, ti.private_size);
  set_audio_sampling_freq((float)sample_rate);
  set_audio_channels(channels);
  set_audio_bit_depth(bits_per_sample);
  set_track_default_duration(samples_per_block * 1000000000 / sample_rate);
  set_track_max_additionals(has_correction ? 1 : 0);

  generic_packetizer_c::set_headers();

  track_entry->EnableLacing(!has_correction);
}

int
wavpack_packetizer_c::process(packet_cptr packet) {
  int64_t samples = get_uint32_le(packet->data->get());

  if (-1 == packet->duration)
    packet->duration = irnd(samples * 1000000000 / sample_rate);
  else
    mxverb(2, "wavpack_packetizer: incomplete block with duration " LLD "\n",
           packet->duration);
  if (-1 == packet->timecode)
    packet->timecode = irnd((double)samples_output * 1000000000 / sample_rate);
  samples_output += samples;
  add_packet(packet);

  return FILE_STATUS_MOREDATA;
}

connection_result_e
wavpack_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                     string &error_message) {
  wavpack_packetizer_c *psrc;

  psrc = dynamic_cast<wavpack_packetizer_c *>(src);
  if (psrc == NULL)
    return CAN_CONNECT_NO_FORMAT;
  connect_check_a_samplerate(sample_rate, psrc->sample_rate);
  connect_check_a_channels(channels, psrc->channels);
  connect_check_a_bitdepth(bits_per_sample, psrc->bits_per_sample);
  return CAN_CONNECT_YES;
}
