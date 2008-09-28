/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   AC3 output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "ac3_common.h"
#include "matroska.h"
#include "output_control.h"
#include "pr_generic.h"
#include "p_ac3.h"

using namespace libmatroska;

ac3_packetizer_c::ac3_packetizer_c(generic_reader_c *_reader,
                                   int _samples_per_sec,
                                   int _channels,
                                   int _bsid,
                                   track_info_c &_ti)
  throw (error_c):
  generic_packetizer_c(_reader, _ti),
  bytes_output(0), packetno(0), bytes_skipped(0),
  samples_per_sec(_samples_per_sec),
  first_packet(true) {

  memset(&first_ac3_header, 0, sizeof(ac3_header_t));
  first_ac3_header.bsid     = _bsid;
  first_ac3_header.channels = _channels;

  set_track_type(track_audio);
  set_track_default_duration((int64_t)(1536000000000.0 / samples_per_sec));
  enable_avi_audio_sync(true);
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

  packet_buffer = byte_buffer.get_buffer();
  size = byte_buffer.get_size();

  if (packet_buffer == NULL)
    return NULL;

  pos = find_ac3_header(packet_buffer, size, ac3header, first_packet || first_ac3_header.has_dependent_frames);

  if ((0 > pos) || ((pos + ac3header->bytes) > size))
    return NULL;

  bytes_skipped += pos;
  if (0 < bytes_skipped) {
    bool warning_printed;

    warning_printed = false;
    if (packetno == 0) {
      int64_t offset;

      offset = handle_avi_audio_sync(bytes_skipped, false);
      if (offset != -1) {
        mxinfo_tid(ti.fname, ti.id,
                   boost::format(Y("This AC3 track contains %1% bytes of non-AC3 data at the beginning. "
                                   "This corresponds to a delay of %2%ms. "
                                   "This delay will be used instead of the non-AC3 data.\n"))
                   % bytes_skipped % (offset / 1000000));
        warning_printed = true;
        ti.tcsync.displacement += offset;
      }
    }
    if (!warning_printed)
      mxwarn_tid(ti.fname, ti.id,
                 boost::format(Y("This AC3 track contains %1% bytes of non-AC3 data which were skipped. "
                                 "The audio/video synchronization may have been lost.\n")) % bytes_skipped);
    byte_buffer.remove(pos);
    packet_buffer = byte_buffer.get_buffer();
    size = byte_buffer.get_size();
    pos = 0;
    bytes_skipped = 0;
  }

  buf = (unsigned char *)safememdup(packet_buffer, ac3header->bytes);

  byte_buffer.remove(ac3header->bytes);

  return buf;
}

void
ac3_packetizer_c::set_headers() {
  string id = MKV_A_AC3;

  if (9 == first_ac3_header.bsid)
    id += "/BSID9";
  else if (10 == first_ac3_header.bsid)
    id += "/BSID10";
  else if (16 == first_ac3_header.bsid)
    id = MKV_A_EAC3;

  set_codec_id(id.c_str());
  set_audio_sampling_freq((float)samples_per_sec);
  set_audio_channels(first_ac3_header.channels);

  generic_packetizer_c::set_headers();
}

int
ac3_packetizer_c::process(packet_cptr packet) {
  unsigned char *ac3_packet;
  unsigned long header;
  ac3_header_t ac3header;

  add_to_buffer(packet->data->get(), packet->data->get_size());
  while ((ac3_packet = get_ac3_packet(&header, &ac3header)) != NULL) {
    adjust_header_values(ac3header);

    int64_t new_timecode = -1 == packet->timecode ? (int64_t)(1000000000.0 * packetno * ac3header.samples / samples_per_sec) : packet->timecode;

    add_packet(new packet_t(new memory_c(ac3_packet, ac3header.bytes, true), new_timecode, (int64_t)(1000000000.0 * ac3header.samples / samples_per_sec)));
    packetno++;
  }

  return FILE_STATUS_MOREDATA;
}

void
ac3_packetizer_c::adjust_header_values(ac3_header_t &ac3_header) {
  if (!first_packet)
    return;

  first_packet = false;

  memcpy(&first_ac3_header, &ac3_header, sizeof(ac3_header_t));

  if (16 == first_ac3_header.bsid)
    set_codec_id(MKV_A_EAC3);

  if (1536 != first_ac3_header.samples)
    set_track_default_duration((int64_t)(1000000000.0 * first_ac3_header.samples / samples_per_sec));

  rerender_track_headers();
}

connection_result_e
ac3_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                 string &error_message) {
  ac3_packetizer_c *asrc;

  asrc = dynamic_cast<ac3_packetizer_c *>(src);
  if (asrc == NULL)
    return CAN_CONNECT_NO_FORMAT;
  connect_check_a_samplerate(samples_per_sec, asrc->samples_per_sec);
  connect_check_a_channels(first_ac3_header.channels, asrc->first_ac3_header.channels);
  return CAN_CONNECT_YES;
}

ac3_bs_packetizer_c::ac3_bs_packetizer_c(generic_reader_c *nreader,
                                         unsigned long nsamples_per_sec,
                                         int nchannels,
                                         int nbsid,
                                         track_info_c &_ti)
  throw (error_c):
  ac3_packetizer_c(nreader, nsamples_per_sec, nchannels, nbsid, _ti) {
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
    mxwarn(Y("ac3_bs_packetizer::add_to_buffer(): Untested code ('size' is odd). "
             "If mkvmerge crashes or if the resulting file does not contain the complete and correct audio track, "
             "then please contact the author Moritz Bunkus at moritz@bunkus.org.\n"));
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
