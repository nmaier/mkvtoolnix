/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_vorbis.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: p_vorbis.cpp,v 1.4 2003/03/04 10:15:33 mosu Exp $
    \brief Vorbis packetizer
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include "config.h"

#ifdef HAVE_OGGVORBIS

#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include "common.h"
#include "pr_generic.h"
#include "p_vorbis.h"
// #include "vorbis_header_utils.h"

#include "KaxTracks.h"
#include "KaxTrackAudio.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

vorbis_packetizer_c::vorbis_packetizer_c(audio_sync_t *nasync,
                                         unsigned char *d_header, int l_header,
                                         unsigned char *d_comments,
                                         int l_comments,
                                         unsigned char *d_codecsetup,
                                         int l_codecsetup)
  throw (error_c) : q_c() {
  int i;

  packetno = 0;
  last_bs = 0;
  samples = 0;
  memcpy(&async, nasync, sizeof(audio_sync_t));
  memset(headers, 0, 3 * sizeof(ogg_packet));
  headers[0].packet = (unsigned char *)malloc(l_header);
  headers[1].packet = (unsigned char *)malloc(l_comments);
  headers[2].packet = (unsigned char *)malloc(l_codecsetup);
  if ((headers[0].packet == NULL) || (headers[1].packet == NULL) ||
      (headers[2].packet == NULL))
    die("malloc");
  memcpy(headers[0].packet, d_header, l_header);
  memcpy(headers[1].packet, d_comments, l_comments);
  memcpy(headers[2].packet, d_codecsetup, l_codecsetup);
  headers[0].bytes = l_header;
  headers[1].bytes = l_comments;
  headers[2].bytes = l_codecsetup;
  headers[0].b_o_s = 1;
  headers[1].packetno = 1;
  headers[2].packetno = 2;
  vorbis_info_init(&vi);
  vorbis_comment_init(&vc);
  for (i = 0; i < 3; i++)
    if (vorbis_synthesis_headerin(&vi, &vc, &headers[i]) < 0)
      throw error_c("Error: vorbis_packetizer: Could not extract the "
                    "stream's parameters from the first packets.\n");
  set_header();
}

vorbis_packetizer_c::~vorbis_packetizer_c() {
  int i;

  for (i = 0; i < 3; i++)
    if (headers[i].packet != NULL)
      free(headers[i].packet);
}

#define AVORBIS "A_VORBIS"

