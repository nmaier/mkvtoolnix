/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_ac3.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: p_ac3.cpp,v 1.15 2003/05/02 20:11:34 mosu Exp $
    \brief AC3 output module
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "pr_generic.h"
#include "ac3_common.h"
#include "p_ac3.h"
#include "matroska.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

using namespace LIBMATROSKA_NAMESPACE;

ac3_packetizer_c::ac3_packetizer_c(unsigned long nsamples_per_sec,
                                   int nchannels, track_info_t *nti)
  throw (error_c): generic_packetizer_c(nti) {
  packetno = 0;
  bytes_output = 0;
  packet_buffer = NULL;
  buffer_size = 0;
  samples_per_sec = nsamples_per_sec;
  channels = nchannels;
  set_header();
}

ac3_packetizer_c::~ac3_packetizer_c() {
  if (packet_buffer != NULL)
    free(packet_buffer);
}

void ac3_packetizer_c::add_to_buffer(unsigned char *buf, int size) {
  unsigned char *new_buffer;
  
  new_buffer = (unsigned char *)realloc(packet_buffer, buffer_size + size);
  if (new_buffer == NULL)
    die("realloc");
  
  memcpy(new_buffer + buffer_size, buf, size);
  packet_buffer = new_buffer;
  buffer_size += size;
}

int ac3_packetizer_c::ac3_packet_available() {
  int pos;
  ac3_header_t  ac3header;
  
  if (packet_buffer == NULL)
    return 0;
  pos = find_ac3_header(packet_buffer, buffer_size, &ac3header);
  if (pos < 0)
    return 0;
  
  return 1;
}

void ac3_packetizer_c::remove_ac3_packet(int pos, int framesize) {
  int new_size;
  unsigned char *temp_buf;
  
  new_size = buffer_size - (pos + framesize);
  if (new_size != 0) {
    temp_buf = (unsigned char *)malloc(new_size);
    if (temp_buf == NULL)
      die("malloc");
    memcpy(temp_buf, &packet_buffer[pos + framesize],
           new_size);
  } else
    temp_buf = NULL;
  free(packet_buffer);
  packet_buffer = temp_buf;
  buffer_size = new_size;
}

unsigned char *ac3_packetizer_c::get_ac3_packet(unsigned long *header,
                                                ac3_header_t *ac3header) {
  int pos;
  unsigned char *buf;
  double pims;
  
  if (packet_buffer == NULL)
    return 0;
  pos = find_ac3_header(packet_buffer, buffer_size, ac3header);
  if (pos < 0)
    return 0;
  if ((pos + ac3header->bytes) > buffer_size)
    return 0;

  pims = ((double)ac3header->bytes) * 1000.0 /
         ((double)ac3header->bit_rate / 8.0);

  if (ti->async.displacement < 0) {
    /*
     * AC3 audio synchronization. displacement < 0 means skipping an
     * appropriate number of packets at the beginning.
     */
    ti->async.displacement += (int)pims;
    if (ti->async.displacement > -(pims / 2))
      ti->async.displacement = 0;
    
    remove_ac3_packet(pos, ac3header->bytes);
    
    return 0;
  }

  if (verbose && (pos > 1))
    fprintf(stdout, "ac3_packetizer: skipping %d bytes (no valid AC3 header "
            "found). This might make audio/video go out of sync, but this "
            "stream is damaged.\n", pos);
  buf = (unsigned char *)malloc(ac3header->bytes);
  if (buf == NULL)
    die("malloc");
  memcpy(buf, packet_buffer + pos, ac3header->bytes);
  
  if (ti->async.displacement > 0) {
    /*
     * AC3 audio synchronization. displacement > 0 is solved by duplicating
     * the very first AC3 packet as often as necessary. I cannot create
     * a packet with total silence because I don't know how, and simply
     * settings the packet's values to 0 does not work as the AC3 header
     * contains a CRC of its data.
     */
    ti->async.displacement -= (int)pims;
    if (ti->async.displacement < (pims / 2))
      ti->async.displacement = 0;
    
    return buf;
  }

  remove_ac3_packet(pos, ac3header->bytes);
  
  return buf;
}

void ac3_packetizer_c::set_header() {
  set_serial(-1);
  set_track_type(track_audio);
  set_codec_id(MKV_A_AC3);
  set_audio_sampling_freq((float)samples_per_sec);
  set_audio_channels(channels);

  if (ti->default_track)
    set_as_default_track('a');

  generic_packetizer_c::set_header();
}

int ac3_packetizer_c::process(unsigned char *buf, int size,
                              int64_t timecode, int64_t, int64_t, int64_t) {
  unsigned char *packet;
  unsigned long header;
  ac3_header_t ac3header;
  int64_t my_timecode;

  if (timecode != -1)
    my_timecode = timecode;

  add_to_buffer(buf, size);
  while ((packet = get_ac3_packet(&header, &ac3header)) != NULL) {
    if (timecode != -1)
      my_timecode = (int64_t)(1000.0 * packetno * 1536 * ti->async.linear / 
                              samples_per_sec);

    add_packet(packet, ac3header.bytes, my_timecode);
    packetno++;
    free(packet);
  }

  return EMOREDATA;
}
