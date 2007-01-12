/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id: xtr_avc.cpp 3436 2007-01-07 20:13:06Z mosu $

   extracts tracks from Matroska files into other files

   Written by Matt Rice <topquark@sluggy.net>.
*/

#include "os.h"

#include "common.h"
#include "commonebml.h"
#include "mpeg4_common.h"

#include "xtr_mpeg1_2.h"

xtr_mpeg1_2_video_c::xtr_mpeg1_2_video_c(const string &_codec_id,
                                         int64_t _tid,
                                         track_spec_t &tspec):
  xtr_base_c(_codec_id, _tid, tspec) {
}

void
xtr_mpeg1_2_video_c::create_file(xtr_base_c *_master,
                                 KaxTrackEntry &track) {

  xtr_base_c::create_file(_master, track);

  KaxCodecPrivate *priv = FINDFIRST(&track, KaxCodecPrivate);
  if (NULL != priv)
    m_seq_hdr = clone_memory(priv->GetBuffer(), priv->GetSize());
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
  binary *buf = (binary *)frame->get();

  if (references_valid)
    keyframe = bref == 0;

  if (keyframe && (NULL != m_seq_hdr.get())) {
    uint32_t marker;
    bool seq_hdr_found = false;

    if (frame->get_size() >= 4) {
      marker = get_uint32_be(buf);
      if (MPEGVIDEO_SEQUENCE_START_CODE == marker)
        seq_hdr_found = true;
    }

    if (!seq_hdr_found)
      out->write(m_seq_hdr->get(), m_seq_hdr->get_size());
  }

  out->write(buf, frame->get_size());
}
