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
    \version \$Id: p_pcm.cpp,v 1.7 2003/03/05 17:44:32 mosu Exp $
    \brief PCM output module
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/
 
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "queue.h"
#include "p_pcm.h"

#include "KaxTracks.h"
#include "KaxTrackAudio.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

pcm_packetizer_c::pcm_packetizer_c(unsigned long nsamples_per_sec,
                                   int nchannels, int nbits_per_sample,
                                   track_info_t *nti) throw (error_c):
  q_c(nti) {
  packetno = 0;
  bps = nchannels * nbits_per_sample * nsamples_per_sec / 8;
  tempbuf = (unsigned char *)malloc(bps + 128);
  if (tempbuf == NULL)
    die("malloc");
  tempbuf_size = bps;
  samples_per_sec = nsamples_per_sec;
  channels = nchannels;
  bits_per_sample = nbits_per_sample;
  bytes_output = 0;
  remaining_sync = 0;
  set_header();
}

pcm_packetizer_c::~pcm_packetizer_c() {
  if (tempbuf != NULL)
    free(tempbuf);
}

// void pcm_packetizer_c::reset() {
// }

#define APCM "A_PCM16IN"

void pcm_packetizer_c::set_header() {
  using namespace LIBMATROSKA_NAMESPACE;
  
  if (kax_last_entry == NULL)
    track_entry =
      &GetChild<KaxTrackEntry>(static_cast<KaxTracks &>(*kax_tracks));
  else
    track_entry =
      &GetNextChild<KaxTrackEntry>(static_cast<KaxTracks &>(*kax_tracks),
        static_cast<KaxTrackEntry &>(*kax_last_entry));
  kax_last_entry = track_entry;

  if (serialno <= 0)
    serialno = track_number++;
  KaxTrackNumber &tnumber =
    GetChild<KaxTrackNumber>(static_cast<KaxTrackEntry &>(*track_entry));
  *(static_cast<EbmlUInteger *>(&tnumber)) = serialno;
  
  *(static_cast<EbmlUInteger *>
    (&GetChild<KaxTrackType>(static_cast<KaxTrackEntry &>(*track_entry)))) =
      track_audio;

  KaxCodecID &codec_id =
    GetChild<KaxCodecID>(static_cast<KaxTrackEntry &>(*track_entry));
  if (bits_per_sample != 16) {
    fprintf(stderr, "Error: pcm_packetizer: Only files with 16bits per sample "
            "are supported at the moment.\n");
    exit(1);
  }
  codec_id.CopyBuffer((binary *)APCM, countof(APCM));

  KaxTrackAudio &track_audio =
    GetChild<KaxTrackAudio>(static_cast<KaxTrackEntry &>(*track_entry));

  KaxAudioSamplingFreq &kax_freq = GetChild<KaxAudioSamplingFreq>(track_audio);
  *(static_cast<EbmlFloat *>(&kax_freq)) = (float)samples_per_sec;
  
  KaxAudioChannels &kax_chans = GetChild<KaxAudioChannels>(track_audio);
  *(static_cast<EbmlUInteger *>(&kax_chans)) = channels;
}

int pcm_packetizer_c::process(unsigned char *buf, int size, int last_frame) {
  int i, bytes_per_packet, remaining_bytes, complete_packets;
  unsigned char *new_buf;

  if (size > tempbuf_size) { 
    tempbuf = (unsigned char *)realloc(tempbuf, size + 128);
    if (tempbuf == NULL)
      die("realloc");
    tempbuf_size = size;
  }

  new_buf = buf;

  if (ti->async.displacement != 0) {
    if (ti->async.displacement > 0) {
      // Add silence.
      int pad_size;

      pad_size = bps * ti->async.displacement / 1000;
      new_buf = (unsigned char *)malloc(size + pad_size);
      if (new_buf == NULL)
        die("malloc");
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
               (bytes_output * 1000 / bps) * ti->async.linear);
    bytes_output += bytes_per_packet;
    packetno++;
  }
  if (remaining_bytes != 0) {
    add_packet(new_buf + complete_packets * bytes_per_packet, remaining_bytes,
               (bytes_output * 1000 / bps) * ti->async.linear);
    bytes_output += remaining_bytes;
    packetno++;
  }

  if (new_buf != buf)
    free(new_buf);

  if (last_frame)
    return 0;
  else  
    return EMOREDATA;
}
