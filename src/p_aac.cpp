/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_aac.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief AAC output module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "pr_generic.h"
#include "aac_common.h"
#include "p_aac.h"
#include "matroska.h"

using namespace libmatroska;

aac_packetizer_c::aac_packetizer_c(generic_reader_c *nreader, int nid,
                                   int nprofile,
                                   unsigned long nsamples_per_sec,
                                   int nchannels, track_info_t *nti,
                                   bool nemphasis_present,
                                   bool nheaderless)
  throw (error_c): generic_packetizer_c(nreader, nti) {
  packetno = 0;
  bytes_output = 0;
  packet_buffer = NULL;
  buffer_size = 0;
  samples_per_sec = nsamples_per_sec;
  channels = nchannels;
  id = nid;
  profile = nprofile;
  headerless = nheaderless;
  emphasis_present = nemphasis_present;

  set_track_type(track_audio);
  set_track_default_duration_ns((int64_t)(1024000000000.0 * ti->async.linear /
                                          samples_per_sec));
  duplicate_data_on_add(headerless);
}

aac_packetizer_c::~aac_packetizer_c() {
  if (packet_buffer != NULL)
    safefree(packet_buffer);
}

void aac_packetizer_c::add_to_buffer(unsigned char *buf, int size) {
  unsigned char *new_buffer;

  new_buffer = (unsigned char *)saferealloc(packet_buffer, buffer_size + size);

  memcpy(new_buffer + buffer_size, buf, size);
  packet_buffer = new_buffer;
  buffer_size += size;
}

int aac_packetizer_c::aac_packet_available() {
  int pos;
  aac_header_t aacheader;

  if (packet_buffer == NULL)
    return 0;
  pos = find_aac_header(packet_buffer, buffer_size, &aacheader,
                        emphasis_present);
  if (pos < 0)
    return 0;

  return 1;
}

void aac_packetizer_c::remove_aac_packet(int pos, int framesize) {
  int new_size;
  unsigned char *temp_buf;

  new_size = buffer_size - (pos + framesize);
  if (new_size != 0)
    temp_buf = (unsigned char *)safememdup(&packet_buffer[pos + framesize],
                                           new_size);
  else
    temp_buf = NULL;
  safefree(packet_buffer);
  packet_buffer = temp_buf;
  buffer_size = new_size;
}

unsigned char *aac_packetizer_c::get_aac_packet(unsigned long *header,
                                                aac_header_t *aacheader) {
  int pos, i, up_shift, down_shift;
  unsigned char *buf, *src;
  double pims;

  if (packet_buffer == NULL)
    return 0;
  pos = find_aac_header(packet_buffer, buffer_size, aacheader,
                        emphasis_present);
  if (pos < 0)
    return 0;
  if ((pos + aacheader->bytes) > buffer_size)
    return 0;

  pims = ((double)aacheader->bytes) * 1000.0 /
         ((double)aacheader->bit_rate / 8.0);

  if (ti->async.displacement < 0) {
    /*
     * AAC audio synchronization. displacement < 0 means skipping an
     * appropriate number of packets at the beginning.
     */
    ti->async.displacement += (int)pims;
    if (ti->async.displacement > -(pims / 2))
      ti->async.displacement = 0;

    remove_aac_packet(pos, aacheader->bytes);

    return 0;
  }

  if (verbose && (pos > 0))
    mxwarn("aac_packetizer: skipping %d bytes (no valid AAC header "
           "found). This might make audio/video go out of sync, but this "
           "stream is damaged.\n", pos);
  if ((aacheader->header_bit_size % 8) == 0)
    buf = (unsigned char *)safememdup(packet_buffer + pos +
                                      aacheader->header_byte_size,
                                      aacheader->data_byte_size);
  else {
    // Header is not byte aligned, i.e. MPEG-4 ADTS
    // This code is from mpeg4ip/server/mp4creator/aac.cpp
    up_shift = aacheader->header_bit_size % 8;
    down_shift = 8 - up_shift;
    src = packet_buffer + pos + aacheader->header_bit_size / 8;

    buf = (unsigned char *)safemalloc(aacheader->data_byte_size);

    buf[0] = src[0] << up_shift;
    for (i = 1; i < aacheader->data_byte_size; i++) {
      buf[i - 1] |= (src[i] >> down_shift);
      buf[i] = (src[i] << up_shift);
    }
  }

  if (ti->async.displacement > 0) {
    /*
     * AAC audio synchronization. displacement > 0 is solved by duplicating
     * the very first AAC packet as often as necessary. I cannot create
     * a packet with total silence because I don't know how, and simply
     * settings the packet's values to 0 does not work as the AAC header
     * contains a CRC of its data.
     */
    ti->async.displacement -= (int)pims;
    if (ti->async.displacement < (pims / 2))
      ti->async.displacement = 0;

    return buf;
  }

  remove_aac_packet(pos, aacheader->bytes);

  return buf;
}

