/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   MPEG4 part 10 video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/codec.h"
#include "common/endian.h"
#include "common/hacks.h"
#include "common/math.h"
#include "common/mpeg4_p10.h"
#include "common/strings/formatting.h"
#include "merge/output_control.h"
#include "output/p_mpeg4_p10.h"

mpeg4_p10_video_packetizer_c::
mpeg4_p10_video_packetizer_c(generic_reader_c *p_reader,
                             track_info_c &p_ti,
                             double fps,
                             int width,
                             int height)
  : video_packetizer_c(p_reader, p_ti, MKV_V_MPEG4_AVC, fps, width, height)
  , m_nalu_size_len_src(0)
  , m_nalu_size_len_dst(0)
  , m_max_nalu_size(0)
{
  m_relaxed_timecode_checking = true;

  setup_nalu_size_len_change();

  set_codec_private(m_ti.m_private_data);
}

void
mpeg4_p10_video_packetizer_c::set_headers() {
  static auto s_debug_fix_bistream_timing_info = debugging_option_c{"fix_bitstream_timing_info"};

  if (m_ti.m_private_data && m_ti.m_private_data->get_size())
    extract_aspect_ratio();

  if (m_ti.m_private_data && m_ti.m_private_data->get_size() && m_ti.m_fix_bitstream_frame_rate) {
    int64_t l_track_default_duration = -1;

    if (m_timecode_factory)
      l_track_default_duration = m_timecode_factory->get_default_duration(-1);

    if ((-1 == l_track_default_duration) && m_default_duration_forced)
      l_track_default_duration = m_htrack_default_duration;

    if ((-1 == l_track_default_duration) && (0.0 < m_fps))
      l_track_default_duration = static_cast<int64_t>(1000000000.0 / m_fps);

    if (-1 != l_track_default_duration)
      l_track_default_duration /= 2;

    mxdebug_if(s_debug_fix_bistream_timing_info,
               boost::format("fix_bitstream_timing_info: factory default_duration %1% default_duration_forced? %2% htrack_default_duration %3% fps %4% l_track_default_duration %5%\n")
               % (m_timecode_factory ? m_timecode_factory->get_default_duration(-1) : -2)
               % m_default_duration_forced % m_htrack_default_duration
               % m_fps % l_track_default_duration);

    set_codec_private(mpeg4::p10::fix_sps_fps(m_ti.m_private_data, l_track_default_duration));
  }

  video_packetizer_c::set_headers();
}

void
mpeg4_p10_video_packetizer_c::extract_aspect_ratio() {
  auto result = mpeg4::p10::extract_par(m_ti.m_private_data);

  set_codec_private(result.new_avcc);

  if (!result.is_valid() || display_dimensions_or_aspect_ratio_set())
    return;

  auto par = static_cast<double>(result.numerator) / static_cast<double>(result.denominator);

  set_video_display_dimensions(1 <= par ? irnd(m_width * par) : m_width,
                               1 <= par ? m_height            : irnd(m_height / par),
                               OPTION_SOURCE_BITSTREAM);

  mxinfo_tid(m_ti.m_fname, m_ti.m_id,
             boost::format(Y("Extracted the aspect ratio information from the MPEG-4 layer 10 (AVC) video data and set the display dimensions to %1%/%2%.\n"))
             % m_ti.m_display_width % m_ti.m_display_height);
}

int
mpeg4_p10_video_packetizer_c::process(packet_cptr packet) {
  if (VFT_PFRAMEAUTOMATIC == packet->bref) {
    packet->fref = -1;
    packet->bref = m_ref_timecode;
  }

  m_ref_timecode = packet->timecode;

  if (m_nalu_size_len_dst && (m_nalu_size_len_dst != m_nalu_size_len_src))
    change_nalu_size_len(packet);

  remove_filler_nalus(*packet->data);

  add_packet(packet);

  return FILE_STATUS_MOREDATA;
}

connection_result_e
mpeg4_p10_video_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                             std::string &error_message) {
  mpeg4_p10_video_packetizer_c *vsrc = dynamic_cast<mpeg4_p10_video_packetizer_c *>(src);
  if (!vsrc)
    return CAN_CONNECT_NO_FORMAT;

  connection_result_e result = video_packetizer_c::can_connect_to(src, error_message);
  if (CAN_CONNECT_YES != result)
    return result;

  if (m_ti.m_private_data && vsrc->m_ti.m_private_data && memcmp(m_ti.m_private_data->get_buffer(), vsrc->m_ti.m_private_data->get_buffer(), m_ti.m_private_data->get_size())) {
    error_message = (boost::format(Y("The codec's private data does not match. Both have the same length (%1%) but different content.")) % m_ti.m_private_data->get_size()).str();
    return CAN_CONNECT_MAYBE_CODECPRIVATE;
  }

  return CAN_CONNECT_YES;
}

