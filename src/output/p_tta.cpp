/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_tta.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id: p_tta.cpp 1657 2004-04-09 17:37:25Z mosu $
    \brief TTA output module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "mkvmerge.h"
#include "common.h"
#include "pr_generic.h"
#include "p_tta.h"
#include "matroska.h"

using namespace libmatroska;

static const double tta_frame_time = 1.04489795918367346939l;

tta_packetizer_c::tta_packetizer_c(generic_reader_c *nreader,
                                   int nchannels,
                                   int nbits_per_sample,
                                   int nsample_rate,
                                   track_info_c *nti)
  throw (error_c):
  generic_packetizer_c(nreader, nti) {
  sample_rate = nsample_rate;
  channels = nchannels;
  bits_per_sample = nbits_per_sample;
  samples_output = 0;

  set_track_type(track_audio);
  set_track_default_duration((int64_t)(1000000000.0 * ti->async.linear *
                                       tta_frame_time));
}

tta_packetizer_c::~tta_packetizer_c() {
}

void
tta_packetizer_c::set_headers() {
  set_codec_id(MKV_A_TTA);
  set_audio_sampling_freq((float)sample_rate);
  set_audio_channels(channels);
  set_audio_bit_depth(bits_per_sample);

  generic_packetizer_c::set_headers();
}

int
tta_packetizer_c::process(memory_c &mem,
                          int64_t,
                          int64_t,
                          int64_t,
                          int64_t) {
  debug_enter("tta_packetizer_c::process");

  add_packet(mem, samples_output * 1000000000 / sample_rate,
             (int64_t)(1000000000.0 * ti->async.linear * tta_frame_time));
  samples_output += (int64_t)(tta_frame_time * sample_rate);

  debug_leave("tta_packetizer_c::process");

  return EMOREDATA;
}

void
tta_packetizer_c::dump_debug_info() {
  mxdebug("tta_packetizer_c: queue: %d\n", packet_queue.size());
}

int
tta_packetizer_c::can_connect_to(generic_packetizer_c *src) {
  tta_packetizer_c *psrc;

  psrc = dynamic_cast<tta_packetizer_c *>(src);
  if (psrc == NULL)
    return CAN_CONNECT_NO_FORMAT;
  if ((sample_rate != psrc->sample_rate) ||
      (channels != psrc->channels) ||
      (bits_per_sample != psrc->bits_per_sample))
    return CAN_CONNECT_NO_PARAMETERS;
  return CAN_CONNECT_YES;
}
