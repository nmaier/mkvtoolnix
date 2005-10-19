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
                       track_spec_t &tspec):
  codec_id(_codec_id), file_name(tspec.out_name), master(NULL), out(NULL),
  tid(_tid), default_duration(0), bytes_written(0) {
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
    out = new mm_file_io_c(file_name, MODE_CREATE);
  } catch(...) {
    mxerror("Failed to create the file '%s': %d (%s)\n", file_name.c_str(),
            errno, strerror(errno));
  }

  default_duration = kt_get_default_duration(track);
}

void
xtr_base_c::handle_block_v1(KaxBlock &block,
                            KaxBlockAdditions *additions,
                            int64_t timecode,
                            int64_t duration,
                            int64_t bref,
                            int64_t fref) {
  int i;

  if (0 == block.NumberFrames())
    return;

  if (0 > duration)
    duration = default_duration * block.NumberFrames();

  for (i = 0; i < block.NumberFrames(); i++) {
    int64_t this_timecode, this_duration;

    if (0 > duration) {
      this_timecode = timecode;
      this_duration = duration;
    } else {
      this_timecode = timecode + i * duration / block.NumberFrames();
      this_duration = duration / block.NumberFrames();
    }

    DataBuffer &data = block.GetBuffer(i);
    memory_cptr frame(new memory_c(data.Buffer(), data.Size(), false));
    handle_frame(frame, additions, this_timecode, this_duration, bref, fref,
                 false, false, true);
  }
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
  else if ((new_codec_id == MKV_A_AC3) ||
           starts_with_case(new_codec_id, "A_MPEG/L") ||
           (new_codec_id == MKV_A_DTS))
    return new xtr_base_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_A_PCM)
    return new xtr_wav_c(new_codec_id, new_tid, tspec);
  else if (new_codec_id == MKV_A_FLAC) {
    if (tspec.embed_in_ogg)
      return new xtr_oggflac_c(new_codec_id, new_tid, tspec);
    else
      return new xtr_flac_c(new_codec_id, new_tid, tspec);
  } else if (new_codec_id == MKV_A_VORBIS)
    return new xtr_oggvorbis_c(new_codec_id, new_tid, tspec);
  else if (starts_with_case(new_codec_id, "A_AAC/"))
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

  return NULL;
}

void
xtr_fullraw_c::create_file(xtr_base_c *_master,
                           KaxTrackEntry &track) {
  KaxCodecPrivate *priv;

  xtr_base_c::create_file(_master, track);

  priv = FINDFIRST(&track, KaxCodecPrivate);

  if ((NULL != priv) && (0 != priv->GetSize()))
    out->write(priv->GetBuffer(), priv->GetSize());
}
