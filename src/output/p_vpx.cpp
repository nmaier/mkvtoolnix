/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   VPX video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/codec.h"
#include "common/ivf.h"
#include "merge/connection_checks.h"
#include "output/p_vpx.h"

using namespace libmatroska;

vpx_video_packetizer_c::vpx_video_packetizer_c(generic_reader_c *p_reader,
                                               track_info_c &p_ti,
                                               codec_type_e p_codec)
  : generic_packetizer_c(p_reader, p_ti)
  , m_previous_timecode(-1)
  , m_codec{p_codec}
{
  m_timecode_factory_application_mode = TFA_SHORT_QUEUEING;

  set_track_type(track_video);
  set_codec_id(p_codec == CT_V_VP8 ? MKV_V_VP8 : MKV_V_VP9);
}

void
vpx_video_packetizer_c::set_headers() {
  generic_packetizer_c::set_headers();
}

int
vpx_video_packetizer_c::process(packet_cptr packet) {
  packet->bref        = ivf::is_keyframe(packet->data, m_codec) ? -1 : m_previous_timecode;
  m_previous_timecode = packet->timecode;

  add_packet(packet);

  return FILE_STATUS_MOREDATA;
}

connection_result_e
vpx_video_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                       std::string &error_message) {
  vpx_video_packetizer_c *psrc = dynamic_cast<vpx_video_packetizer_c *>(src);
  if (!psrc)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_codec_id(m_hcodec_id, psrc->m_hcodec_id);

  connect_check_v_width(m_hvideo_pixel_width,      psrc->m_hvideo_pixel_width);
  connect_check_v_height(m_hvideo_pixel_height,    psrc->m_hvideo_pixel_height);

  return CAN_CONNECT_YES;
}

bool
vpx_video_packetizer_c::is_compatible_with(output_compatibility_e compatibility) {
  return (OC_MATROSKA == compatibility) || (OC_WEBM == compatibility);
}
