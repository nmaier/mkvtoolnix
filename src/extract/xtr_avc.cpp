/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   extracts tracks from Matroska files into other files

   Written by Matt Rice <topquark@sluggy.net>.
*/

#include "common.h"
#include "commonebml.h"

#include "xtr_avc.h"

static const binary start_code[4] = { 0x00, 0x00, 0x00, 0x01 };

xtr_avc_c::xtr_avc_c(const string &_codec_id,
                     int64_t _tid,
                     track_spec_t &tspec):
  xtr_base_c(_codec_id, _tid, tspec) {
}

void
xtr_avc_c::write_nal(const binary *data,
                     int &pos,
                     int data_size,
                     int nal_size_size) {
  int i;
  int nal_size = 0;

  for (i = 0; i < nal_size_size; ++i)
    nal_size = (nal_size << 8) | data[pos++];

  if ((pos + nal_size) > data_size)
    mxerror("Track " LLD ": nal too big\n", tid);

  out->write(start_code, 4);
  out->write(data+pos, nal_size);
  pos += nal_size;
}

void
xtr_avc_c::create_file(xtr_base_c *_master,
                       KaxTrackEntry &track) {

  xtr_base_c::create_file(_master, track);

  KaxCodecPrivate *priv = FINDFIRST(&track, KaxCodecPrivate);
  if (NULL == priv)
    mxerror("Track " LLD " with the CodecID '%s' is missing the \"codec "
            "private\" element and cannot be extracted.\n", tid,
            codec_id.c_str());

  if (priv->GetSize() < 6)
    mxerror("Track " LLD " CodecPrivate is too small.\n", tid);

  binary *buf = priv->GetBuffer();
  nal_size_size = 1 + (buf[4] & 3);

  int i, pos = 6, numsps = buf[5] & 0x1f, numpps;

  for (i = 0; (i < numsps) && (priv->GetSize() > pos); ++i)
    write_nal(buf, pos, priv->GetSize(), 2);

  if (priv->GetSize() <= pos)
    return;

  numpps = buf[pos++];

  for (i = 0; (i < numpps) && (priv->GetSize() > pos); ++i)
    write_nal(buf, pos, priv->GetSize(), 2);
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
  int pos;
  binary *buf = (binary *)frame->get();

  pos = 0;
  while (frame->get_size() > pos)
    write_nal(buf, pos, frame->get_size(), nal_size_size);
}
