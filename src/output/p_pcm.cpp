/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_pcm.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief PCM output module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "mkvmerge.h"
#include "common.h"
#include "pr_generic.h"
#include "p_pcm.h"
#include "matroska.h"

using namespace libmatroska;

pcm_packetizer_c::pcm_packetizer_c(generic_reader_c *nreader,
                                   unsigned long nsamples_per_sec,
                                   int nchannels,
                                   int nbits_per_sample,
                                   track_info_c *nti,
                                   bool nbig_endian)
  throw (error_c):
  generic_packetizer_c(nreader, nti) {
  int i;

  packetno = 0;
  bps = nchannels * nbits_per_sample * nsamples_per_sec / 8;
  samples_per_sec = nsamples_per_sec;
  channels = nchannels;
  bits_per_sample = nbits_per_sample;
  bytes_output = 0;
  skip_bytes = 0;
  big_endian = nbig_endian;
  for (i = 32; i > 2; i >>= 2)
    if ((samples_per_sec % i) == 0)
      break;
  if ((i == 2) && ((packet_size % 5) == 0))
    i = 5;
  packet_size = samples_per_sec / i;

  set_track_type(track_audio);
  set_track_default_duration((int64_t)(1000000000.0 * ti->async.linear *
                                          packet_size / samples_per_sec));

  packet_size *= channels * bits_per_sample / 8;
}

pcm_packetizer_c::~pcm_packetizer_c() {
}

void
pcm_packetizer_c::set_headers() {
  if (big_endian)
    set_codec_id(MKV_A_PCM_BE);
  else
    set_codec_id(MKV_A_PCM);
  set_audio_sampling_freq((float)samples_per_sec);
  set_audio_channels(channels);
  set_audio_bit_depth(bits_per_sample);

  generic_packetizer_c::set_headers();
}

int
pcm_packetizer_c::process(memory_c &mem,
                          int64_t,
                          int64_t,
                          int64_t,
                          int64_t) {
  unsigned char *new_buf;

  debug_enter("pcm_packetizer_c::process");

  if (initial_displacement != 0) {
    if (initial_displacement > 0) {
      // Add silence.
      int pad_size;

      pad_size = bps * initial_displacement / 1000000000;
      new_buf = (unsigned char *)safemalloc(pad_size);
      memset(new_buf, 0, pad_size);
      buffer.add(new_buf, pad_size);
      safefree(new_buf);
    } else
      // Skip bytes.
      skip_bytes = -1 * bps * initial_displacement / 1000000000;
    initial_displacement = 0;
  }

  if (skip_bytes) {
    if (skip_bytes > mem.size) {
      skip_bytes -= mem.size;
      return EMOREDATA;
    }
    mem.size -= skip_bytes;
    new_buf = &mem.data[skip_bytes];
    skip_bytes = 0;
  } else
    new_buf = mem.data;

  buffer.add(new_buf, mem.size);

  while (buffer.get_size() >= packet_size) {
    memory_c mem(buffer.get_buffer(), packet_size, false);
    add_packet(mem, bytes_output * 1000000000 / bps, packet_size * 1000000000 /
               bps);
    buffer.remove(packet_size);
    bytes_output += packet_size;
  }

  debug_leave("pcm_packetizer_c::process");

  return EMOREDATA;
}

void
pcm_packetizer_c::flush() {
  uint32_t size;

  size = buffer.get_size();
  if (size > 0) {
    memory_c mem(buffer.get_buffer(), size, false);
    add_packet(mem, bytes_output * 1000000000 / bps,
               size * 1000000000 / bps);
    bytes_output += size;
    buffer.remove(size);
  }
}

void
pcm_packetizer_c::dump_debug_info() {
  mxdebug("pcm_packetizer_c: queue: %d\n", packet_queue.size());
}

int
pcm_packetizer_c::can_connect_to(generic_packetizer_c *src) {
  pcm_packetizer_c *psrc;

  psrc = dynamic_cast<pcm_packetizer_c *>(src);
  if (psrc == NULL)
    return CAN_CONNECT_NO_FORMAT;
  if ((samples_per_sec != psrc->samples_per_sec) ||
      (channels != psrc->channels) ||
      (bits_per_sample != psrc->bits_per_sample))
    return CAN_CONNECT_NO_PARAMETERS;
  return CAN_CONNECT_YES;
}
