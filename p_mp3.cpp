/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_mp3.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: p_mp3.cpp,v 1.1 2003/02/16 17:04:38 mosu Exp $
    \brief MP3 output module
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "queue.h"
#include "mp3_common.h"
#include "p_mp3.h"

#include "KaxTracks.h"
#include "KaxTrackAudio.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

mp3_packetizer_c::mp3_packetizer_c(unsigned long nsamples_per_sec,
                                   int nchannels, int nmp3rate,
                                   audio_sync_t *nasync, range_t *nrange)
                                   throw (error_c) : q_c() {
  samples_per_sec = nsamples_per_sec;
  channels = nchannels;
  mp3rate = nmp3rate;
  bytes_output = 0;
  memcpy(&async, nasync, sizeof(audio_sync_t));
  memcpy(&range, nrange, sizeof(range_t));
  packet_buffer = NULL;
  buffer_size = 0;
  packetno = 0;
  set_header();
}

mp3_packetizer_c::~mp3_packetizer_c() {
  if (packet_buffer != NULL)
    free(packet_buffer);
}

void mp3_packetizer_c::add_to_buffer(char *buf, int size) {
  char *new_buffer;
  
  new_buffer = (char *)realloc(packet_buffer, buffer_size + size);
  if (new_buffer == NULL)
    die("realloc");
  
  memcpy(new_buffer + buffer_size, buf, size);
  packet_buffer = new_buffer;
  buffer_size += size;
}

int mp3_packetizer_c::mp3_packet_available() {
  unsigned long header;
  int           pos;
  mp3_header_t  mp3header;
  
  if (packet_buffer == NULL)
    return 0;
  pos = find_mp3_header(packet_buffer, buffer_size, &header);
  if (pos < 0)
    return 0;
  decode_mp3_header(header, &mp3header);
  if ((pos + mp3header.framesize + 4) > buffer_size)
    return 0;
  
  return 1;
}

void mp3_packetizer_c::remove_mp3_packet(int pos, int framesize) {
  int   new_size;
  char *temp_buf;
  
  new_size = buffer_size - (pos + framesize + 4) + 1;
  temp_buf = (char *)malloc(new_size);
  if (temp_buf == NULL)
    die("malloc");
  if (new_size != 0)
    memcpy(temp_buf, &packet_buffer[pos + framesize + 4 - 1], new_size);
  free(packet_buffer);
  packet_buffer = temp_buf;
  buffer_size = new_size;
}

char *mp3_packetizer_c::get_mp3_packet(unsigned long *header,
                                       mp3_header_t *mp3header) {
  int     pos;
  char   *buf;
  double  pims;
  
  if (packet_buffer == NULL)
    return 0;
  pos = find_mp3_header(packet_buffer, buffer_size, header);
  if (pos < 0)
    return 0;
  decode_mp3_header(*header, mp3header);
  if ((pos + mp3header->framesize + 4) > buffer_size)
    return 0;

  pims = 1000.0 * 1152.0 / mp3_freqs[mp3header->sampling_frequency];

  if (async.displacement < 0) {
    /*
     * MP3 audio synchronization. displacement < 0 means skipping an
     * appropriate number of packets at the beginning.
     */
    async.displacement += (int)pims;
    if (async.displacement > -(pims / 2))
      async.displacement = 0;
    
    remove_mp3_packet(pos, mp3header->framesize);
    
    return 0;
  }

  if ((verbose > 1) && (pos > 1))
    fprintf(stdout, "mp3_packetizer: skipping %d bytes (no valid MP3 header "
            "found).\n", pos);
  buf = (char *)malloc(mp3header->framesize + 4);
  if (buf == NULL)
    die("malloc");
  memcpy(buf, packet_buffer + pos, mp3header->framesize + 4);
  
  if (async.displacement > 0) {
    /*
     * MP3 audio synchronization. displacement > 0 is solved by creating
     * silent MP3 packets and repeating it over and over again (well only as
     * often as necessary of course. Wouldn't want to spoil your movie by
     * providing a silent MP3 stream ;)).
     */
    async.displacement -= (int)pims;
    if (async.displacement < (pims / 2))
      async.displacement = 0;
    memset(buf + 4, 0, mp3header->framesize);
    
    return buf;
  }

  remove_mp3_packet(pos, mp3header->framesize);

  return buf;
}

void mp3_packetizer_c::set_header() {
  using namespace LIBMATROSKA_NAMESPACE;
  
  if (kax_last_entry == NULL)
    track_entry =
      &GetChild<KaxTrackEntry>(static_cast<KaxTracks &>(*kax_tracks));
  else
    track_entry =
      &GetNextChild<KaxTrackEntry>(static_cast<KaxTracks &>(*kax_tracks),
        static_cast<KaxTrackEntry &>(*kax_last_entry));
  kax_last_entry = track_entry;
  
  serialno = track_number++;
  KaxTrackNumber &tnumber =
    GetChild<KaxTrackNumber>(static_cast<KaxTrackEntry &>(*track_entry));
  *(static_cast<EbmlUInteger *>(&tnumber)) = serialno;
  
  *(static_cast<EbmlUInteger *>
    (&GetChild<KaxTrackType>(static_cast<KaxTrackEntry &>(*track_entry)))) =
      track_audio;

  KaxCodecID &codec_id =
    GetChild<KaxCodecID>(static_cast<KaxTrackEntry &>(*track_entry));
  codec_id.CopyBuffer((binary *)"A_MPEGLAYER3", countof("A_MPEGLAYER3"));

  KaxTrackAudio &track_audio =
    GetChild<KaxTrackAudio>(static_cast<KaxTrackEntry &>(*track_entry));

  KaxAudioSamplingFreq &kax_freq = GetChild<KaxAudioSamplingFreq>(track_audio);
  *(static_cast<EbmlFloat *>(&kax_freq)) = (float)samples_per_sec;
  
  KaxAudioChannels &kax_chans = GetChild<KaxAudioChannels>(track_audio);
  *(static_cast<EbmlUInteger *>(&kax_chans)) = channels;
}

int mp3_packetizer_c::process(char *buf, int size, int last_frame) {
  char          *packet;
  unsigned long  header;
  mp3_header_t   mp3header;

  if (packetno == 0)
    set_header();

  add_to_buffer(buf, size);
  while ((packet = get_mp3_packet(&header, &mp3header)) != NULL) {
    if ((4 - ((header >> 17) & 3)) != 3) {
      fprintf(stdout, "Warning: p_mp3: packet is not a valid MP3 packet (" \
              "packet number %lld)\n", packetno - 2);
      return EMOREDATA;
    }  

    add_packet(packet, mp3header.framesize + 4,
               (u_int64_t)(1000.0 * packetno * 1152 * async.linear / 
               samples_per_sec));
    packetno++;
    free(packet);
  }

  if (last_frame)
    return 0;
  else
    return EMOREDATA;
}
