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
    \version \$Id: p_pcm.cpp,v 1.20 2003/05/05 18:37:36 mosu Exp $
    \brief PCM output module
    \author Moritz Bunkus         <moritz @ bunkus.org>
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

using namespace LIBMATROSKA_NAMESPACE;

pcm_packetizer_c::pcm_packetizer_c(unsigned long nsamples_per_sec,
                                   int nchannels, int nbits_per_sample,
                                   track_info_t *nti) throw (error_c):
  generic_packetizer_c(nti) {
  packetno = 0;
  bps = nchannels * nbits_per_sample * nsamples_per_sec / 8;
  tempbuf = (unsigned char *)safemalloc(bps + 128);
  tempbuf_size = bps;
  samples_per_sec = nsamples_per_sec;
  channels = nchannels;
  bits_per_sample = nbits_per_sample;
  bytes_output = 0;
  remaining_sync = 0;
}

pcm_packetizer_c::~pcm_packetizer_c() {
  if (tempbuf != NULL)
    safefree(tempbuf);
}

void pcm_packetizer_c::set_headers() {
  set_serial(-1);
  set_track_type(track_audio);
  set_codec_id(MKV_A_PCM);
  set_audio_sampling_freq((float)samples_per_sec);
  set_audio_channels(channels);
  set_audio_bit_depth(bits_per_sample);

  if (ti->default_track)
    set_as_default_track('a');

  generic_packetizer_c::set_headers();
}

int pcm_packetizer_c::process(unsigned char *buf, int size,
                              int64_t, int64_t, int64_t, int64_t) {
  int i, bytes_per_packet, remaining_bytes, complete_packets;
  unsigned char *new_buf;

  if (size > tempbuf_size) { 
    tempbuf = (unsigned char *)saferealloc(tempbuf, size + 128);
    tempbuf_size = size;
  }

  new_buf = buf;

  if (ti->async.displacement != 0) {
    if (ti->async.displacement > 0) {
      // Add silence.
      int pad_size;

      pad_size = bps * ti->async.displacement / 1000;
      new_buf = (unsigned char *)safemalloc(size + pad_size);
      memset(new_buf, 0, pad_size);
      memcpy(&new_buf[pad_size], buf, size);
      size += pad_size;
    } else
      // Skip bytes.
      remaining_sync = -1 * bps * ti->async.displacement / 1000;
    ti->async.displacement = 0;
  }

  if (remaining_sync > 0) {
    if (remaining_sync > size) {
      remaining_sync -= size;
      return EMOREDATA;
    }
    memmove(buf, &buf[remaining_sync], size - remaining_sync);
    size -= remaining_sync;
    remaining_sync = 0;
  }

  bytes_per_packet = bps / pcm_interleave;
  complete_packets = size / bytes_per_packet;
  remaining_bytes = size % bytes_per_packet;

  for (i = 0; i < complete_packets; i++) {
    add_packet(new_buf + i * bytes_per_packet, bytes_per_packet,
               (int64_t)((bytes_output * 1000 / bps) * ti->async.linear));
    bytes_output += bytes_per_packet;
    packetno++;
  }
  if (remaining_bytes != 0) {
    add_packet(new_buf + complete_packets * bytes_per_packet, remaining_bytes,
               (int64_t)((bytes_output * 1000 / bps) * ti->async.linear));
    bytes_output += remaining_bytes;
    packetno++;
  }

  if (new_buf != buf)
    safefree(new_buf);

  return EMOREDATA;
}
