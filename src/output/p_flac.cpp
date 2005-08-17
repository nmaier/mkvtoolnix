/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   FLAC packetizer

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "config.h"

#if defined(HAVE_FLAC_FORMAT_H)
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include "common.h"
#include "flac_common.h"
#include "pr_generic.h"
#include "p_flac.h"
#include "matroska.h"

using namespace libmatroska;

flac_packetizer_c::flac_packetizer_c(generic_reader_c *_reader,
                                     unsigned char *_header,
                                     int _l_header,
                                     track_info_c &_ti)
  throw (error_c): generic_packetizer_c(_reader, _ti),
  num_packets(0) {
  int result;

  if ((_l_header < 4) || (_header[0] != 'f') || (_header [1] != 'L') ||
      (_header[2] != 'a') || (_header[3] != 'C')) {
    header.set(safemalloc(_l_header + 4));
    memcpy(header.get(), "fLaC", 4);
    memcpy(header.get() + 4, _header, _l_header);
    l_header = _l_header + 4;
  } else {
    header.set(safememdup(_header, _l_header));
    l_header = _l_header;
  }

  result = flac_decode_headers(header, l_header, 1,
                               FLAC_HEADER_STREAM_INFO, &stream_info);
  if (!(result & FLAC_HEADER_STREAM_INFO))
    mxerror(_(FMT_TID "The FLAC headers could not be parsed: the stream info "
              "structure was not found.\n"),
            ti.fname.c_str(), (int64_t)ti.id);

  set_track_type(track_audio);
  if (stream_info.min_blocksize == stream_info.max_blocksize)
    set_track_default_duration((int64_t)(1000000000ll *
                                         stream_info.min_blocksize *
                                         ti.async.linear /
                                         stream_info.sample_rate));

  if ((ti.async.displacement != 0) || (ti.async.linear != 1.0))
    mxwarn("FLAC packetizer: Audio synchronization has not been "
           "implemented for FLAC yet.\n");
}

flac_packetizer_c::~flac_packetizer_c() {
}

void
flac_packetizer_c::set_headers() {
  set_codec_id(MKV_A_FLAC);
  set_codec_private(header, l_header);
  set_audio_sampling_freq((float)stream_info.sample_rate);
  set_audio_channels(stream_info.channels);
  set_audio_bit_depth(stream_info.bits_per_sample);

  generic_packetizer_c::set_headers();
}

int
flac_packetizer_c::process(packet_cptr packet) {
  debug_enter("flac_packetizer_c::process");

  packet->duration = flac_get_num_samples(packet->memory->data,
                                          packet->memory->size, stream_info);
  if (packet->duration == -1) {
    mxwarn(_(FMT_TID "Packet number %lld contained an invalid FLAC header "
             "and is being skipped.\n"), ti.fname.c_str(), (int64_t)ti.id,
           num_packets + 1);
    debug_leave("flac_packetizer_c::process");
    return FILE_STATUS_MOREDATA;
  }
  packet->duration = packet->duration * 1000000000ll / stream_info.sample_rate;
  add_packet(packet);
  num_packets++;
  debug_leave("flac_packetizer_c::process");

  return FILE_STATUS_MOREDATA;
}

void
flac_packetizer_c::dump_debug_info() {
  mxdebug("flac_packetizer_c: queue: %u\n", (unsigned int)packet_queue.size());
}

connection_result_e
flac_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                  string &error_message) {
  flac_packetizer_c *fsrc;

  fsrc = dynamic_cast<flac_packetizer_c *>(src);
  if (fsrc == NULL)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_a_samplerate(stream_info.sample_rate,
                             fsrc->stream_info.sample_rate);
  connect_check_a_channels(stream_info.channels,
                           fsrc->stream_info.channels);
  connect_check_a_bitdepth(stream_info.bits_per_sample,
                           fsrc->stream_info.bits_per_sample);

  if ((l_header != fsrc->l_header) ||
      (NULL == header.get()) || (NULL == fsrc->header.get()) ||
      memcmp(header.get(), fsrc->header.get(), l_header)) {
    error_message = mxsprintf("The FLAC header data is different for the "
                              "two tracks (lengths: %d and %d)",
                              l_header, fsrc->l_header);
    return CAN_CONNECT_NO_PARAMETERS;
  }
  return CAN_CONNECT_YES;
}
#endif
