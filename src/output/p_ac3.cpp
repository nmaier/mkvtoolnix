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
    \version $Id$
    \brief AC3 output module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "pr_generic.h"
#include "ac3_common.h"
#include "p_ac3.h"
#include "matroska.h"
#include "mkvmerge.h"

using namespace libmatroska;

ac3_packetizer_c::ac3_packetizer_c(generic_reader_c *nreader,
                                   unsigned long nsamples_per_sec,
                                   int nchannels,
                                   int nbsid,
                                   track_info_c *nti)
  throw (error_c):
  generic_packetizer_c(nreader, nti) {
  packetno = 0;
  bytes_output = 0;
  samples_per_sec = nsamples_per_sec;
  channels = nchannels;
  bsid = nbsid;

  set_track_type(track_audio);
  if (use_durations)
    set_track_default_duration((int64_t)(1536000000000.0 *ti->async.linear /
                                            samples_per_sec));
}

ac3_packetizer_c::~ac3_packetizer_c() {
}

void
ac3_packetizer_c::add_to_buffer(unsigned char *buf,
                                int size) {
  byte_buffer.add(buf, size);
}

unsigned char *
ac3_packetizer_c::get_ac3_packet(unsigned long *header,
                                 ac3_header_t *ac3header) {
  int pos, size;
  unsigned char *buf, *packet_buffer;
  double pins;

  packet_buffer = byte_buffer.get_buffer();
  size = byte_buffer.get_size();

  if (packet_buffer == NULL)
    return NULL;
  pos = find_ac3_header(packet_buffer, size, ac3header);
  if (pos < 0)
    return NULL;
  if ((pos + ac3header->bytes) > size)
    return NULL;

  pins = 1536000000000.0 / samples_per_sec;

  if (needs_negative_displacement(pins)) {
    /*
     * AC3 audio synchronization. displacement < 0 means skipping an
     * appropriate number of packets at the beginning.
     */
    displace(-pins);
    byte_buffer.remove(pos + ac3header->bytes);

    return NULL;
  }

  if (verbose && (pos > 1))
    mxwarn("ac3_packetizer: skipping %d bytes (no valid AC3 header "
           "found). This might make audio/video go out of sync, but this "
           "stream is damaged.\n", pos);
  buf = (unsigned char *)safememdup(packet_buffer + pos, ac3header->bytes);

  if (needs_positive_displacement(pins)) {
    /*
     * AC3 audio synchronization. displacement > 0 is solved by duplicating
     * the very first AC3 packet as often as necessary. I cannot create
     * a packet with total silence because I don't know how, and simply
     * settings the packet's values to 0 does not work as the AC3 header
     * contains a CRC of its data.
     */
    displace(pins);
    return buf;
  }

  byte_buffer.remove(pos + ac3header->bytes);

  return buf;
}

void
ac3_packetizer_c::set_headers() {
  string id = MKV_A_AC3;

  if (bsid == 9)
    id += "/BSID9";
  else if (bsid == 10)
    id += "/BSID10";
  set_codec_id(id.c_str());
  set_audio_sampling_freq((float)samples_per_sec);
  set_audio_channels(channels);

  generic_packetizer_c::set_headers();
}

int
ac3_packetizer_c::process(memory_c &mem,
                          int64_t timecode,
                          int64_t,
                          int64_t,
                          int64_t) {
  unsigned char *packet;
  unsigned long header;
  ac3_header_t ac3header;
  int64_t my_timecode;

  debug_enter("ac3_packetizer_c::process");

  add_to_buffer(mem.data, mem.size);
  while ((packet = get_ac3_packet(&header, &ac3header)) != NULL) {
    if (timecode == -1)
      my_timecode = (int64_t)(1000000000.0 * packetno * 1536 /
                              samples_per_sec);
    else
      my_timecode = timecode + ti->async.displacement;
    my_timecode = (int64_t)(my_timecode * ti->async.linear);
    memory_c mem(packet, ac3header.bytes, true);
    add_packet(mem, my_timecode,
               (int64_t)(1000000000.0 * 1536 * ti->async.linear /
                         samples_per_sec));
    packetno++;
  }

  debug_leave("ac3_packetizer_c::process");

  return EMOREDATA;
}

void
ac3_packetizer_c::dump_debug_info() {
  mxdebug("ac3_packetizer_c: queue: %d; buffer size: %d\n",
          packet_queue.size(), byte_buffer.get_size());
}

ac3_bs_packetizer_c::ac3_bs_packetizer_c(generic_reader_c *nreader,
                                         unsigned long nsamples_per_sec,
                                         int nchannels,
                                         int nbsid,
                                         track_info_c *nti)
  throw (error_c):
  ac3_packetizer_c(nreader, nsamples_per_sec, nchannels, nbsid, nti) {
  bsb_present = false;
}

static bool warning_printed = false;

void
ac3_bs_packetizer_c::add_to_buffer(unsigned char *buf,
                                   int size) {
  unsigned char *new_buffer, *dptr, *sendptr, *sptr;
  int size_add;
  bool new_bsb_present;

  if (((size % 2) == 1) && !warning_printed) {
    mxwarn("Untested code. If mkvmerge crashes, "
           "or if the resulting file does not contain the complete and "
           "correct audio track, then please contact the author, Moritz "
           "Bunkus, at moritz@bunkus.org.\n");
    warning_printed = true;
  }

  if (bsb_present) {
    size_add = 1;
    sendptr = buf + size + 1;
  } else {
    size_add = 0;
    sendptr = buf + size;
  }
  size_add += size;
  if ((size_add % 2) == 1) {
    size_add--;
    sendptr--;
    new_bsb_present = true;
  } else
    new_bsb_present = false;

  new_buffer = (unsigned char *)safemalloc(size_add);
  dptr = new_buffer;
  sptr = buf;

  if (bsb_present) {
    dptr[1] = bsb;
    dptr[0] = sptr[0];
    sptr++;
    dptr += 2;
  }

  while (sptr < sendptr) {
    dptr[0] = sptr[1];
    dptr[1] = sptr[0];
    dptr += 2;
    sptr += 2;
  }

  if (new_bsb_present)
    bsb = *sptr;
  bsb_present = new_bsb_present;
  byte_buffer.add(new_buffer, size_add);
  safefree(new_buffer);
}
