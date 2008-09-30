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

#include "os.h"

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

#include "checksums.h"
using namespace libmatroska;

flac_packetizer_c::flac_packetizer_c(generic_reader_c *p_reader,
                                     track_info_c &p_ti,
                                     unsigned char *header,
                                     int l_header)
  throw (error_c):
  generic_packetizer_c(p_reader, p_ti)
  , m_num_packets(0)
{
  int result;

  if ((4 > l_header) || memcmp(header, "fLaC", 4)) {
    m_header = memory_c::alloc(l_header + 4);
    memcpy(m_header->get(),     "fLaC", 4);
    memcpy(m_header->get() + 4, header, l_header);

  } else
    m_header = memory_cptr(new memory_c((unsigned char *)safememdup(header, l_header), l_header, true));

  result = flac_decode_headers(m_header->get(), m_header->get_size(), 1, FLAC_HEADER_STREAM_INFO, &m_stream_info);
  if (!(result & FLAC_HEADER_STREAM_INFO))
    mxerror_tid(ti.fname, ti.id, Y("The FLAC headers could not be parsed: the stream info structure was not found.\n"));

  set_track_type(track_audio);
  if (m_stream_info.min_blocksize == m_stream_info.max_blocksize)
    set_track_default_duration((int64_t)(1000000000ll * m_stream_info.min_blocksize / m_stream_info.sample_rate));
}

flac_packetizer_c::~flac_packetizer_c() {
}

void
flac_packetizer_c::set_headers() {
  set_codec_id(MKV_A_FLAC);
  set_codec_private(m_header->get(), m_header->get_size());
  set_audio_sampling_freq((float)m_stream_info.sample_rate);
  set_audio_channels(m_stream_info.channels);
  set_audio_bit_depth(m_stream_info.bits_per_sample);

  generic_packetizer_c::set_headers();
}

int
flac_packetizer_c::process(packet_cptr packet) {
  m_num_packets++;

  packet->duration = flac_get_num_samples(packet->data->get(), packet->data->get_size(), m_stream_info);

  if (-1 == packet->duration) {
    mxwarn_tid(ti.fname, ti.id, boost::format(Y("Packet number %1% contained an invalid FLAC header and is being skipped.\n")) % m_num_packets);
    return FILE_STATUS_MOREDATA;
  }

  packet->duration = packet->duration * 1000000000ll / m_stream_info.sample_rate;
  add_packet(packet);

  return FILE_STATUS_MOREDATA;
}

connection_result_e
flac_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                  string &error_message) {
  flac_packetizer_c *fsrc = dynamic_cast<flac_packetizer_c *>(src);
  if (NULL == fsrc)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_a_samplerate(m_stream_info.sample_rate, fsrc->m_stream_info.sample_rate);
  connect_check_a_channels(m_stream_info.channels, fsrc->m_stream_info.channels);
  connect_check_a_bitdepth(m_stream_info.bits_per_sample, fsrc->m_stream_info.bits_per_sample);

  if (   (m_header->get_size() != fsrc->m_header->get_size())
      || (NULL == m_header.get())
      || (NULL == fsrc->m_header.get())
      || memcmp(m_header->get(), fsrc->m_header->get(), m_header->get_size())) {
    error_message = (boost::format(Y("The FLAC header data is different for the two tracks (lengths: %1% and %2%)")) % m_header->get_size() % fsrc->m_header->get_size()).str();
    return CAN_CONNECT_MAYBE_CODECPRIVATE;
  }

  return CAN_CONNECT_YES;
}
#endif
