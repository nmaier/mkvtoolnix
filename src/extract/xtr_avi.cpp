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
  avi(NULL), fps(0.0) {
}

void
xtr_avi_c::create_file(xtr_base_c *_master,
                       KaxTrackEntry &track) {
  const alBITMAPINFOHEADER *bih;
  char ccodec[5];
  string writing_app;
  KaxCodecPrivate *priv;

  priv = FINDFIRST(&track, KaxCodecPrivate);
  if (NULL == priv)
    mxerror("Track %lld with the CodecID '%s' is missing the \"codec private"
            "\" element and cannot be extracted.\n", tid, codec_id.c_str());

  default_duration = kt_get_default_duration(track);
  if (0 >= default_duration)
    mxerror("Track %lld with the CodecID '%s' is missing the \"default "
            "duration\" element and cannot be extracted.\n", tid,
            codec_id.c_str());
  fps = (double)1000000000.0 / (double)default_duration;

  if (NULL != _master)
    mxerror("Cannot write track %lld with the CodecID '%s' to the file '%s' "
            "because track %lld with the CodecID '%s' is already being "
            "written to the same file.\n", tid, codec_id.c_str(),
            file_name.c_str(), _master->tid, _master->codec_id.c_str());

  avi = AVI_open_output_file(file_name.c_str());
  if (NULL == avi)
    mxerror("The file '%s' could not be opened for writing (%s).\n",
            file_name.c_str(), AVI_strerror());
  writing_app = "mkvextract";
  if (!no_variable_data)
    writing_app += mxsprintf(" %s", VERSION);
  avi->writing_app = safestrdup(writing_app.c_str());

  bih = (const alBITMAPINFOHEADER *)priv->GetBuffer();
  memcpy(ccodec, &bih->bi_compression, 4);
  ccodec[4] = 0;
  AVI_set_video(avi, kt_get_v_pixel_width(track), kt_get_v_pixel_height(track),
                fps, ccodec);
}

void
xtr_avi_c::handle_block(KaxBlock &block,
                        KaxBlockAdditions *additions,
                        int64_t timecode,
                        int64_t duration,
                        int64_t bref,
                        int64_t fref) {
  int i;

  if (0 >= duration)
    duration = default_duration;

  for (i = 0; i < block.NumberFrames(); i++) {
    DataBuffer &data = block.GetBuffer(i);

    AVI_write_frame(avi, (char *)data.Buffer(), data.Size(),
                    bref != 0 ? 0 : 1);
    if (((double)duration / 1000000.0 - (1000.0 / fps)) >= 1.5) {
      int k, nfr;

      nfr = irnd((double)duration / 1000000.0 * fps / 1000.0);
      for (k = 2; k <= nfr; k++)
        AVI_write_frame(avi, "", 0, 0);
    }
  }
}

void
xtr_avi_c::finish_file() {
  AVI_close(avi);
}

