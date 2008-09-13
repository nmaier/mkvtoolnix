/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include "commonebml.h"

#include "xtr_avi.h"

xtr_avi_c::xtr_avi_c(const string &codec_id,
                     int64_t tid,
                     track_spec_t &tspec)
  : xtr_base_c(codec_id, tid, tspec)
  , m_avi(NULL)
  , m_fps(0.0)
  , m_bih(NULL)
{
}

void
xtr_avi_c::create_file(xtr_base_c *master,
                       KaxTrackEntry &track) {
  init_content_decoder(track);

  KaxCodecPrivate *priv = FINDFIRST(&track, KaxCodecPrivate);
  if (NULL == priv)
    mxerror("Track " LLD " with the CodecID '%s' is missing the \"codec private\" element and cannot be extracted.\n", m_tid, m_codec_id.c_str());

  m_default_duration = kt_get_default_duration(track);
  if (0 >= m_default_duration)
    mxerror("Track " LLD " with the CodecID '%s' is missing the \"default duration\" element and cannot be extracted.\n", m_tid, m_codec_id.c_str());

  m_fps = (double)1000000000.0 / (double)m_default_duration;

  if (NULL != master)
    mxerror("Cannot write track " LLD " with the CodecID '%s' to the file '%s' because track " LLD " with the CodecID '%s' is already being"
            " written to the same file.\n", m_tid, m_codec_id.c_str(), m_file_name.c_str(), master->m_tid, master->m_codec_id.c_str());

  m_avi = AVI_open_output_file(m_file_name.c_str());
  if (NULL == m_avi)
    mxerror("The file '%s' could not be opened for writing (%s).\n", m_file_name.c_str(), AVI_strerror());

  string writing_app = "mkvextract";
  if (!no_variable_data)
    writing_app += mxsprintf(" %s", VERSION);
  m_avi->writing_app = safestrdup(writing_app.c_str());

  char ccodec[5];
  memory_cptr mpriv = decode_codec_private(priv);
  m_bih             = (alBITMAPINFOHEADER *)safememdup(mpriv->get(), mpriv->get_size());
  memcpy(ccodec, &m_bih->bi_compression, 4);
  ccodec[4]         = 0;

  if (get_uint32_le(&m_bih->bi_size) != sizeof(alBITMAPINFOHEADER)) {
    m_avi->extradata      = m_bih + 1;
    m_avi->extradata_size = get_uint32_le(&m_bih->bi_size) - sizeof(alBITMAPINFOHEADER);
  }

  AVI_set_video(m_avi, kt_get_v_pixel_width(track), kt_get_v_pixel_height(track), m_fps, ccodec);
}

void
xtr_avi_c::handle_frame(memory_cptr &frame,
                        KaxBlockAdditions *additions,
                        int64_t timecode,
                        int64_t duration,
                        int64_t bref,
                        int64_t fref,
                        bool keyframe,
                        bool discardable,
                        bool references_valid) {
  if (references_valid)
    keyframe = (bref == 0);

  m_content_decoder.reverse(frame, CONTENT_ENCODING_SCOPE_BLOCK);
  AVI_write_frame(m_avi, (char *)frame->get(), frame->get_size(), keyframe);

  if (((double)duration / 1000000.0 - (1000.0 / m_fps)) >= 1.5) {
    int k;
    int nfr = irnd((double)duration / 1000000.0 * m_fps / 1000.0);
    for (k = 2; k <= nfr; k++)
      AVI_write_frame(m_avi, NULL, 0, 0);
  }
}

void
xtr_avi_c::finish_file() {
  AVI_close(m_avi);
  safefree(m_bih);
  m_bih = NULL;
}

