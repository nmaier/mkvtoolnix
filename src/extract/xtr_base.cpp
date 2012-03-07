/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>

#include "common/ebml.h"
#include "common/matroska.h"
#include "common/mm_write_buffer_io.h"
#include "common/strings/editing.h"
#include "extract/xtr_aac.h"
#include "extract/xtr_avc.h"
#include "extract/xtr_avi.h"
#include "extract/xtr_base.h"
#include "extract/xtr_cpic.h"
#include "extract/xtr_ivf.h"
#include "extract/xtr_mpeg1_2.h"
#include "extract/xtr_ogg.h"
#include "extract/xtr_pgs.h"
#include "extract/xtr_rmff.h"
#include "extract/xtr_textsubs.h"
#include "extract/xtr_tta.h"
#include "extract/xtr_vobsub.h"
#include "extract/xtr_wav.h"

using namespace libmatroska;

xtr_base_c::xtr_base_c(const std::string &codec_id,
                       int64_t tid,
                       track_spec_t &tspec,
                       const char *container_name)
  : m_codec_id(codec_id)
  , m_file_name(tspec.out_name)
  , m_container_name(nullptr == container_name ? Y("raw data") : container_name)
  , m_master(nullptr)
  , m_tid(tid)
  , m_track_num(-1)
  , m_default_duration(0)
  , m_bytes_written(0)
  , m_content_decoder_initialized(false)
{
}

xtr_base_c::~xtr_base_c() {
}

void
xtr_base_c::create_file(xtr_base_c *master,
                        KaxTrackEntry &track) {
  if (nullptr != master)
    mxerror(boost::format(Y("Cannot write track %1% with the CodecID '%2%' to the file '%3%' because "
                            "track %4% with the CodecID '%5%' is already being written to the same file.\n"))
            % m_tid % m_codec_id % m_file_name % master->m_tid % master->m_codec_id);

  try {
    init_content_decoder(track);
    m_out = mm_write_buffer_io_c::open(m_file_name, 5 * 1024 * 1024);
  } catch(...) {
    mxerror(boost::format(Y("Failed to create the file '%1%': %2% (%3%)\n")) % m_file_name % errno % strerror(errno));
  }

  m_default_duration = kt_get_default_duration(track);
}

void
xtr_base_c::handle_frame(memory_cptr &frame,
                         KaxBlockAdditions *,
                         int64_t,
                         int64_t,
                         int64_t,
                         int64_t,
                         bool,
                         bool,
                         bool) {
  m_content_decoder.reverse(frame, CONTENT_ENCODING_SCOPE_BLOCK);
  m_out->write(frame);
  m_bytes_written += frame->get_size();
}

void
xtr_base_c::finish_track() {
}

void
xtr_base_c::finish_file() {
}

void
xtr_base_c::headers_done() {
}

memory_cptr
xtr_base_c::decode_codec_private(KaxCodecPrivate *priv) {
  memory_cptr mpriv(new memory_c(priv->GetBuffer(), priv->GetSize()));
  m_content_decoder.reverse(mpriv, CONTENT_ENCODING_SCOPE_CODECPRIVATE);

  return mpriv;
}

void
xtr_base_c::init_content_decoder(KaxTrackEntry &track) {
  if (m_content_decoder_initialized)
    return;

  if (!m_content_decoder.initialize(track))
    mxerror(Y("Tracks with unsupported content encoding schemes (compression or encryption) cannot be extracted.\n"));

  m_content_decoder_initialized = true;
}

