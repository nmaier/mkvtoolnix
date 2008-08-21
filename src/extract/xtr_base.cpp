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

#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>

#include "common.h"
#include "commonebml.h"
#include "matroska.h"

#include "xtr_aac.h"
#include "xtr_avc.h"
#include "xtr_avi.h"
#include "xtr_base.h"
#include "xtr_cpic.h"
#include "xtr_mpeg1_2.h"
#include "xtr_ogg.h"
#include "xtr_rmff.h"
#include "xtr_textsubs.h"
#include "xtr_tta.h"
#include "xtr_vobsub.h"
#include "xtr_wav.h"

using namespace std;
using namespace libmatroska;

xtr_base_c::xtr_base_c(const string &_codec_id,
                       int64_t _tid,
                       track_spec_t &tspec,
                       const char *_container_name):
  codec_id(_codec_id), file_name(tspec.out_name),
  container_name(NULL == _container_name ? "raw data" : _container_name),
  master(NULL), out(NULL), tid(_tid), default_duration(0), bytes_written(0),
  content_decoder_initialized(false) {
}

xtr_base_c::~xtr_base_c() {
  if (NULL == master)
    delete out;
}

void
xtr_base_c::create_file(xtr_base_c *_master,
                        KaxTrackEntry &track) {
  if (NULL != _master)
    mxerror("Cannot write track " LLD " with the CodecID '%s' to the file "
            "'%s' because track " LLD " with the CodecID '%s' is already "
            "being written to the same file.\n", tid, codec_id.c_str(),
            file_name.c_str(), _master->tid, _master->codec_id.c_str());

  try {
    init_content_decoder(track);
    out = new mm_file_io_c(file_name, MODE_CREATE);
  } catch(...) {
    mxerror("Failed to create the file '%s': %d (%s)\n", file_name.c_str(),
            errno, strerror(errno));
  }

  default_duration = kt_get_default_duration(track);
}

void
xtr_base_c::handle_frame(memory_cptr &frame,
                         KaxBlockAdditions *additions,
                         int64_t timecode,
                         int64_t duration,
                         int64_t bref,
                         int64_t fref,
                         bool keyframe,
                         bool discardable,
                         bool references_valid) {
  content_decoder.reverse(frame, CONTENT_ENCODING_SCOPE_BLOCK);
  out->write(frame->get(), frame->get_size());
  bytes_written += frame->get_size();
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
  content_decoder.reverse(mpriv, CONTENT_ENCODING_SCOPE_CODECPRIVATE);

  return mpriv;
}

void
xtr_base_c::init_content_decoder(KaxTrackEntry &track) {
  if (content_decoder_initialized)
    return;

  if (!content_decoder.initialize(track))
    mxerror("Tracks with unsupported content encoding schemes (compression "
            "or encryption) cannot be extracted.\n");

  content_decoder_initialized = true;
}

xtr_base_c *
xtr_base_c::create_extractor(const string &new_codec_id,
                             int64_t new_tid,
                             track_spec_t &tspec) {
  // Raw format
  if (1 == tspec.extract_raw)
    return new xtr_base_c(new_codec_id, new_tid, tspec);
  else if (2 == tspec.extract_raw)
    return new xtr_fullraw_c(new_codec_id, new_tid, tspec);
  // Audio formats
  else if (new_codec_id == MKV_A_AC3)
    return new xtr_base_c(new_codec_id, new_tid, tspec, "Dolby Digital (AC3)");
  else if (new_codec_id == MKV_A_EAC3)
    return new xtr_base_c(new_codec_id, new_tid, tspec, "Dolby Digital Plus (EAC3)");
  else if (starts_with_case(new_codec_id, "A_MPEG/L"))
    return new xtr_base_c(new_codec_id, new_tid, tspec,
                          "MPEG-1 Audio Layer 2/3");
  else if (new_codec_id == MKV_A_DTS)
    return new xtr_base_c(new_codec_id, new_tid, tspec,
                          "Digital Theater System (DTS)");
  else if (new_codec_id == MKV_A_PCM)
    return new xtr_wav_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_A_FLAC) {
    if (tspec.embed_in_ogg)
      return new xtr_oggflac_c(new_codec_id, new_tid, tspec);
    else
      return new xtr_flac_c(new_codec_id, new_tid, tspec);
  } else if (new_codec_id == MKV_A_VORBIS)
    return new xtr_oggvorbis_c(new_codec_id, new_tid, tspec);
  else if (starts_with_case(new_codec_id, "A_AAC"))
    return new xtr_aac_c(new_codec_id, new_tid, tspec);
  else if (starts_with_case(new_codec_id, "A_REAL/"))
    return new xtr_rmff_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_A_TTA)
    return new xtr_tta_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_A_WAVPACK4)
    return new xtr_wavpack4_c(new_codec_id, new_tid, tspec);

  // Video formats
  else if (new_codec_id == MKV_V_MSCOMP)
    return new xtr_avi_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_V_MPEG4_AVC)
    return new xtr_avc_c(new_codec_id, new_tid, tspec);
  else if (starts_with_case(new_codec_id, "V_REAL/"))
    return new xtr_rmff_c(new_codec_id, new_tid, tspec);
  else if ((new_codec_id == MKV_V_MPEG1) ||
           (new_codec_id == MKV_V_MPEG2))
    return new xtr_mpeg1_2_video_c(new_codec_id, new_tid, tspec);

  // Subtitle formats
  else if ((new_codec_id == MKV_S_TEXTUTF8) ||
           (new_codec_id == MKV_S_TEXTASCII))
    return new xtr_srt_c(new_codec_id, new_tid, tspec);
  else if ((new_codec_id == MKV_S_TEXTSSA) ||
           (new_codec_id == MKV_S_TEXTASS) ||
           (new_codec_id == "S_SSA") ||
           (new_codec_id == "S_ASS"))
    return new xtr_ssa_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_S_VOBSUB)
    return new xtr_vobsub_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_S_TEXTUSF)
    return new xtr_usf_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_V_COREPICTURE)
    return new xtr_cpic_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_S_KATE)
    return new xtr_oggkate_c(new_codec_id, new_tid, tspec);

  return NULL;
}

void
xtr_fullraw_c::create_file(xtr_base_c *_master,
                           KaxTrackEntry &track) {
  KaxCodecPrivate *priv;

  xtr_base_c::create_file(_master, track);

  priv = FINDFIRST(&track, KaxCodecPrivate);

  if ((NULL != priv) && (0 != priv->GetSize())) {
    memory_cptr mem(new memory_c(priv->GetBuffer(), priv->GetSize(), false));
    content_decoder.reverse(mem, CONTENT_ENCODING_SCOPE_CODECPRIVATE);
    out->write(mem->get(), mem->get_size());
  }
}

void
xtr_fullraw_c::handle_codec_state(memory_cptr &codec_state) {
  content_decoder.reverse(codec_state, CONTENT_ENCODING_SCOPE_CODECPRIVATE);
  out->write(codec_state->get(), codec_state->get_size());
}
