/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Matt Rice <topquark@sluggy.net>.
*/

#include "common/os.h"

#include "common/common.h"
#include "common/ebml.h"
#include "extract/xtr_avc.h"

static const binary s_start_code[4] = { 0x00, 0x00, 0x00, 0x01 };

xtr_avc_c::xtr_avc_c(const std::string &codec_id,
                     int64_t tid,
                     track_spec_t &tspec)
  : xtr_base_c(codec_id, tid, tspec)
{
}

void
xtr_avc_c::write_nal(const binary *data,
                     int &pos,
                     int data_size,
                     int write_nal_size_size) {
  int i;
  int nal_size = 0;

  for (i = 0; i < write_nal_size_size; ++i)
    nal_size = (nal_size << 8) | data[pos++];

  if ((pos + nal_size) > data_size)
    mxerror(boost::format(Y("Track %1%: NAL too big\n")) % m_tid);

  m_out->write(s_start_code, 4);
  m_out->write(data + pos, nal_size);

  pos += nal_size;
}

void
xtr_avc_c::create_file(xtr_base_c *master,
                       KaxTrackEntry &track) {
  xtr_base_c::create_file(master, track);

  KaxCodecPrivate *priv = FINDFIRST(&track, KaxCodecPrivate);
  if (NULL == priv)
    mxerror(boost::format(Y("Track %1% with the CodecID '%2%' is missing the \"codec private\" element and cannot be extracted.\n")) % m_tid % m_codec_id);

  memory_cptr mpriv = decode_codec_private(priv);

  if (mpriv->get_size() < 6)
    mxerror(boost::format(Y("Track %1% CodecPrivate is too small.\n")) % m_tid);

  binary *buf = mpriv->get();
  m_nal_size_size = 1 + (buf[4] & 3);

  int pos    = 6;
  int numsps = buf[5] & 0x1f;
  int i;
  for (i = 0; (i < numsps) && (mpriv->get_size() > pos); ++i)
    write_nal(buf, pos, mpriv->get_size(), 2);

  if (mpriv->get_size() <= pos)
    return;

  int numpps = buf[pos++];

  for (i = 0; (i < numpps) && (mpriv->get_size() > pos); ++i)
    write_nal(buf, pos, mpriv->get_size(), 2);
}

void
xtr_avc_c::handle_frame(memory_cptr &frame,
                        KaxBlockAdditions *additions,
                        int64_t timecode,
                        int64_t duration,
                        int64_t bref,
                        int64_t fref,
                        bool keyframe,
                        bool discardable,
                        bool references_valid) {
  m_content_decoder.reverse(frame, CONTENT_ENCODING_SCOPE_BLOCK);

  int pos     = 0;
  binary *buf = (binary *)frame->get();

  while (frame->get_size() > pos)
    write_nal(buf, pos, frame->get_size(), m_nal_size_size);
}
