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
    \version \$Id: p_aac.cpp,v 1.3 2003/05/18 20:57:07 mosu Exp $
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

using namespace LIBMATROSKA_NAMESPACE;

aac_packetizer_c::aac_packetizer_c(generic_reader_c *nreader, int nid,
                                   unsigned long nsamples_per_sec,
                                   int nchannels, int adif, track_info_t *nti)
  throw (error_c): generic_packetizer_c(nreader, nti) {
  packetno = 0;
  bytes_output = 0;
  packet_buffer = NULL;
  buffer_size = 0;
  samples_per_sec = nsamples_per_sec;
  channels = nchannels;
  id = nid;
  is_adif = adif;

  set_track_type(track_audio);
  duplicate_data_on_add(false);
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
  aac_header_t  aacheader;
  
  if (packet_buffer == NULL)
    return 0;
  pos = find_aac_header(packet_buffer, buffer_size, &aacheader);
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
  int pos;
  unsigned char *buf;
  double pims;
  
  if (packet_buffer == NULL)
    return 0;
  pos = find_aac_header(packet_buffer, buffer_size, aacheader);
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
    fprintf(stdout, "aac_packetizer: skipping %d bytes (no valid AAC header "
            "found). This might make audio/video go out of sync, but this "
            "stream is damaged.\n", pos);
  buf = (unsigned char *)safememdup(packet_buffer + pos, aacheader->bytes);
  
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
  if (id == 0)
    set_codec_id(MKV_A_AAC_4LC);
  else
    set_codec_id(MKV_A_AAC_2LC);
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

  if (timecode != -1)
    my_timecode = timecode;

  add_to_buffer(buf, size);
  while ((packet = get_aac_packet(&header, &aacheader)) != NULL) {
    if (timecode == -1)
      my_timecode = (int64_t)(1000.0 * packetno * 1024 * ti->async.linear / 
                              samples_per_sec);

    add_packet(packet, aacheader.bytes, my_timecode,
               (int64_t)(1000.0 * 1024 * ti->async.linear / samples_per_sec));
    packetno++;
  }

  return EMOREDATA;
}
