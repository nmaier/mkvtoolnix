/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   WAVPACK output module

   Written by Steve Lhomme <steve.lhomme@free.fr>.
*/

#include "common/common_pch.h"

#include "common/endian.h"
#include "common/math.h"
#include "common/matroska.h"
#include "output/p_wavpack.h"

using namespace libmatroska;

wavpack_packetizer_c::wavpack_packetizer_c(generic_reader_c *p_reader,
                                           track_info_c &p_ti,
                                           wavpack_meta_t &meta)
  : generic_packetizer_c(p_reader, p_ti)
  , m_channels(meta.channel_count)
  , m_sample_rate(meta.sample_rate)
  , m_bits_per_sample(meta.bits_per_sample)
  , m_samples_per_block(meta.samples_per_block)
  , m_samples_output(0)
  , m_has_correction(meta.has_correction && (0 != m_htrack_max_add_block_ids))
{
  set_track_type(track_audio);
}

void
wavpack_packetizer_c::set_headers() {
  set_codec_id(MKV_A_WAVPACK4);
  set_codec_private(m_ti.m_private_data, m_ti.m_private_size);
  set_audio_sampling_freq((float)m_sample_rate);
  set_audio_channels(m_channels);
  set_audio_bit_depth(m_bits_per_sample);
  set_track_default_duration(m_samples_per_block * 1000000000 / m_sample_rate);
  set_track_max_additionals(m_has_correction ? 1 : 0);

  generic_packetizer_c::set_headers();

  m_track_entry->EnableLacing(!m_has_correction);
}

int
wavpack_packetizer_c::process(packet_cptr packet) {
  int64_t samples = get_uint32_le(packet->data->get_buffer());

  if (-1 == packet->duration)
    packet->duration = irnd(samples * 1000000000 / m_sample_rate);
  else
    mxverb(2, boost::format("wavpack_packetizer: incomplete block with duration %1%\n") % packet->duration);

  if (-1 == packet->timecode)
    packet->timecode = irnd((double)m_samples_output * 1000000000 / m_sample_rate);

  m_samples_output += samples;
  add_packet(packet);

  return FILE_STATUS_MOREDATA;
}

connection_result_e
wavpack_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                     std::string &error_message) {
  wavpack_packetizer_c *psrc = dynamic_cast<wavpack_packetizer_c *>(src);
  if (nullptr == psrc)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_a_samplerate(m_sample_rate,   psrc->m_sample_rate);
  connect_check_a_channels(m_channels,        psrc->m_channels);
  connect_check_a_bitdepth(m_bits_per_sample, psrc->m_bits_per_sample);

  return CAN_CONNECT_YES;
}
