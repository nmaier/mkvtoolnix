/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   PASSTHROUGH output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/matroska.h"
#include "merge/pr_generic.h"
#include "output/p_passthrough.h"

using namespace libmatroska;

passthrough_packetizer_c::passthrough_packetizer_c(generic_reader_c *p_reader,
                                                   track_info_c &p_ti)
  : generic_packetizer_c(p_reader, p_ti)
{
  m_timecode_factory_application_mode = TFA_FULL_QUEUEING;
}

void
passthrough_packetizer_c::set_headers() {
  generic_packetizer_c::set_headers();
}

int
passthrough_packetizer_c::process(packet_cptr packet) {
  add_packet(packet);

  return FILE_STATUS_MOREDATA;
}

#define CMP(member) (member != psrc->member)

connection_result_e
passthrough_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                         std::string &error_message) {
  passthrough_packetizer_c *psrc = dynamic_cast<passthrough_packetizer_c *>(src);
  if (NULL == psrc)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_codec_id(m_hcodec_id, psrc->m_hcodec_id);

  if (CMP(m_htrack_type) || CMP(m_hcodec_id))
    return CAN_CONNECT_NO_PARAMETERS;

  if (   ((NULL == m_ti.m_private_data) && (NULL != psrc->m_ti.m_private_data))
      || ((NULL != m_ti.m_private_data) && (NULL == psrc->m_ti.m_private_data))
      || (m_ti.m_private_size != psrc->m_ti.m_private_size)
      || (   (NULL != m_ti.m_private_data)
          && (NULL != psrc->m_ti.m_private_data)
          && (m_ti.m_private_size == psrc->m_ti.m_private_size)
          && memcmp(m_ti.m_private_data, psrc->m_ti.m_private_data, m_ti.m_private_size))) {
    error_message = (boost::format(Y("The codec's private data does not match (lengths: %1% and %2%).")) % m_ti.m_private_size % psrc->m_ti.m_private_size).str();
    return CAN_CONNECT_MAYBE_CODECPRIVATE;
  }

  switch (m_htrack_type) {
    case track_video:
      connect_check_v_width(m_hvideo_pixel_width,      psrc->m_hvideo_pixel_width);
      connect_check_v_height(m_hvideo_pixel_height,    psrc->m_hvideo_pixel_height);
      connect_check_v_dwidth(m_hvideo_display_width,   psrc->m_hvideo_display_width);
      connect_check_v_dheight(m_hvideo_display_height, psrc->m_hvideo_display_height);
      if (CMP(m_htrack_min_cache) || CMP(m_htrack_max_cache) || CMP(m_htrack_default_duration))
        return CAN_CONNECT_NO_PARAMETERS;
      break;

    case track_audio:
      connect_check_a_samplerate(m_haudio_sampling_freq, psrc->m_haudio_sampling_freq);
      connect_check_a_channels(m_haudio_channels,        psrc->m_haudio_channels);
      connect_check_a_bitdepth(m_haudio_bit_depth,       psrc->m_haudio_bit_depth);
      if (CMP(m_htrack_default_duration))
        return CAN_CONNECT_NO_PARAMETERS;
      break;

    case track_subtitle:
      break;

    default:
      break;
  }

  return CAN_CONNECT_YES;
}
