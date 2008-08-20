/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   RealAudio output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "matroska.h"
#include "pr_generic.h"
#include "p_realaudio.h"

using namespace libmatroska;

ra_packetizer_c::ra_packetizer_c(generic_reader_c *_reader,
                                 int _samples_per_sec,
                                 int _channels,
                                 int _bits_per_sample,
                                 uint32_t _fourcc,
                                 unsigned char *_private_data,
                                 int _private_size,
                                 track_info_c &_ti)
  throw (error_c):
  generic_packetizer_c(_reader, _ti),
  bytes_output(0), packetno(0),
  samples_per_sec(_samples_per_sec), channels(_channels),
  bits_per_sample(_bits_per_sample), fourcc(_fourcc),
  private_data(new memory_c((unsigned char *)
                            safememdup(_private_data, _private_size),
                            _private_size, true)),
  skip_to_keyframe(false),
  buffer_until_keyframe(false) {

  set_track_type(track_audio, TFA_SHORT_QUEUEING);
}

ra_packetizer_c::~ra_packetizer_c() {
}

void
ra_packetizer_c::set_headers() {
  char codec_id[20];
  int i;

  sprintf(codec_id, "A_REAL/%c%c%c%c", (char)(fourcc >> 24),
          (char)((fourcc >> 16) & 0xff), (char)((fourcc >> 8) & 0xff),
          (char)(fourcc & 0xff));
  for (i = 0; i < strlen(codec_id); i++)
    codec_id[i] = toupper(codec_id[i]);
  set_codec_id(codec_id);
  set_audio_sampling_freq((float)samples_per_sec);
  set_audio_channels(channels);
  set_audio_bit_depth(bits_per_sample);
  set_codec_private(private_data->get(), private_data->get_size());

  generic_packetizer_c::set_headers();
  track_entry->EnableLacing(false);
}

int
ra_packetizer_c::process(packet_cptr packet) {
  debug_enter("ra_packetizer_c::process");

  add_packet(packet);

  debug_leave("ra_packetizer_c::process");

  return FILE_STATUS_MOREDATA;
}

void
ra_packetizer_c::dump_debug_info() {
}

connection_result_e
ra_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                string &error_message) {
  ra_packetizer_c *psrc;

  psrc = dynamic_cast<ra_packetizer_c *>(src);
  if (psrc == NULL)
    return CAN_CONNECT_NO_FORMAT;
  connect_check_a_samplerate(samples_per_sec, psrc->samples_per_sec);
  connect_check_a_channels(channels, psrc->channels);
  connect_check_a_bitdepth(bits_per_sample, psrc->bits_per_sample);
  return CAN_CONNECT_YES;
}
