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

wavpack_packetizer_c::wavpack_packetizer_c(generic_reader_c *nreader,
                                           wavpack_meta_t &meta,
                                           track_info_c *nti)
  throw (error_c):
  generic_packetizer_c(nreader, nti) {
  sample_rate = meta.sample_rate;
  channels = meta.channel_count;
  bits_per_sample = meta.bits_per_sample;
  samples_per_block = meta.samples_per_block;
  samples_output = 0;

  set_track_type(track_audio);
}

void
wavpack_packetizer_c::set_headers() {
  set_codec_id(MKV_A_WAVPACK4);
  set_audio_sampling_freq((float)sample_rate);
  set_audio_channels(channels);
  set_audio_bit_depth(bits_per_sample);
  set_track_default_duration(samples_per_block * 1000000000 / sample_rate);
// _todo_ specify in the track header if it's using BlockAdditionals

  generic_packetizer_c::set_headers();
}

int
wavpack_packetizer_c::process(memory_c &mem,
                              int64_t,
                              int64_t duration,
                              int64_t,
                              int64_t) {
  debug_enter("wavpack_packetizer_c::process");
  int64_t samples = get_uint32(&mem.data[12]);

  if (duration == -1) {
    add_packet(mem, irnd(samples_output * 1000000000 / sample_rate),
               irnd(samples * 1000000000 / sample_rate));
    samples_output += samples;
  } else {
    mxverb(2, "wavpack_packetizer: incomplete block with duration %lld\n",
           duration);
    add_packet(mem, irnd((double)samples_output * 1000000000 / sample_rate),
               duration);
    samples_output += samples;
  }

  debug_leave("wavpack_packetizer_c::process");

  return FILE_STATUS_MOREDATA;
}

void
wavpack_packetizer_c::dump_debug_info() {
  mxdebug("wavpack_packetizer_c: queue: %d\n", packet_queue.size());
}

connection_result_e
wavpack_packetizer_c::can_connect_to(generic_packetizer_c *src) {
  wavpack_packetizer_c *psrc;

  psrc = dynamic_cast<wavpack_packetizer_c *>(src);
  if (psrc == NULL)
    return CAN_CONNECT_NO_FORMAT;
  if ((sample_rate != psrc->sample_rate) ||
      (channels != psrc->channels) || 
      (bits_per_sample != psrc->bits_per_sample))
    return CAN_CONNECT_NO_PARAMETERS;
  return CAN_CONNECT_YES;
}
