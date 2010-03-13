/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Matt Rice <topquark@sluggy.net>.
*/

#include "common/common.h"

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

  KaxCodecPrivate *priv = FINDFIRST(&track, KaxCodecPrivate);
  if (NULL != priv) {
    memory_cptr mpriv   = decode_codec_private(priv);
    m_seq_hdr           = clone_memory(mpriv);
  }
}

void
xtr_mpeg1_2_video_c::handle_frame(memory_cptr &frame,
                                  KaxBlockAdditions *additions,
                                  int64_t timecode,
                                  int64_t duration,
                                  int64_t bref,
                                  int64_t fref,
                                  bool keyframe,
                                  bool discardable,
                                  bool references_valid) {
  m_content_decoder.reverse(frame, CONTENT_ENCODING_SCOPE_BLOCK);

  binary *buf = (binary *)frame->get_buffer();

  if (references_valid)
    keyframe = (0 == bref);

  if (keyframe && m_seq_hdr.is_set()) {
    bool seq_hdr_found = false;

    if (frame->get_size() >= 4) {
      uint32_t marker = get_uint32_be(buf);
      if (MPEGVIDEO_SEQUENCE_START_CODE == marker)
        seq_hdr_found = true;
    }

    if (!seq_hdr_found)
      m_out->write(m_seq_hdr);
  }

  m_out->write(buf, frame->get_size());
}

void
xtr_mpeg1_2_video_c::handle_codec_state(memory_cptr &codec_state) {
  m_content_decoder.reverse(codec_state, CONTENT_ENCODING_SCOPE_CODECPRIVATE);
  m_seq_hdr = clone_memory(codec_state);
}
