/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * MP3 output module
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "matroska.h"
#include "mp3_common.h"
#include "output_control.h"
#include "p_mp3.h"

using namespace libmatroska;

mp3_packetizer_c::mp3_packetizer_c(generic_reader_c *nreader,
                                   unsigned long nsamples_per_sec,
                                   int nchannels,
                                   bool source_is_good,
                                   track_info_c *nti)
  throw (error_c):
  generic_packetizer_c(nreader, nti), byte_buffer(128 * 1024) {
  samples_per_sec = nsamples_per_sec;
  channels = nchannels;
  bytes_output = 0;
  packetno = 0;
  spf = 1152;
  codec_id_set = false;
  valid_headers_found = source_is_good;

  set_track_type(track_audio);
  set_track_default_duration((int64_t)(1152000000000.0 * ti->async.linear /
                                          samples_per_sec));
  enable_avi_audio_sync(true);
}

mp3_packetizer_c::~mp3_packetizer_c() {
}

void
mp3_packetizer_c::handle_garbage(int64_t bytes) {
  bool warning_printed;

  warning_printed = false;
  if (packetno == 0) {
    int64_t offset;

    offset = handle_avi_audio_sync(bytes, !(ti->avi_block_align % 384) ||
                                   !(ti->avi_block_align % 576));
    if (offset != -1) {
      mxinfo("The MPEG audio track %lld from '%s' contained %lld bytes of "
             "garbage at the beginning. This corresponds to a delay of "
             "%lldms. This delay will be used instead of the garbage data."
             "\n", ti->id, ti->fname.c_str(), bytes, offset / 1000000);
      warning_printed = true;
    }
  }
  if (!warning_printed)
    mxwarn("The MPEG audio track %lld from '%s' contained %lld bytes of "
           "garbage at the beginning which were skipped. The audio/video "
           "synchronization may have been lost.\n", ti->id, ti->fname.c_str(),
           bytes);
}

unsigned char *
mp3_packetizer_c::get_mp3_packet(mp3_header_t *mp3header) {
  int pos, size;
  unsigned char *buf;
  double pins;
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

  // Try to be smart. We might get fed trash before the very first MP3
  // header. And now a user has presented streams in which the trash
  // contains valid MP3 headers before the 'real' ones...
  // Screw the guys who program apps that use _random_ _trash_ for filling
  // gaps. Screw those who try to use AVI no matter the 'cost'!
  if (!valid_headers_found) {
    pos = find_consecutive_mp3_headers(byte_buffer.get_buffer(),
                                       byte_buffer.get_size(), 5);
    if (pos >= 0) {
      // Great, we have found five consecutive identical headers. Be happy
      // with those!
      valid_headers_found = true;
      if (pos > 0) {
        handle_garbage(pos);
        byte_buffer.remove(pos);
      }
      pos = 0;
      decode_mp3_header(byte_buffer.get_buffer(), mp3header);
    } else
      return NULL;
  }

  if (pos > 0) {
    handle_garbage(pos);
    byte_buffer.remove(pos);
    pos = 0;
  }

  if (packetno == 0) {
    spf = mp3header->samples_per_channel;
    codec_id = MKV_A_MP3;
    codec_id[codec_id.length() - 1] = (char)(mp3header->layer + '0');
    set_codec_id(codec_id.c_str());
    if (spf != 1152)
      set_track_default_duration((int64_t)(1000000000.0 * spf *
                                           ti->async.linear /
                                           samples_per_sec));
    rerender_track_headers();
  }

  if (mp3header->framesize > byte_buffer.get_size())
    return NULL;

  pins = 1000000000.0 * (double)spf / mp3header->sampling_frequency;

  if (needs_negative_displacement(pins)) {
    /*
     * MP3 audio synchronization. displacement < 0 means skipping an
     * appropriate number of packets at the beginning.
     */
    displace(-pins);
    byte_buffer.remove(mp3header->framesize);

    return NULL;
  }

  buf = (unsigned char *)safememdup(byte_buffer.get_buffer(),
                                    mp3header->framesize);

  if (needs_positive_displacement(pins)) {
    /*
     * MP3 audio synchronization. displacement > 0 is solved by creating
     * silent MP3 packets and repeating it over and over again (well only as
     * often as necessary of course. Wouldn't want to spoil your movie by
     * providing a silent MP3 stream ;)).
     */
    displace(pins);
    memset(buf + 4, 0, mp3header->framesize - 4);

    return buf;
  }

  byte_buffer.remove(mp3header->framesize);

  return buf;
}

void
mp3_packetizer_c::set_headers() {
  if (!codec_id_set) {
    set_codec_id(MKV_A_MP3);
    codec_id_set = true;
  }
  set_audio_sampling_freq((float)samples_per_sec);
  set_audio_channels(channels);

  generic_packetizer_c::set_headers();
}

int
mp3_packetizer_c::process(memory_c &mem,
                          int64_t timecode,
                          int64_t,
                          int64_t,
                          int64_t) {
  unsigned char *packet;
  mp3_header_t mp3header;
  int64_t my_timecode;

  debug_enter("mp3_packetizer_c::process");

  byte_buffer.add(mem.data, mem.size);
  while ((packet = get_mp3_packet(&mp3header)) != NULL) {
    if (timecode == -1)
      my_timecode = (int64_t)(1000000000.0 * packetno * spf / samples_per_sec);
    else
      my_timecode = timecode + ti->async.displacement;
    my_timecode = (int64_t)(my_timecode * ti->async.linear);
    memory_c mem(packet, mp3header.framesize, true);
    add_packet(mem, my_timecode,
               (int64_t)(1000000000.0 * spf * ti->async.linear /
                         samples_per_sec));
    packetno++;
  }

  debug_leave("mp3_packetizer_c::process");

  return FILE_STATUS_MOREDATA;
}

void
mp3_packetizer_c::dump_debug_info() {
  mxdebug("mp3_packetizer_c: queue: %d; buffer_size: %d\n",
          packet_queue.size(), byte_buffer.get_size());
}

connection_result_e
mp3_packetizer_c::can_connect_to(generic_packetizer_c *src) {
  mp3_packetizer_c *msrc;

  msrc = dynamic_cast<mp3_packetizer_c *>(src);
  if (msrc == NULL)
    return CAN_CONNECT_NO_FORMAT;
  if ((samples_per_sec != msrc->samples_per_sec) ||
      (channels != msrc->channels))
    return CAN_CONNECT_NO_PARAMETERS;
  return CAN_CONNECT_YES;
}
