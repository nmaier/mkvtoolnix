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
    \version \$Id$
    \brief MP3 output module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "pr_generic.h"
#include "mp3_common.h"
#include "p_mp3.h"
#include "matroska.h"

using namespace LIBMATROSKA_NAMESPACE;

mp3_packetizer_c::mp3_packetizer_c(generic_reader_c *nreader,
                                   unsigned long nsamples_per_sec,
                                   int nchannels, track_info_t *nti)
  throw (error_c): generic_packetizer_c(nreader, nti) {
  samples_per_sec = nsamples_per_sec;
  channels = nchannels;
  bytes_output = 0;
  packet_buffer = NULL;
  buffer_size = 0;
  packetno = 0;

  set_track_type(track_audio);
  duplicate_data_on_add(false);
}

mp3_packetizer_c::~mp3_packetizer_c() {
  if (packet_buffer != NULL)
    safefree(packet_buffer);
}

void mp3_packetizer_c::add_to_buffer(unsigned char *buf, int size) {
  unsigned char *new_buffer;

  new_buffer = (unsigned char *)saferealloc(packet_buffer, buffer_size + size);

  memcpy(new_buffer + buffer_size, buf, size);
  packet_buffer = new_buffer;
  buffer_size += size;
}

int mp3_packetizer_c::mp3_packet_available() {
  unsigned long header;
  int pos;
  mp3_header_t mp3header;

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
  int new_size;
  unsigned char *temp_buf;

  new_size = buffer_size - (pos + framesize + 4) + 1;
  temp_buf = (unsigned char *)safemalloc(new_size);
  if (new_size != 0)
    memcpy(temp_buf, &packet_buffer[pos + framesize + 4 - 1], new_size);
  safefree(packet_buffer);
  packet_buffer = temp_buf;
  buffer_size = new_size;
}

unsigned char *mp3_packetizer_c::get_mp3_packet(unsigned long *header,
                                                mp3_header_t *mp3header) {
  int pos;
  unsigned char *buf;
  double pims;

  if (packet_buffer == NULL)
    return 0;
  pos = find_mp3_header(packet_buffer, buffer_size, header);
  if (pos < 0)
    return 0;
  decode_mp3_header(*header, mp3header);
  if ((pos + mp3header->framesize + 4) > buffer_size)
    return 0;

  pims = 1000.0 * 1152.0 / mp3_freqs[mp3header->sampling_frequency];

  if (ti->async.displacement < 0) {
    /*
     * MP3 audio synchronization. displacement < 0 means skipping an
     * appropriate number of packets at the beginning.
     */
    ti->async.displacement += (int)pims;
    if (ti->async.displacement > -(pims / 2))
      ti->async.displacement = 0;

    remove_mp3_packet(pos, mp3header->framesize);

    return 0;
  }

  if ((verbose > 1) && (pos > 1))
    fprintf(stdout, "mp3_packetizer: skipping %d bytes (no valid MP3 header "
            "found).\n", pos);
  buf = (unsigned char *)safememdup(packet_buffer + pos, mp3header->framesize
                                    + 4);

  if (ti->async.displacement > 0) {
    /*
     * MP3 audio synchronization. displacement > 0 is solved by creating
     * silent MP3 packets and repeating it over and over again (well only as
     * often as necessary of course. Wouldn't want to spoil your movie by
     * providing a silent MP3 stream ;)).
     */
    ti->async.displacement -= (int)pims;
    if (ti->async.displacement < (pims / 2))
      ti->async.displacement = 0;
    memset(buf + 4, 0, mp3header->framesize);

    return buf;
  }

  remove_mp3_packet(pos, mp3header->framesize);

  return buf;
}

void mp3_packetizer_c::set_headers() {
  set_codec_id(MKV_A_MP3);
  set_audio_sampling_freq((float)samples_per_sec);
  set_audio_channels(channels);

  generic_packetizer_c::set_headers();
}

int mp3_packetizer_c::process(unsigned char *buf, int size,
                              int64_t timecode, int64_t, int64_t, int64_t) {
  unsigned char *packet;
  unsigned long header;
  mp3_header_t mp3header;
  int64_t my_timecode;

  debug_enter("mp3_packetizer_c::process");

  if (timecode != -1)
    my_timecode = timecode;

  add_to_buffer(buf, size);
  while ((packet = get_mp3_packet(&header, &mp3header)) != NULL) {
    packetno++;

    if ((4 - ((header >> 17) & 3)) != 3) {
      fprintf(stdout, "Warning: p_mp3: packet is not a valid MP3 packet ("
              "packet number %lld)\n", packetno);
      safefree(packet);
      packetno++;
      debug_leave("mp3_packetizer_c::process");
      return EMOREDATA;
    }

    if (timecode == -1)
      my_timecode = (int64_t)(1000.0 * packetno * 1152 * ti->async.linear /
                              samples_per_sec);

    add_packet(packet, mp3header.framesize + 4, my_timecode,
               (int64_t)(1000.0 * 1152 * ti->async.linear / samples_per_sec));
  }

  debug_leave("mp3_packetizer_c::process");

  return EMOREDATA;
}

void mp3_packetizer_c::dump_debug_info() {
  fprintf(stderr, "DBG> mp3_packetizer_c: queue: %d; buffer_size: %d\n",
          packet_queue.size(), buffer_size);
}
