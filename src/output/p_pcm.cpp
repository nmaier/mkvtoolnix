/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   PCM output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Patches by Robert Millan <rmh@aybabtu.com>.
*/

#include "common/common_pch.h"

#include "common/matroska.h"
#include "merge/connection_checks.h"
#include "output/p_pcm.h"

using namespace libmatroska;

pcm_packetizer_c::pcm_packetizer_c(generic_reader_c *p_reader,
                                   track_info_c &p_ti,
                                   int samples_per_sec,
                                   int channels,
                                   int bits_per_sample,
                                   bool ieee_float)
  : generic_packetizer_c(p_reader, p_ti)
  , m_samples_per_sec(samples_per_sec)
  , m_channels(channels)
  , m_bits_per_sample(bits_per_sample)
  , m_packet_size(0)
  , m_samples_output(0)
  , m_ieee_float(ieee_float)
  , m_s2tc(1000000000ll, m_samples_per_sec)
{

  int i;
  for (i = 32; 2 < i; i >>= 2)
    if ((m_samples_per_sec % i) == 0)
      break;

  if ((2 == i) && ((m_samples_per_sec % 5) == 0))
    i = 5;

  m_samples_per_packet = samples_per_sec / i;

  set_track_type(track_audio);
  set_track_default_duration((int64_t)(1000000000.0 * m_samples_per_packet / m_samples_per_sec));

  /* It could happen that (channels * bits_per_sample < 8).  Because of this,
     we mustn't divide by 8 in the same line, or the result would be hosed. */
  m_packet_size  = m_samples_per_packet * m_channels * m_bits_per_sample;
  m_packet_size /= 8;
}

pcm_packetizer_c::~pcm_packetizer_c() {
}

void
pcm_packetizer_c::set_headers() {
  if (m_ieee_float)
    set_codec_id(MKV_A_PCM_FLOAT);

  else if (big_endian)
    set_codec_id(MKV_A_PCM_BE);

  else
    set_codec_id(MKV_A_PCM);

  set_audio_sampling_freq((float)m_samples_per_sec);
  set_audio_channels(m_channels);
  set_audio_bit_depth(m_bits_per_sample);

  generic_packetizer_c::set_headers();
}

int
pcm_packetizer_c::process(packet_cptr packet) {
  if (packet->has_timecode())
    return process_packaged(packet);

  m_buffer.add(packet->data->get_buffer(), packet->data->get_size());

  while (m_buffer.get_size() >= m_packet_size) {
    add_packet(new packet_t(memory_c::clone(m_buffer.get_buffer(), m_packet_size), m_samples_output * m_s2tc, m_samples_per_packet * m_s2tc));

    m_buffer.remove(m_packet_size);
    m_samples_output += m_samples_per_packet;
  }

  return FILE_STATUS_MOREDATA;
}

int
pcm_packetizer_c::process_packaged(packet_cptr packet) {
  int64_t samples_here = m_buffer.get_size() * 8 / m_channels / m_bits_per_sample;
  m_samples_output     = packet->timecode / m_s2tc + samples_here;

  add_packet(new packet_t(packet->data, packet->timecode, samples_here * m_s2tc));

  return FILE_STATUS_MOREDATA;
}

void
pcm_packetizer_c::flush_impl() {
  uint32_t size = m_buffer.get_size();
  if (0 >= size)
    return;

  int64_t samples_here = size * 8 / m_channels / m_bits_per_sample;
  add_packet(new packet_t(memory_c::clone(m_buffer.get_buffer(), size), m_samples_output * m_s2tc, samples_here * m_s2tc));

  m_samples_output += samples_here;
  m_buffer.remove(size);
}

connection_result_e
pcm_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                 std::string &error_message) {
  pcm_packetizer_c *psrc = dynamic_cast<pcm_packetizer_c *>(src);
  if (!psrc)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_a_samplerate(m_samples_per_sec, psrc->m_samples_per_sec);
  connect_check_a_channels(m_channels, psrc->m_channels);
  connect_check_a_bitdepth(m_bits_per_sample, psrc->m_bits_per_sample);

  return CAN_CONNECT_YES;
}