xtr_base_c *
xtr_base_c::create_extractor(const std::string &new_codec_id,
                             int64_t new_tid,
                             track_spec_t &tspec) {
  // Raw format
  if (track_spec_t::tm_raw == tspec.target_mode)
    return new xtr_base_c(new_codec_id, new_tid, tspec);
  else if (track_spec_t::tm_full_raw == tspec.target_mode)
    return new xtr_fullraw_c(new_codec_id, new_tid, tspec);

  // Audio formats
  else if (new_codec_id == MKV_A_AC3)
    return new xtr_base_c(new_codec_id, new_tid, tspec, "Dolby Digital (AC3)");
  else if (new_codec_id == MKV_A_EAC3)
    return new xtr_base_c(new_codec_id, new_tid, tspec, "Dolby Digital Plus (EAC3)");
  else if (ba::istarts_with(new_codec_id, "A_MPEG/L"))
    return new xtr_base_c(new_codec_id, new_tid, tspec, "MPEG-1 Audio Layer 2/3");
  else if (new_codec_id == MKV_A_DTS)
    return new xtr_base_c(new_codec_id, new_tid, tspec, "Digital Theater System (DTS)");
  else if (new_codec_id == MKV_A_PCM)
    return new xtr_wav_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_A_FLAC)
    return new xtr_flac_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_A_VORBIS)
    return new xtr_oggvorbis_c(new_codec_id, new_tid, tspec);
  else if (ba::istarts_with(new_codec_id, "A_AAC"))
    return new xtr_aac_c(new_codec_id, new_tid, tspec);
  else if (ba::istarts_with(new_codec_id, "A_REAL/"))
    return new xtr_rmff_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_A_MLP)
    return new xtr_base_c(new_codec_id, new_tid, tspec, "MLP");
  else if (new_codec_id == MKV_A_TRUEHD)
    return new xtr_base_c(new_codec_id, new_tid, tspec, "TrueHD");
  else if (new_codec_id == MKV_A_TTA)
    return new xtr_tta_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_A_WAVPACK4)
    return new xtr_wavpack4_c(new_codec_id, new_tid, tspec);

  // Video formats
  else if (new_codec_id == MKV_V_MSCOMP)
    return new xtr_avi_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_V_MPEG4_AVC)
    return new xtr_avc_c(new_codec_id, new_tid, tspec);
  else if (ba::istarts_with(new_codec_id, "V_REAL/"))
    return new xtr_rmff_c(new_codec_id, new_tid, tspec);
  else if ((new_codec_id == MKV_V_MPEG1) || (new_codec_id == MKV_V_MPEG2))
    return new xtr_mpeg1_2_video_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_V_THEORA)
    return new xtr_oggtheora_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_V_VP8)
    return new xtr_ivf_c(new_codec_id, new_tid, tspec);

  // Subtitle formats
  else if ((new_codec_id == MKV_S_TEXTUTF8) || (new_codec_id == MKV_S_TEXTASCII))
    return new xtr_srt_c(new_codec_id, new_tid, tspec);
  else if ((new_codec_id == MKV_S_TEXTSSA) || (new_codec_id == MKV_S_TEXTASS) || (new_codec_id == "S_SSA") || (new_codec_id == "S_ASS"))
    return new xtr_ssa_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_S_VOBSUB)
    return new xtr_vobsub_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_S_TEXTUSF)
    return new xtr_usf_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_V_COREPICTURE)
    return new xtr_cpic_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_S_KATE)
    return new xtr_oggkate_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_S_HDMV_PGS)
    return new xtr_pgs_c(new_codec_id, new_tid, tspec);

  return nullptr;
}

void
xtr_fullraw_c::create_file(xtr_base_c *master,
                           KaxTrackEntry &track) {
  xtr_base_c::create_file(master, track);

  KaxCodecPrivate *priv = FINDFIRST(&track, KaxCodecPrivate);

  if ((nullptr != priv) && (0 != priv->GetSize())) {
    memory_cptr mem(new memory_c(priv->GetBuffer(), priv->GetSize(), false));
    m_content_decoder.reverse(mem, CONTENT_ENCODING_SCOPE_CODECPRIVATE);
    m_out->write(mem);
  }
}

void
xtr_fullraw_c::handle_codec_state(memory_cptr &codec_state) {
  m_content_decoder.reverse(codec_state, CONTENT_ENCODING_SCOPE_CODECPRIVATE);
  m_out->write(codec_state);
}
