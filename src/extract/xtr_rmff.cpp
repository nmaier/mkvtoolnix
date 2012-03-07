/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/ebml.h"
#include "extract/xtr_rmff.h"

xtr_rmff_c::xtr_rmff_c(const std::string &codec_id,
                       int64_t tid,
                       track_spec_t &tspec)
  : xtr_base_c(codec_id, tid, tspec)
  , m_file(nullptr)
  , m_rmtrack(nullptr) {
}

void
xtr_rmff_c::create_file(xtr_base_c *master,
                        KaxTrackEntry &track) {
  KaxCodecPrivate *priv = FINDFIRST(&track, KaxCodecPrivate);
  if (nullptr == priv)
    mxerror(boost::format(Y("Track %1% with the CodecID '%2%' is missing the \"codec private\" element and cannot be extracted.\n")) % m_tid % m_codec_id);

  init_content_decoder(track);
  memory_cptr mpriv = decode_codec_private(priv);

  m_master = master;
  if (nullptr == m_master) {
    m_file = rmff_open_file(m_file_name.c_str(), RMFF_OPEN_MODE_WRITING);
    if (nullptr == m_file)
      mxerror(boost::format(Y("The file '%1%' could not be opened for writing (%2%, %3%).\n")) % m_file_name % errno % strerror(errno));

  } else
    m_file = static_cast<xtr_rmff_c *>(m_master)->m_file;

  m_rmtrack = rmff_add_track(m_file, 1);
  if (nullptr == m_rmtrack)
    mxerror(boost::format(Y("Memory allocation error: %1% (%2%).\n")) % rmff_last_error % rmff_last_error_msg);

  rmff_set_type_specific_data(m_rmtrack, mpriv->get_buffer(), mpriv->get_size());

  if ('V' == m_codec_id[0])
    rmff_set_track_data(m_rmtrack, "Video", "video/x-pn-realvideo");
  else
    rmff_set_track_data(m_rmtrack, "Audio", "audio/x-pn-realaudio");
}

void
xtr_rmff_c::handle_frame(memory_cptr &frame,
                         KaxBlockAdditions *,
                         int64_t timecode,
                         int64_t,
                         int64_t bref,
                         int64_t,
                         bool keyframe,
                         bool,
                         bool references_valid) {
  m_content_decoder.reverse(frame, CONTENT_ENCODING_SCOPE_BLOCK);

  if (references_valid)
    keyframe = (0 == bref);

  rmff_frame_t *rmff_frame = rmff_allocate_frame(frame->get_size(), frame->get_buffer());
  if (nullptr == rmff_frame)
    mxerror(Y("Memory for a RealAudio/RealVideo frame could not be allocated.\n"));

  rmff_frame->timecode = timecode / 1000000;
  if (keyframe)
    rmff_frame->flags  = RMFF_FRAME_FLAG_KEYFRAME;

  if ('V' == m_codec_id[0])
    rmff_write_packed_video_frame(m_rmtrack, rmff_frame);
  else
    rmff_write_frame(m_rmtrack, rmff_frame);

  rmff_release_frame(rmff_frame);
}

void
xtr_rmff_c::finish_file() {
  if ((nullptr == m_master) && (nullptr != m_file)) {
    rmff_write_index(m_file);
    rmff_fix_headers(m_file);
    rmff_close_file(m_file);
  }
}

void
xtr_rmff_c::headers_done() {
  if (nullptr == m_master) {
    m_file->cont_header_present = 1;
    rmff_write_headers(m_file);
  }
}
