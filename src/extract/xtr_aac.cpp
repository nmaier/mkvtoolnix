/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/aac.h"
#include "common/ebml.h"
#include "common/matroska.h"
#include "extract/xtr_aac.h"

xtr_aac_c::xtr_aac_c(const std::string &codec_id,
                     int64_t tid,
                     track_spec_t &tspec)
  : xtr_base_c(codec_id, tid, tspec)
  , m_channels(0)
  , m_id(0)
  , m_profile(0)
  , m_srate_idx(0)
{
}

void
xtr_aac_c::create_file(xtr_base_c *master,
                       KaxTrackEntry &track) {
  xtr_base_c::create_file(master, track);

  m_channels = kt_get_a_channels(track);
  int sfreq  = (int)kt_get_a_sfreq(track);

  if (m_codec_id == MKV_A_AAC) {
    KaxCodecPrivate *priv = FindChild<KaxCodecPrivate>(&track);
    if (!priv)
      mxerror(boost::format(Y("Track %1% with the CodecID '%2%' is missing the \"codec private\" element and cannot be extracted.\n")) % m_tid % m_codec_id);

    int output_sfreq = 0;
    bool is_sbr      = false;

    memory_cptr mem(new memory_c(priv->GetBuffer(), priv->GetSize(), false));
    m_content_decoder.reverse(mem, CONTENT_ENCODING_SCOPE_CODECPRIVATE);

    if (!parse_aac_data(mem->get_buffer(), mem->get_size(), m_profile, m_channels, sfreq, output_sfreq, is_sbr))
      mxerror(boost::format(Y("Track %1% with the CodecID '%2%' contains invalid \"codec private\" data for AAC.\n")) % m_tid % m_codec_id);
    m_id = 0;

  } else {
    // A_AAC/MPEG4/MAIN
    // 0123456789012345
    if (m_codec_id[10] == '4')
      m_id = 0;
    else if (m_codec_id[10] == '2')
      m_id = 1;
    else
      mxerror(boost::format(Y("Track ID %1% has an unknown AAC type.\n")) % m_tid);

    if (!strcmp(&m_codec_id[12], "MAIN"))
      m_profile = 0;
    else if (   !strcmp(&m_codec_id[12], "LC")
             ||  strstr(&m_codec_id[12], "SBR"))
      m_profile = 1;
    else if (!strcmp(&m_codec_id[12], "SSR"))
      m_profile = 2;
    else if (!strcmp(&m_codec_id[12], "LTP"))
      m_profile = 3;
    else
      mxerror(boost::format(Y("Track ID %1% has an unknown AAC type.\n")) % m_tid);
  }

  if (92017 <= sfreq)
    m_srate_idx = 0;
  else if (75132 <= sfreq)
    m_srate_idx = 1;
  else if (55426 <= sfreq)
    m_srate_idx = 2;
  else if (46009 <= sfreq)
    m_srate_idx = 3;
  else if (37566 <= sfreq)
    m_srate_idx = 4;
  else if (27713 <= sfreq)
    m_srate_idx = 5;
  else if (23004 <= sfreq)
    m_srate_idx = 6;
  else if (18783 <= sfreq)
    m_srate_idx = 7;
  else if (13856 <= sfreq)
    m_srate_idx = 8;
  else if (11502 <= sfreq)
    m_srate_idx = 9;
  else if ( 9391 <= sfreq)
    m_srate_idx = 10;
  else
    m_srate_idx = 11;
}

#ifdef COMP_MSC
#pragma warning(disable:4309)	//truncation of constant data. 0xff is an int.
#endif

void
xtr_aac_c::handle_frame(xtr_frame_t &f) {
  // Recreate the ADTS headers. What a fun. Like runing headlong into
  // a solid wall. But less painful. Well such is life, you know.
  // But then again I've just seen a beautiful girl walking by my
  // window, and suddenly the world is a bright place. Everything's
  // a matter of perspective. And if I didn't enjoy writing even this
  // code then I wouldn't do it at all. So let's get to it!

  char adts[56 / 8];
  // sync word, 12 bits
  adts[0]  = 0xff;
  adts[1]  = 0xf0;

  // ID, 1 bit
  adts[1] |= m_id << 3;
  // layer: 2 bits = 00

  // protection absent: 1 bit = 1 (ASSUMPTION!)
  adts[1] |= 1;

  // profile, 2 bits
  adts[2]  = m_profile << 6;

  // sampling frequency index, 4 bits
  adts[2] |= m_srate_idx << 2;

  // private, 1 bit = 0 (ASSUMPTION!)

  // channels, 3 bits
  adts[2] |= (m_channels & 4) >> 2;
  adts[3]  = (m_channels & 3) << 6;

  // original/copy, 1 bit = 0(ASSUMPTION!)

  // home, 1 bit = 0 (ASSUMPTION!)

  // copyright id bit, 1 bit = 0 (ASSUMPTION!)

  // copyright id start, 1 bit = 0 (ASSUMPTION!)

  // frame length, 13 bits
  int len  = f.frame->get_size() + 7;
  adts[3] |= len >> 11;
  adts[4]  = (len >> 3) & 0xff;
  adts[5]  = (len & 7) << 5;

  // adts buffer fullness, 11 bits, 0x7ff = VBR (ASSUMPTION!)
  adts[5] |= 0x1f;
  adts[6]  = 0xfc;

  // number of raw frames, 2 bits, 0 (meaning 1 frame) (ASSUMPTION!)

  // Write the ADTS header and the data itself.
  m_out->write(adts, 56 / 8);
  m_out->write(f.frame);
}
