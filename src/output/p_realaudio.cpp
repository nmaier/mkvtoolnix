/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   RealAudio output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/matroska.h"
#include "merge/pr_generic.h"
#include "output/p_realaudio.h"

using namespace libmatroska;

ra_packetizer_c::ra_packetizer_c(generic_reader_c *p_reader,
                                 track_info_c &p_ti,
                                 int samples_per_sec,
                                 int channels,
                                 int bits_per_sample,
                                 uint32_t fourcc,
                                 unsigned char *private_data,
                                 int private_size)
  throw (error_c)
  : generic_packetizer_c(p_reader, p_ti)
  , m_samples_per_sec(samples_per_sec)
  , m_channels(channels)
  , m_bits_per_sample(bits_per_sample)
  , m_fourcc(fourcc)
  , m_private_data(new memory_c((unsigned char *)safememdup(private_data, private_size), private_size, true))
{
  set_track_type(track_audio, TFA_SHORT_QUEUEING);
}

ra_packetizer_c::~ra_packetizer_c() {
}

void
ra_packetizer_c::set_headers() {
  std::string codec_id = (boost::format("A_REAL/%1%%2%%3%%4%")
                          % (char)(m_fourcc >> 24) % (char)((m_fourcc >> 16) & 0xff) % (char)((m_fourcc >> 8) & 0xff) % (char)(m_fourcc & 0xff)).str();
  set_codec_id(ba::to_upper_copy(codec_id));
  set_audio_sampling_freq((float)m_samples_per_sec);
  set_audio_channels(m_channels);
  set_audio_bit_depth(m_bits_per_sample);
  set_codec_private(m_private_data->get_buffer(), m_private_data->get_size());

  generic_packetizer_c::set_headers();
  m_track_entry->EnableLacing(false);
}

int
ra_packetizer_c::process(packet_cptr packet) {
  add_packet(packet);

  return FILE_STATUS_MOREDATA;
}

connection_result_e
ra_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                std::string &error_message) {
  ra_packetizer_c *psrc = dynamic_cast<ra_packetizer_c *>(src);
  if (NULL == psrc)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_a_samplerate(m_samples_per_sec, psrc->m_samples_per_sec);
  connect_check_a_channels(m_channels,          psrc->m_channels);
  connect_check_a_bitdepth(m_bits_per_sample,   psrc->m_bits_per_sample);

  return CAN_CONNECT_YES;
}
