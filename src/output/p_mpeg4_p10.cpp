/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   MPEG4 part 10 video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include "common/common.h"
#include "common/endian.h"
#include "common/hacks.h"
#include "common/math.h"
#include "common/matroska.h"
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

  relaxed_timecode_checking = true;

  if ((NULL != ti.private_data) && (0 < ti.private_size)) {
    extract_aspect_ratio();
    setup_nalu_size_len_change();
  }
}

void
mpeg4_p10_video_packetizer_c::extract_aspect_ratio() {
  uint32_t num, den;
  unsigned char *priv = ti.private_data;

  if (mpeg4::p10::extract_par(ti.private_data, ti.private_size, num, den) && (0 != num) && (0 != den)) {
    if (!ti.aspect_ratio_given && !ti.display_dimensions_given) {
      double par = (double)num / (double)den;

      if (1 <= par) {
        ti.display_width  = irnd(m_width * par);
        ti.display_height = m_height;

      } else {
        ti.display_width  = m_width;
        ti.display_height = irnd(m_height / par);

      }

      ti.display_dimensions_given = true;
      mxinfo_tid(ti.fname, ti.id,
                 boost::format(Y("Extracted the aspect ratio information from the MPEG-4 layer 10 (AVC) video data and set the display dimensions to %1%/%2%.\n"))
                 % ti.display_width % ti.display_height);
    }

    set_codec_private(ti.private_data, ti.private_size);
  }

  if (priv != ti.private_data)
    safefree(priv);
}

int
mpeg4_p10_video_packetizer_c::process(packet_cptr packet) {
  if (VFT_PFRAMEAUTOMATIC == packet->bref) {
    packet->fref = -1;
    packet->bref = m_ref_timecode;
  }

  m_ref_timecode = packet->timecode;

  if (m_nalu_size_len_dst)
    change_nalu_size_len(packet);

  add_packet(packet);

  return FILE_STATUS_MOREDATA;
}

connection_result_e
mpeg4_p10_video_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                             string &error_message) {
  mpeg4_p10_video_packetizer_c *vsrc = dynamic_cast<mpeg4_p10_video_packetizer_c *>(src);
  if (NULL == vsrc)
    return CAN_CONNECT_NO_FORMAT;

  connection_result_e result = video_packetizer_c::can_connect_to(src, error_message);
  if (CAN_CONNECT_YES != result)
    return result;

  if ((NULL != ti.private_data) && memcmp(ti.private_data, vsrc->ti.private_data, ti.private_size)) {
    error_message = (boost::format(Y("The codec's private data does not match. Both have the same length (%1%) but different content.")) % ti.private_size).str();
    return CAN_CONNECT_MAYBE_CODECPRIVATE;
  }

  return CAN_CONNECT_YES;
}

void
mpeg4_p10_video_packetizer_c::setup_nalu_size_len_change() {
  if (!ti.private_data || (5 > ti.private_size))
    return;

  m_nalu_size_len_src = (ti.private_data[4] & 0x03) + 1;

  if (!ti.nalu_size_length || (ti.nalu_size_length == m_nalu_size_len_src))
    return;

  m_nalu_size_len_dst = ti.nalu_size_length;
  ti.private_data[4]  = (ti.private_data[4] & 0xfc) | (m_nalu_size_len_dst - 1);
  m_max_nalu_size     = 1ll << (8 * m_nalu_size_len_dst);

  set_codec_private(ti.private_data, ti.private_size);

  mxverb(2, boost::format("mpeg4_p10: Adjusting NALU size length from %1% to %2%\n") % m_nalu_size_len_src % m_nalu_size_len_dst);
}

void
mpeg4_p10_video_packetizer_c::change_nalu_size_len(packet_cptr packet) {
  unsigned char *src = packet->data->get();
  int size           = packet->data->get_size();

  if (!src || !size)
    return;

  vector<int> nalu_sizes;

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
      mxerror_tid(ti.fname, ti.id, boost::format(Y("The chosen NALU size length of %1% is too small. Try using '4'.\n")) % m_nalu_size_len_dst);

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
  unsigned char *dst = packet->data->get();
  src_pos            = 0;
  int dst_pos        = 0;

  int i;
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

