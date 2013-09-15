/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Matt Rice <topquark@sluggy.net>.
*/

#include "common/common_pch.h"

#include "common/ebml.h"
#include "common/endian.h"
#include "common/mpeg1_2.h"
#include "extract/xtr_mpeg1_2.h"

xtr_mpeg1_2_video_c::xtr_mpeg1_2_video_c(const std::string &codec_id,
                                         int64_t tid,
                                         track_spec_t &tspec)
  : xtr_base_c(codec_id, tid, tspec)
{
}

void
xtr_mpeg1_2_video_c::create_file(xtr_base_c *master,
                                 KaxTrackEntry &track) {

  xtr_base_c::create_file(master, track);

  KaxCodecPrivate *priv = FindChild<KaxCodecPrivate>(&track);
  if (priv)
    m_seq_hdr = decode_codec_private(priv);
}

void
xtr_mpeg1_2_video_c::handle_frame(xtr_frame_t &f) {
  m_content_decoder.reverse(f.frame, CONTENT_ENCODING_SCOPE_BLOCK);

  binary *buf = (binary *)f.frame->get_buffer();

  if (f.references_valid)
    f.keyframe = (0 == f.bref);

  if (f.keyframe && m_seq_hdr) {
    bool seq_hdr_found = false;
    size_t size        = f.frame->get_size();

    if (4 <= size) {
      uint32_t marker = get_uint32_be(buf);
      seq_hdr_found   = MPEGVIDEO_SEQUENCE_HEADER_START_CODE == marker;

      if (seq_hdr_found && (8 <= size)) {
        size_t end_pos = 7;
        marker         = get_uint32_be(&buf[4]);
        while (   ((end_pos + 1) < size)
               && (   (0x00000100               != (marker & 0xffffff00))
                   || (MPEGVIDEO_EXT_START_CODE == marker))) {
          ++end_pos;
          marker = (marker << 8) | buf[end_pos];
        }

        size_t seq_size = end_pos - (((end_pos + 1) < size) ? 3 : 4);

        if ((m_seq_hdr->get_size() != seq_size) || memcmp(m_seq_hdr->get_buffer(), buf, seq_size))
          m_seq_hdr = memory_c::clone(buf, seq_size);
      }
    }

    if (!seq_hdr_found)
      m_out->write(m_seq_hdr);
  }

  m_out->write(buf, f.frame->get_size());
}

void
xtr_mpeg1_2_video_c::handle_codec_state(memory_cptr &codec_state) {
  m_content_decoder.reverse(codec_state, CONTENT_ENCODING_SCOPE_CODECPRIVATE);
  m_seq_hdr = codec_state->clone();
}
