/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   VobBtn packetizer

   Written by Steve Lhomme <steve.lhomme@free.fr>.
   Modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <matroska/KaxContentEncoding.h>

#include "common/codec.h"
#include "common/compression.h"
#include "common/endian.h"
#include "common/mm_io.h"
#include "merge/connection_checks.h"
#include "output/p_vobbtn.h"

using namespace libmatroska;

vobbtn_packetizer_c::vobbtn_packetizer_c(generic_reader_c *p_reader,
                                         track_info_c &p_ti,
                                         int width,
                                         int height)
  : generic_packetizer_c(p_reader, p_ti)
  , m_previous_timecode(0)
  , m_width(width)
  , m_height(height)
{
  set_track_type(track_buttons);
  set_default_compression_method(COMPRESSION_ZLIB);
}

vobbtn_packetizer_c::~vobbtn_packetizer_c() {
}

void
vobbtn_packetizer_c::set_headers() {
  set_codec_id(MKV_B_VOBBTN);

  set_video_pixel_width(m_width);
  set_video_pixel_height(m_height);

  generic_packetizer_c::set_headers();

  m_track_entry->EnableLacing(false);
}

int
vobbtn_packetizer_c::process(packet_cptr packet) {
  uint32_t vobu_start = get_uint32_be(packet->data->get_buffer() + 0x0d);
  uint32_t vobu_end   = get_uint32_be(packet->data->get_buffer() + 0x11);

  packet->duration = (int64_t)(100000.0 * (float)(vobu_end - vobu_start) / 9);
  if (-1 == packet->timecode) {
    packet->timecode     = m_previous_timecode;
    m_previous_timecode += packet->duration;
  }

  packet->duration_mandatory = true;
  add_packet(packet);

  return FILE_STATUS_MOREDATA;
}

connection_result_e
vobbtn_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                    std::string &error_message) {
  vobbtn_packetizer_c *vsrc = dynamic_cast<vobbtn_packetizer_c *>(src);
  if (!vsrc)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_v_width(m_width,   vsrc->m_width);
  connect_check_v_height(m_height, vsrc->m_height);

  return CAN_CONNECT_YES;
}
