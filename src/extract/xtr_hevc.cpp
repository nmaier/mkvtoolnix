/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

*/

#ifndef MTX_XTR_HEVC_CPP
#define MTX_XTR_HEVC_CPP

#include "extract/xtr_hevc.h"

void
xtr_hevc_c::create_file(xtr_base_c *master,
                        KaxTrackEntry &track) {
  xtr_base_c::create_file(master, track);

  KaxCodecPrivate *priv = FindChild<KaxCodecPrivate>(&track);
  if (!priv)
    mxerror(boost::format(Y("Track %1% with the CodecID '%2%' is missing the \"codec private\" element and cannot be extracted.\n")) % m_tid % m_codec_id);

  memory_cptr mpriv = decode_codec_private(priv);

  if (mpriv->get_size() < 23)
    mxerror(boost::format(Y("Track %1% CodecPrivate is too small.\n")) % m_tid);

  binary *buf = mpriv->get_buffer();
  unsigned char *p = (unsigned char *)buf;
  p = p;
  m_nal_size_size = 1 + (buf[21] & 3);

  // Parameter sets in this order: vps, sps, pps, sei
  unsigned int num_parameter_sets = buf[22];

  size_t i = 0;
  size_t pos = 23;
  while((i < num_parameter_sets) && (mpriv->get_size() > pos)) {
    pos++; // skip over the nal unit type

    unsigned int nal_unit_count = (buf[pos] << 8) | buf[pos+1];
    pos += 2;

    for(size_t j = 0; (j < nal_unit_count) && (mpriv->get_size() > pos); j++, i++)
      if (!write_nal(buf, pos, mpriv->get_size(), 2))
        break;
  }
}

#endif
