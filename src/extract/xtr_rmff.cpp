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

#include "xtr_rmff.h"

xtr_rmff_c::xtr_rmff_c(const string &_codec_id,
                       int64_t _tid,
                       track_spec_t &tspec):
  xtr_base_c(_codec_id, _tid, tspec),
  file(NULL), rmtrack(NULL) {
}

void
xtr_rmff_c::create_file(xtr_base_c *_master,
                        KaxTrackEntry &track) {
  KaxCodecPrivate *priv;

  priv = FINDFIRST(&track, KaxCodecPrivate);
  if (NULL == priv)
    mxerror("Track " LLD " with the CodecID '%s' is missing the \"codec "
            "private\" element and cannot be extracted.\n", tid,
            codec_id.c_str());

  init_content_decoder(track);
  memory_cptr mpriv = decode_codec_private(priv);

  master = _master;
  if (NULL == master) {
    file = rmff_open_file(file_name.c_str(), RMFF_OPEN_MODE_WRITING);
    if (file == NULL)
      mxerror("The file '%s' could not be opened for writing (%d, %s).\n",
              file_name.c_str(), errno, strerror(errno));
  } else
    file = static_cast<xtr_rmff_c *>(master)->file;

  rmtrack = rmff_add_track(file, 1);
  if (rmtrack == NULL)
    mxerror("Memory allocation error: %d (%s).\n",
            rmff_last_error, rmff_last_error_msg);

  rmff_set_type_specific_data(rmtrack, mpriv->get(), mpriv->get_size());

  if ('V' == codec_id[0])
    rmff_set_track_data(rmtrack, "Video", "video/x-pn-realvideo");
  else
    rmff_set_track_data(rmtrack, "Audio", "audio/x-pn-realaudio");
}

void
xtr_rmff_c::handle_frame(memory_cptr &frame,
                         KaxBlockAdditions *additions,
                         int64_t timecode,
                         int64_t duration,
                         int64_t bref,
                         int64_t fref,
                         bool keyframe,
                         bool discardable,
                         bool references_valid) {
  rmff_frame_t *rmff_frame;

  content_decoder.reverse(frame, CONTENT_ENCODING_SCOPE_BLOCK);

  rmff_frame = rmff_allocate_frame(frame->get_size(), frame->get());
  if (rmff_frame == NULL)
    mxerror("Memory for a RealAudio/RealVideo frame could not be "
            "allocated.\n");
  rmff_frame->timecode = timecode / 1000000;
  if (references_valid)
    keyframe = 0 == bref;
  if (keyframe)
    rmff_frame->flags = RMFF_FRAME_FLAG_KEYFRAME;
  if ('V' == codec_id[0])
    rmff_write_packed_video_frame(rmtrack, rmff_frame);
  else
    rmff_write_frame(rmtrack, rmff_frame);
  rmff_release_frame(rmff_frame);
}

void
xtr_rmff_c::finish_file() {
  if ((NULL == master) && (NULL != file)) {
    rmff_write_index(file);
    rmff_fix_headers(file);
    rmff_close_file(file);
  }
}

void
xtr_rmff_c::headers_done() {
  if (NULL == master) {
    file->cont_header_present = 1;
    rmff_write_headers(file);
  }
}
