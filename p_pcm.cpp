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
    \version \$Id: p_pcm.cpp,v 1.1 2003/02/24 13:12:27 mosu Exp $
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

pcm_packetizer_c::pcm_packetizer_c(void *nprivate_data, int nprivate_size,
                                   unsigned long nsamples_per_sec,
                                   int nchannels, int nbits_per_sample,
                                   audio_sync_t *nasync, range_t *nrange)
                                   throw (error_c) : q_c() {
  packetno = 0;
  bps = nchannels * nbits_per_sample * nsamples_per_sec / 8;
  tempbuf = (char *)malloc(bps + 128);
  if (tempbuf == NULL)
    die("malloc");
  samples_per_sec = nsamples_per_sec;
  channels = nchannels;
  bits_per_sample = nbits_per_sample;
  bytes_output = 0;
  memcpy(&async, nasync, sizeof(audio_sync_t));
  memcpy(&range, nrange, sizeof(range_t));
  set_private_data(nprivate_data, nprivate_size);
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
    fprintf(stderr, "pcm_packetizer: Only files with 16bits per sample "
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

int pcm_packetizer_c::process(char *buf, int size, int last_frame) {
  int i, bytes_per_packet, remaining_bytes, complete_packets;

  if (size > bps) { 
    fprintf(stderr, "FATAL: pcm_packetizer: size (%d) > bps (%d)\n", size,
            bps);
    exit(1);
  }

  bytes_per_packet = bps / pcm_interleave;
  complete_packets = size / bytes_per_packet;
  remaining_bytes = size % bytes_per_packet;

  for (i = 0; i < complete_packets; i++) {
    add_packet(buf + i * bytes_per_packet, bytes_per_packet,
               bytes_output * 1000 / bps);
    bytes_output += bytes_per_packet;
  }
  if (remaining_bytes != 0) {
    add_packet(buf + complete_packets * bytes_per_packet, remaining_bytes,
               bytes_output * 1000 / bps);
    bytes_output += remaining_bytes;
  }

  if (last_frame)
    return 0;
  else  
    return EMOREDATA;
}
