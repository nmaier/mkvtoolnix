/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_dts.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: p_dts.cpp,v 1.1 2003/05/15 08:58:52 mosu Exp $
    \brief DTS output module
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "pr_generic.h"
#include "dts_common.h"
#include "p_dts.h"
#include "matroska.h"

using namespace LIBMATROSKA_NAMESPACE;

dts_packetizer_c::dts_packetizer_c(generic_reader_c *nreader,
                                   unsigned long nsamples_per_sec,
                                   track_info_t *nti)
  throw (error_c): generic_packetizer_c(nreader, nti) {
  packetno = 0;
  bytes_output = 0;
  packet_buffer = NULL;
  buffer_size = 0;
  samples_per_sec = nsamples_per_sec;
  
  set_track_type(track_audio);
  duplicate_data_on_add(false);
}

dts_packetizer_c::~dts_packetizer_c() {
  safefree(packet_buffer);
}

void dts_packetizer_c::add_to_buffer(unsigned char *buf, int size) {
  unsigned char *new_buffer;
  
  new_buffer = (unsigned char *)saferealloc(packet_buffer, buffer_size + size);
  
  memcpy(new_buffer + buffer_size, buf, size);
  packet_buffer = new_buffer;
  buffer_size += size;
}

int dts_packetizer_c::dts_packet_available() {
  int pos;
  dts_header_t  dtsheader;
  
  if (packet_buffer == NULL)
    return 0;
  pos = find_dts_header(packet_buffer, buffer_size, &dtsheader);
  if (pos < 0)
    return 0;
  
  return 1;
}

void dts_packetizer_c::remove_dts_packet(int pos, int framesize) {
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

unsigned char *dts_packetizer_c::get_dts_packet(unsigned long *header,
                                                dts_header_t *dtsheader) {
  int pos;
  unsigned char *buf;
  double pims;
  
  if (packet_buffer == NULL)
    return 0;
  pos = find_dts_header(packet_buffer, buffer_size, dtsheader);
  if (pos < 0)
    return 0;
  if ((pos + dtsheader->bytes) > buffer_size)
    return 0;

  pims = ((double)((dtsheader->bytes + 511) & (~511))) * 1000.0 /
    ((double)dtsheader->bit_rate / 8.0);

  if (ti->async.displacement < 0) {
    /*
     * DTS audio synchronization. displacement < 0 means skipping an
     * appropriate number of packets at the beginning.
     */
    ti->async.displacement += (int)pims;
    if (ti->async.displacement > -(pims / 2))
      ti->async.displacement = 0;
    
    remove_dts_packet(pos, dtsheader->bytes);
    
    return 0;
  }

  if (verbose && (pos > 1))
    fprintf(stdout, "dts_packetizer: skipping %d bytes (no valid DTS header "
            "found). This might make audio/video go out of sync, but this "
            "stream is damaged.\n", pos);
  buf = (unsigned char *)safememdup(packet_buffer + pos, dtsheader->bytes);
  
  if (ti->async.displacement > 0) {
    /*
     * DTS audio synchronization. displacement > 0 is solved by duplicating
     * the very first DTS packet as often as necessary. I cannot create
     * a packet with total silence because I don't know how, and simply
     * settings the packet's values to 0 does not work as the DTS header
     * contains a CRC of its data.
     */
    ti->async.displacement -= (int)pims;
    if (ti->async.displacement < (pims / 2))
      ti->async.displacement = 0;
    
    return buf;
  }

  remove_dts_packet(pos, dtsheader->bytes);
  
  return buf;
}

void dts_packetizer_c::set_headers() {
  set_codec_id(MKV_A_DTS);
  set_audio_sampling_freq((float)samples_per_sec);
  set_audio_channels(6);

  generic_packetizer_c::set_headers();
}

int dts_packetizer_c::process(unsigned char *buf, int size,
                              int64_t timecode, int64_t, int64_t, int64_t) {
  unsigned char *packet;
  unsigned long header;
  dts_header_t dtsheader;
  int64_t my_timecode;
  double pims;
  
  if (timecode != -1)
    my_timecode = timecode;

  add_to_buffer(buf, size);
  while ((packet = get_dts_packet(&header, &dtsheader)) != NULL) {

    if (timecode == -1) {
      pims = ((double)((dtsheader.bytes + 511) & (~511))) * 1000.0 /
        ((double)dtsheader.bit_rate / 8.0);
      my_timecode = (int64_t)(pims * (double)packetno);
    }
	 
    add_packet(packet, dtsheader.bytes, my_timecode, (int64_t)pims);
    packetno++;
  }

  return EMOREDATA;
}

