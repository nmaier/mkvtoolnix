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
 * DTS output module
 *
 * Written by Peter Niemayer <niemayer@isg.de>.
 * Modified by Moritz Bunkus <moritz@bunkus.org>.
 */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "pr_generic.h"
#include "dts_common.h"
#include "matroska.h"
#include "output_control.h"
#include "p_dts.h"

using namespace libmatroska;

dts_packetizer_c::dts_packetizer_c(generic_reader_c *nreader,
                                   const dts_header_t &dtsheader,
                                   track_info_c *nti,
                                   bool _get_first_header_later)
  throw (error_c):
  generic_packetizer_c(nreader, nti) {
  //packetno = 0;
  samples_written = 0;
  bytes_written = 0;

  packet_buffer = NULL;
  buffer_size = 0;
  skipping_is_normal = false;

  first_header = dtsheader;
  last_header = dtsheader;
  get_first_header_later = _get_first_header_later;

  set_track_type(track_audio);
}

dts_packetizer_c::~dts_packetizer_c() {
  safefree(packet_buffer);
}

void
dts_packetizer_c::add_to_buffer(unsigned char *buf,
                                int size) {
  unsigned char *new_buffer;

  new_buffer = (unsigned char *)saferealloc(packet_buffer, buffer_size + size);

  memcpy(new_buffer + buffer_size, buf, size);
  packet_buffer = new_buffer;
  buffer_size += size;
}

int
dts_packetizer_c::dts_packet_available() {
  int pos;
  dts_header_t dtsheader;

  if (packet_buffer == NULL)
    return 0;

  pos = find_dts_header(packet_buffer, buffer_size, &dtsheader);
  if (pos < 0)
    return 0;

  return 1;
}

void
dts_packetizer_c::remove_dts_packet(int pos,
                                    int framesize) {
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

unsigned char *
dts_packetizer_c::get_dts_packet(dts_header_t &dtsheader) {
  int pos;
  unsigned char *buf;
  double pins;

  if (packet_buffer == NULL)
    return 0;
  pos = find_dts_header(packet_buffer, buffer_size, &dtsheader);
  if (pos < 0)
    return 0;
  if ((pos + dtsheader.frame_byte_size) > buffer_size)
    return 0;

  if (get_first_header_later) {
    first_header = dtsheader;
    last_header = dtsheader;
    get_first_header_later = false;
    set_headers();
    rerender_track_headers();
  }

  if (dtsheader != last_header) {
    mxinfo("DTS header information changed! - New format:\n");
    print_dts_header(&dtsheader);
    last_header = dtsheader;
  }


  pins = get_dts_packet_length_in_nanoseconds(&dtsheader);

  if (needs_negative_displacement(pins)) {
    /*
     * DTS audio synchronization. displacement < 0 means skipping an
     * appropriate number of packets at the beginning.
     */
    displace(-pins);
    remove_dts_packet(pos, dtsheader.frame_byte_size);

    return 0;
  }

  if (verbose && (pos > 0) && !skipping_is_normal)
    mxwarn("dts_packetizer: skipping %d bytes (no valid DTS header "
           "found). This might make audio/video go out of sync, but this "
           "stream is damaged.\n", pos);

  buf = (unsigned char *)safememdup(packet_buffer + pos,
                                    dtsheader.frame_byte_size);

  if (needs_positive_displacement(pins)) {
    /*
     * DTS audio synchronization. displacement > 0 is solved by duplicating
     * the very first DTS packet as often as necessary. I cannot create
     * a packet with total silence because I don't know how, and simply
     * settings the packet's values to 0 does not work as the DTS header
     * contains a CRC of its data.
     */
    displace(pins);
    return buf;
  }

  remove_dts_packet(pos, dtsheader.frame_byte_size);

  return buf;
}

void
dts_packetizer_c::set_headers() {
  set_codec_id(MKV_A_DTS);
  set_audio_sampling_freq((float)first_header.core_sampling_frequency);
  if ((first_header.lfe_type == dts_header_t::lfe_64) ||
      (first_header.lfe_type == dts_header_t::lfe_128))
    set_audio_channels(first_header.audio_channels + 1);
  else
    set_audio_channels(first_header.audio_channels);

  generic_packetizer_c::set_headers();
}

int
dts_packetizer_c::process(memory_c &mem,
                          int64_t timecode,
                          int64_t,
                          int64_t,
                          int64_t) {

  int64_t my_timecode;

  debug_enter("dts_packetizer_c::process");

  if (timecode != -1)
    my_timecode = timecode;

  add_to_buffer(mem.data, mem.size);

  dts_header_t dtsheader;
  unsigned char *packet;
  while ((packet = get_dts_packet(dtsheader)) != NULL) {
    int64_t packet_len_in_ns =
      (int64_t)get_dts_packet_length_in_nanoseconds(&dtsheader);

    if (timecode == -1)
      my_timecode = (int64_t)(((double)samples_written * 1000000000.0) /
                              ((double)dtsheader.core_sampling_frequency));
    else
      my_timecode = timecode + ti->async.displacement;
    my_timecode = (int64_t)(my_timecode * ti->async.linear);
    memory_c mem(packet, dtsheader.frame_byte_size, true);
    add_packet(mem, my_timecode, (int64_t)(packet_len_in_ns *
                                           ti->async.linear));

    bytes_written += dtsheader.frame_byte_size;
    samples_written += get_dts_packet_length_in_core_samples(&dtsheader);
  }

  debug_leave("dts_packetizer_c::process");

  return EMOREDATA;
}

void
dts_packetizer_c::dump_debug_info() {
  mxdebug("dts_packetizer_c: queue: %d\n", packet_queue.size());
}

int
dts_packetizer_c::can_connect_to(generic_packetizer_c *src) {
  dts_packetizer_c *dsrc;

  dsrc = dynamic_cast<dts_packetizer_c *>(src);
  if (dsrc == NULL)
    return CAN_CONNECT_NO_FORMAT;
  // Hmm...
  return CAN_CONNECT_YES;
}
