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

xtr_avi_c::xtr_avi_c(const string &_codec_id,
                     int64_t _tid,
                     track_spec_t &tspec):
  xtr_base_c(_codec_id, _tid, tspec),
  avi(NULL), fps(0.0), bih(NULL) {
}

void
xtr_avi_c::create_file(xtr_base_c *_master,
                       KaxTrackEntry &track) {
  char ccodec[5];
  string writing_app;
  KaxCodecPrivate *priv;

  init_content_decoder(track);

  priv = FINDFIRST(&track, KaxCodecPrivate);
  if (NULL == priv)
    mxerror("Track " LLD " with the CodecID '%s' is missing the \"codec "
            "private\" element and cannot be extracted.\n", tid,
            codec_id.c_str());

  default_duration = kt_get_default_duration(track);
  if (0 >= default_duration)
    mxerror("Track " LLD " with the CodecID '%s' is missing the \"default "
            "duration\" element and cannot be extracted.\n", tid,
            codec_id.c_str());
  fps = (double)1000000000.0 / (double)default_duration;

  if (NULL != _master)
    mxerror("Cannot write track " LLD " with the CodecID '%s' to the file "
            "'%s' because track " LLD " with the CodecID '%s' is already being"
            " written to the same file.\n", tid, codec_id.c_str(),
            file_name.c_str(), _master->tid, _master->codec_id.c_str());

  avi = AVI_open_output_file(file_name.c_str());
  if (NULL == avi)
    mxerror("The file '%s' could not be opened for writing (%s).\n",
            file_name.c_str(), AVI_strerror());
  writing_app = "mkvextract";
  if (!no_variable_data)
    writing_app += mxsprintf(" %s", VERSION);
  avi->writing_app = safestrdup(writing_app.c_str());

  memory_cptr mpriv = decode_codec_private(priv);
  bih = (alBITMAPINFOHEADER *)safememdup(mpriv->get(), mpriv->get_size());
  memcpy(ccodec, &bih->bi_compression, 4);
  ccodec[4] = 0;
  if (get_uint32_le(&bih->bi_size) != sizeof(alBITMAPINFOHEADER)) {
    avi->extradata = bih + 1;
    avi->extradata_size = get_uint32_le(&bih->bi_size) -
      sizeof(alBITMAPINFOHEADER);
  }
  AVI_set_video(avi, kt_get_v_pixel_width(track), kt_get_v_pixel_height(track),
                fps, ccodec);
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
    keyframe = bref == 0;

  content_decoder.reverse(frame, CONTENT_ENCODING_SCOPE_BLOCK);
  AVI_write_frame(avi, (char *)frame->get(), frame->get_size(), keyframe);

  if (((double)duration / 1000000.0 - (1000.0 / fps)) >= 1.5) {
    int k, nfr;

    nfr = irnd((double)duration / 1000000.0 * fps / 1000.0);
    for (k = 2; k <= nfr; k++)
      AVI_write_frame(avi, NULL, 0, 0);
  }
}

void
xtr_avi_c::finish_file() {
  AVI_close(avi);
  safefree(bih);
  bih = NULL;
}

