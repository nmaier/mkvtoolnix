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
                                   int nchannels, track_info_c *nti)
  throw (error_c):
  generic_packetizer_c(nreader, nti), byte_buffer(128 * 1024) {
  samples_per_sec = nsamples_per_sec;
  channels = nchannels;
  bytes_output = 0;
  packetno = 0;
  spf = 1152;

  set_track_type(track_audio);
  set_track_default_duration_ns((int64_t)(1152000000000.0 * ti->async.linear /
                                          samples_per_sec));
  duplicate_data_on_add(false);
}

mp3_packetizer_c::~mp3_packetizer_c() {
}

unsigned char *mp3_packetizer_c::get_mp3_packet(mp3_header_t *mp3header) {
  int pos, size;
  unsigned char *buf;
  double pims;
  string codec_id;

  if (byte_buffer.get_size() == 0)
    return 0;
  while (1) {
    buf = byte_buffer.get_buffer();
    size = byte_buffer.get_size();
    pos = find_mp3_header(buf, size);
    if (pos < 0)
      return NULL;
    decode_mp3_header(&buf[pos], mp3header);
    if ((pos + mp3header->framesize) > size)
      return NULL;
    if (!mp3header->is_tag)
      break;

    mxverb(2, "mp3_packetizer: Removing TAG packet with size %d\n",
           mp3header->framesize);
    byte_buffer.remove(mp3header->framesize + pos);
  }

  if (packetno == 0) {
    spf = mp3header->samples_per_channel;
    codec_id = MKV_A_MP3;
    codec_id[codec_id.length() - 1] = (char)(mp3header->layer + '0');
    *(static_cast<EbmlString *>
      (&GetChild<KaxCodecID>(*track_entry))) = codec_id;
    if (spf != 1152) {
      set_track_default_duration_ns((int64_t)(1000000000.0 * spf *
                                              ti->async.linear /
                                              samples_per_sec));
      *(static_cast<EbmlUInteger *>
        (&GetChild<KaxTrackDefaultDuration>(*track_entry))) =
        (int64_t)(1000000000.0 * spf * ti->async.linear / samples_per_sec);
    }
    rerender_headers(out);
  }

  if ((pos + mp3header->framesize) > byte_buffer.get_size())
    return NULL;

  pims = 1000.0 * (float)spf / mp3header->sampling_frequency;

  if (needs_negative_displacement(pims)) {
    /*
     * MP3 audio synchronization. displacement < 0 means skipping an
     * appropriate number of packets at the beginning.
     */
    displace(-pims);
    byte_buffer.remove(mp3header->framesize + pos);

    return NULL;
  }

  if (pos > 0) {
    if (verbose)
      mxwarn("mp3_packetizer: skipping %d bytes (no valid MP3 header found)."
             "\n", pos);
    byte_buffer.remove(pos);
    pos = 0;
  }
  if (fast_mode)
    buf = (unsigned char *)safemalloc(mp3header->framesize);
  else
    buf = (unsigned char *)safememdup(byte_buffer.get_buffer(),
                                      mp3header->framesize);

  if (needs_positive_displacement(pims)) {
    /*
     * MP3 audio synchronization. displacement > 0 is solved by creating
     * silent MP3 packets and repeating it over and over again (well only as
     * often as necessary of course. Wouldn't want to spoil your movie by
     * providing a silent MP3 stream ;)).
     */
    displace(pims);
    memset(buf + 4, 0, mp3header->framesize - 4);

    return buf;
  }

  byte_buffer.remove(mp3header->framesize);

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
  mp3_header_t mp3header;
  int64_t my_timecode;

  debug_enter("mp3_packetizer_c::process");

  byte_buffer.add(buf, size);
  while ((packet = get_mp3_packet(&mp3header)) != NULL) {
#ifdef DEBUG
    dump_packet(packet, mp3header.framesize);
#endif    

    if (timecode == -1)
      my_timecode = (int64_t)(1000.0 * packetno * spf / samples_per_sec);
    else
      my_timecode = timecode + ti->async.displacement;
    my_timecode = (int64_t)(my_timecode * ti->async.linear);
    add_packet(packet, mp3header.framesize, my_timecode,
               (int64_t)(1000.0 * spf * ti->async.linear / samples_per_sec));
    packetno++;
  }

  debug_leave("mp3_packetizer_c::process");

  return EMOREDATA;
}

void mp3_packetizer_c::dump_debug_info() {
  mxdebug("mp3_packetizer_c: queue: %d; buffer_size: %d\n",
          packet_queue.size(), byte_buffer.get_size());
}