void
mpeg4_p10_video_packetizer_c::setup_nalu_size_len_change() {
  if (!m_ti.m_private_data || (5 > m_ti.m_private_data->get_size()))
    return;

  auto private_data   = m_ti.m_private_data->get_buffer();
  m_nalu_size_len_src = (private_data[4] & 0x03) + 1;
  m_nalu_size_len_dst = m_nalu_size_len_src;

  if (!m_ti.m_nalu_size_length || (m_ti.m_nalu_size_length == m_nalu_size_len_src))
    return;

  m_nalu_size_len_dst = m_ti.m_nalu_size_length;
  private_data[4]     = (private_data[4] & 0xfc) | (m_nalu_size_len_dst - 1);
  m_max_nalu_size     = 1ll << (8 * m_nalu_size_len_dst);

  set_codec_private(m_ti.m_private_data);

  mxverb(2, boost::format("mpeg4_p10: Adjusting NALU size length from %1% to %2%\n") % m_nalu_size_len_src % m_nalu_size_len_dst);
}

void
mpeg4_p10_video_packetizer_c::change_nalu_size_len(packet_cptr packet) {
  unsigned char *src = packet->data->get_buffer();
  int size           = packet->data->get_size();

  if (!src || !size)
    return;

  std::vector<int> nalu_sizes;

  int src_pos = 0;

  // Find all NALU sizes in this packet.
  while (src_pos < size) {
    if ((size - src_pos) < m_nalu_size_len_src)
      break;

    int nalu_size = 0;
    int i;
    for (i = 0; i < m_nalu_size_len_src; ++i)
      nalu_size = (nalu_size << 8) + src[src_pos + i];

    if ((size - src_pos - m_nalu_size_len_src) < nalu_size)
      nalu_size = size - src_pos - m_nalu_size_len_src;

    if (nalu_size > m_max_nalu_size)
      mxerror_tid(m_ti.m_fname, m_ti.m_id, boost::format(Y("The chosen NALU size length of %1% is too small. Try using '4'.\n")) % m_nalu_size_len_dst);

    src_pos += m_nalu_size_len_src + nalu_size;

    nalu_sizes.push_back(nalu_size);
  }

  // Allocate memory if the new NALU size length is greater
  // than the previous one. Otherwise reuse the existing memory.
  if (m_nalu_size_len_dst > m_nalu_size_len_src) {
    int new_size = size + nalu_sizes.size() * (m_nalu_size_len_dst - m_nalu_size_len_src);
    packet->data = memory_cptr(new memory_c((unsigned char *)safemalloc(new_size), new_size, true));
  }

  // Copy the NALUs and write the new sized length field.
  unsigned char *dst = packet->data->get_buffer();
  src_pos            = 0;
  int dst_pos        = 0;

  size_t i;
  for (i = 0; nalu_sizes.size() > i; ++i) {
    int nalu_size = nalu_sizes[i];

    int shift;
    for (shift = 0; shift < m_nalu_size_len_dst; ++shift)
      dst[dst_pos + shift] = (nalu_size >> (8 * (m_nalu_size_len_dst - 1 - shift))) & 0xff;

    memmove(&dst[dst_pos + m_nalu_size_len_dst], &src[src_pos + m_nalu_size_len_src], nalu_size);

    src_pos += m_nalu_size_len_src + nalu_size;
    dst_pos += m_nalu_size_len_dst + nalu_size;
  }

  packet->data->set_size(dst_pos);
}

void
mpeg4_p10_video_packetizer_c::remove_filler_nalus(memory_c &data)
  const {
  auto ptr        = data.get_buffer();
  auto total_size = data.get_size();
  auto idx        = 0u;

  while ((idx + m_nalu_size_len_dst) < total_size) {
    uint64_t nalu_size = (4 == m_nalu_size_len_dst) ? get_uint32_be(&ptr[idx])
                       : (3 == m_nalu_size_len_dst) ? get_uint24_be(&ptr[idx])
                       :                              get_uint16_be(&ptr[idx]);

    nalu_size         += m_nalu_size_len_dst;

    if ((idx + nalu_size) > total_size)
      break;

    if (ptr[idx + m_nalu_size_len_dst] == NALU_TYPE_FILLER_DATA) {
      memmove(&ptr[idx], &ptr[idx + nalu_size], total_size - idx - nalu_size);
      total_size -= nalu_size;
    }

    idx += nalu_size;
  }

  if (data.get_size() == total_size)
    return;

  data.resize(total_size);
}