void aac_packetizer_c::set_headers() {
  if (id == AAC_ID_MPEG4) {
    if (profile == AAC_PROFILE_MAIN)
      set_codec_id(MKV_A_AAC_4MAIN);
    else if (profile == AAC_PROFILE_LC)
      set_codec_id(MKV_A_AAC_4LC);
    else if (profile == AAC_PROFILE_SSR)
      set_codec_id(MKV_A_AAC_4SSR);
    else if (profile == AAC_PROFILE_LTP)
      set_codec_id(MKV_A_AAC_4LTP);
    else if (profile == AAC_PROFILE_SBR)
      set_codec_id(MKV_A_AAC_4SBR);
    else
      die("aac_packetizer: Unknown AAC MPEG-4 object type %d.", profile);
  } else {
    if (profile == AAC_PROFILE_MAIN)
      set_codec_id(MKV_A_AAC_2MAIN);
    else if (profile == AAC_PROFILE_LC)
      set_codec_id(MKV_A_AAC_2LC);
    else if (profile == AAC_PROFILE_SSR)
      set_codec_id(MKV_A_AAC_2SSR);
    else if (profile == AAC_PROFILE_SBR)
      set_codec_id(MKV_A_AAC_2SBR);
    else
      die("aac_packetizer: Unknown AAC MPEG-2 profile %d.", profile);
  }
  set_audio_sampling_freq((float)samples_per_sec);
  set_audio_channels(channels);

  generic_packetizer_c::set_headers();
}

int aac_packetizer_c::process(unsigned char *buf, int size,
                              int64_t timecode, int64_t, int64_t, int64_t) {
  unsigned char *packet;
  unsigned long header;
  aac_header_t aacheader;
  int64_t my_timecode;

  debug_enter("aac_packetizer_c::process");

  if (headerless) {
    if (timecode != -1)
      my_timecode = timecode;
    else
      my_timecode = (int64_t)(1000.0 * packetno * 1024 * ti->async.linear /
                              samples_per_sec);
    add_packet(buf, size, my_timecode,
               (int64_t)(1000.0 * 1024 * ti->async.linear / samples_per_sec));

    debug_leave("aac_packetizer_c::process");

    return EMOREDATA;
  }

  if (timecode != -1)
    my_timecode = timecode;

  add_to_buffer(buf, size);
  while ((packet = get_aac_packet(&header, &aacheader)) != NULL) {
    if (timecode == -1)
      my_timecode = (int64_t)(1000.0 * packetno * 1024 * ti->async.linear /
                              samples_per_sec);

    add_packet(packet, aacheader.data_byte_size, my_timecode,
               (int64_t)(1000.0 * 1024 * ti->async.linear / samples_per_sec));
    packetno++;
  }

  debug_leave("aac_packetizer_c::process");

  return EMOREDATA;
}

void aac_packetizer_c::dump_debug_info() {
  mxdebug("aac_packetizer_c: queue: %d; buffer size: %d\n",
          packet_queue.size(), buffer_size);
}
