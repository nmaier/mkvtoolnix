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
    \version $Id$
    \brief MP3 output module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "matroska.h"
#include "mkvmerge.h"
#include "mp3_common.h"
#include "pr_generic.h"
#include "p_mp3.h"

using namespace libmatroska;

mp3_packetizer_c::mp3_packetizer_c(generic_reader_c *nreader,
                                   unsigned long nsamples_per_sec,
                                   int nchannels, int nlayer,
                                   track_info_t *nti)
  throw (error_c): generic_packetizer_c(nreader, nti) {
  samples_per_sec = nsamples_per_sec;
  channels = nchannels;
  layer = nlayer;
  bytes_output = 0;
  packet_buffer = NULL;
  buffer_size = 0;
  packetno = 0;
  spf = 1152;

  set_track_type(track_audio);
  if (use_durations)
    set_track_default_duration_ns((int64_t)(1152000000000.0 *
                                            ti->async.linear /
                                            samples_per_sec));
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

void mp3_packetizer_c::remove_mp3_packet(int pos, int framesize) {
  int new_size;
  unsigned char *temp_buf;

  new_size = buffer_size - (pos + framesize) + 1;
  temp_buf = (unsigned char *)safemalloc(new_size);
  if (new_size != 0)
    memcpy(temp_buf, &packet_buffer[pos + framesize - 1], new_size);
  safefree(packet_buffer);
  packet_buffer = temp_buf;
  buffer_size = new_size;
}

unsigned char *mp3_packetizer_c::get_mp3_packet(mp3_header_t *mp3header) {
  int pos;
  unsigned char *buf;
  double pims;

  if (packet_buffer == NULL)
    return 0;
  while (1) {
    pos = find_mp3_header(packet_buffer, buffer_size);
    if (pos < 0)
      return NULL;
    decode_mp3_header(&packet_buffer[pos], mp3header);
    if ((pos + mp3header->framesize) > buffer_size)
      return NULL;
    if (!mp3header->is_tag)
      break;

    mxverb(2, "mp3_packetizer: Removing TAG packet with size %d\n",
           mp3header->framesize);
    remove_mp3_packet(pos, mp3header->framesize);
  }

  if (packetno == 0) {
    spf = mp3header->samples_per_channel;
    if ((spf != 1152) && use_durations) {
      set_track_default_duration_ns((int64_t)(1000000000.0 * spf *
                                              ti->async.linear /
                                              samples_per_sec));
      *(static_cast<EbmlUInteger *>
        (&GetChild<KaxTrackDefaultDuration>(*track_entry))) =
        (int64_t)(1000000000.0 * spf * ti->async.linear / samples_per_sec);
      rerender_headers(out);
    }
  }

  if ((pos + mp3header->framesize) > buffer_size)
    return NULL;

  pims = 1000.0 * (float)spf / mp3header->sampling_frequency;

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
    mxwarn("mp3_packetizer: skipping %d bytes (no valid MP3 header found).\n",
           pos);
  buf = (unsigned char *)safememdup(packet_buffer + pos, mp3header->framesize);

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
    memset(buf + 4, 0, mp3header->framesize - 4);

    return buf;
  }

  remove_mp3_packet(pos, mp3header->framesize);

  return buf;
}

void mp3_packetizer_c::set_headers() {
  string codec_id;

  codec_id = MKV_A_MP3;
  codec_id[codec_id.length() - 1] = (char)(layer + '0');
  set_codec_id(codec_id.c_str());
  set_audio_sampling_freq((float)samples_per_sec);
  set_audio_channels(channels);

  generic_packetizer_c::set_headers();
}

int mp3_packetizer_c::process(unsigned char *buf, int size,
                              int64_t timecode, int64_t, int64_t, int64_t) {
  unsigned char *packet;
  mp3_header_t mp3header;
  int64_t my_timecode, old_packetno;

  debug_enter("mp3_packetizer_c::process");

  if (timecode != -1)
    my_timecode = timecode;

  add_to_buffer(buf, size);
  while ((packet = get_mp3_packet(&mp3header)) != NULL) {
    old_packetno = packetno;
    packetno++;

#ifdef DEBUG
    dump_packet(packet, mp3header.framesize);
#endif    

    if (timecode == -1)
      my_timecode = (int64_t)(1000.0 * old_packetno * spf * ti->async.linear /
                              samples_per_sec);

    add_packet(packet, mp3header.framesize, my_timecode,
               (int64_t)(1000.0 * spf * ti->async.linear / samples_per_sec));
  }

  debug_leave("mp3_packetizer_c::process");

  return EMOREDATA;
}

void mp3_packetizer_c::dump_debug_info() {
  mxdebug("mp3_packetizer_c: queue: %d; buffer_size: %d\n",
          packet_queue.size(), buffer_size);
}