void vorbis_packetizer_c::set_header() {
  using namespace LIBMATROSKA_NAMESPACE;
  unsigned char *buffer;
  int n, offset, i, lsize;
  
  if (kax_last_entry == NULL)
    track_entry =
      &GetChild<KaxTrackEntry>(static_cast<KaxTracks &>(*kax_tracks));
  else
    track_entry =
      &GetNextChild<KaxTrackEntry>(static_cast<KaxTracks &>(*kax_tracks),
        static_cast<KaxTrackEntry &>(*kax_last_entry));
  kax_last_entry = track_entry;

  if (serialno == -1)
    serialno = track_number++;
  KaxTrackNumber &tnumber =
    GetChild<KaxTrackNumber>(static_cast<KaxTrackEntry &>(*track_entry));
  *(static_cast<EbmlUInteger *>(&tnumber)) = serialno;
  
  *(static_cast<EbmlUInteger *>
    (&GetChild<KaxTrackType>(static_cast<KaxTrackEntry &>(*track_entry)))) =
      track_audio;

  KaxCodecID &codec_id =
    GetChild<KaxCodecID>(static_cast<KaxTrackEntry &>(*track_entry));
  codec_id.CopyBuffer((binary *)AVORBIS, countof(AVORBIS));

  KaxTrackAudio &track_audio =
    GetChild<KaxTrackAudio>(static_cast<KaxTrackEntry &>(*track_entry));

  KaxAudioSamplingFreq &kax_freq = GetChild<KaxAudioSamplingFreq>(track_audio);
  *(static_cast<EbmlFloat *>(&kax_freq)) = (float)vi.rate;
  
  KaxAudioChannels &kax_chans = GetChild<KaxAudioChannels>(track_audio);
  *(static_cast<EbmlUInteger *>(&kax_chans)) = vi.channels;

  // We use lacing for the blocks. The first bytes is the number of
  // packets being laced. For each packet in the lace there's the length
  // coded like this:
  // length = 0
  // while (next byte == 255) { length += 255 }
  // length += this byte which is < 255
  // The last packet's length can be calculated by the length of
  // the KaxCodecPrivate and all prior packets, so there's no length for it.
  lsize = 1 + (headers[0].bytes / 255) + 1 + (headers[1].bytes / 255) + 1 +
    headers[0].bytes + headers[1].bytes + headers[2].bytes;
  buffer = (unsigned char *)malloc(lsize);
  if (buffer == NULL)
    die("malloc");

  buffer[0] = 3;                // The number of packets.
  offset = 1;
  for (i = 0; i < 2; i++) {
    for (n = headers[i].bytes; n >= 255; n -= 255) {
      buffer[offset] = 255;
      offset++;
    }
    buffer[offset] = n;
    offset++;
    memcpy(&buffer[offset], headers[i].packet, headers[i].bytes);
    offset += headers[i].bytes;
  }
  memcpy(&buffer[offset], headers[2].packet, headers[2].bytes);

  KaxCodecPrivate &codec_private = 
    GetChild<KaxCodecPrivate>(static_cast<KaxTrackEntry &>(*track_entry));
  codec_private.CopyBuffer((binary *)buffer, lsize);

  free(buffer);
}

/* 
 * Some notes - processing is straight-forward if no AV synchronization
 * is needed - the packet is simply stored in the Matroska file.
 * Unfortunately things are not that easy if AV sync is done. For a
 * negative displacement packets are simply discarded if their timecode
 * is set before the displacement. For positive displacements the packetizer
 * has to generate silence packets and put them into the Matroska file first.
 */
int vorbis_packetizer_c::process(unsigned char *data, int size,
                                 int64_t timecode) {
  unsigned char zero[2];
  ogg_packet op;
  u_int64_t this_bs, samples_here, samples_needed;
  
  // Recalculate the timecode if needed.
  if (timecode == -1)
    timecode = samples * 1000 / vi.rate;

  // Positive displacement, first packet? Well then lets create silence.
  if ((packetno == 0) && (async.displacement > 0)) {
    // Create a fake packet so we can use vorbis_packet_blocksize().
    zero[0] = 0;
    zero[1] = 0;
    memset(&op, 0, sizeof(ogg_packet));
    op.packet = zero;
    op.bytes = 2;

    // Calculate how many samples we have to create.
    samples_needed = vi.rate * 1000 / async.displacement;

    this_bs = vorbis_packet_blocksize(&vi, &op);
    samples_here = (this_bs + last_bs) / 4;
    while ((samples + samples_here) < samples_needed) {
      samples += samples_here;
      last_bs = this_bs;
      samples_here = (this_bs + last_bs) / 4;
      add_packet(zero, 2, samples * 1000 / vi.rate);
    }

    async.displacement = 0;
  }

  // Update the number of samples we have processed so that we can
  // calculate the timecode on the next call.
  op.packet = data;
  op.bytes = size;
  this_bs = vorbis_packet_blocksize(&vi, &op);
  samples_here = (this_bs + last_bs) / 4;
  samples += samples_here;
  last_bs = this_bs;

  // Handle the displacement.
  timecode += async.displacement;

  // Handle the linear sync - simply multiply with the given factor.
  timecode = (int64_t)((double)timecode * async.linear);

  // If a negative sync value was used we may have to skip this packet.
  if (timecode < 0)
    return EMOREDATA;

  add_packet(data, size, (u_int64_t)timecode);

  return EMOREDATA;
}
 
#endif // HAVE_OGGVORBIS
