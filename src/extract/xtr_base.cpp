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
#include "xtr_avi.h"
#include "xtr_base.h"
#include "xtr_ogg.h"
#include "xtr_rmff.h"
#include "xtr_textsubs.h"
#include "xtr_tta.h"
#include "xtr_wav.h"

using namespace std;
using namespace libmatroska;

int64_t
kt_get_default_duration(KaxTrackEntry &track) {
  KaxTrackDefaultDuration *default_duration;

  default_duration = FINDFIRST(&track, KaxTrackDefaultDuration);
  if (NULL == default_duration)
    return 0;
  return uint64(*default_duration);
}

int64_t
kt_get_number(KaxTrackEntry &track) {
  KaxTrackNumber *number;

  number = FINDFIRST(&track, KaxTrackNumber);
  if (NULL == number)
    return 0;
  return uint64(*number);
}

string
kt_get_codec_id(KaxTrackEntry &track) {
  KaxCodecID *codec_id;

  codec_id = FINDFIRST(&track, KaxCodecID);
  if (NULL == codec_id)
    return "";
  return string(*codec_id);
}

int
kt_get_max_blockadd_id(KaxTrackEntry &track) {
  KaxMaxBlockAdditionID *max_blockadd_id;

  max_blockadd_id = FINDFIRST(&track, KaxMaxBlockAdditionID);
  if (NULL == max_blockadd_id)
    return 0;
  return uint32(*max_blockadd_id);
}

int
kt_get_a_channels(KaxTrackEntry &track) {
  KaxTrackAudio *audio;
  KaxAudioChannels *channels;

  audio = FINDFIRST(&track, KaxTrackAudio);
  if (NULL == audio)
    return 1;

  channels = FINDFIRST(audio, KaxAudioChannels);
  if (NULL == channels)
    return 1;

  return uint32(*channels);
}

float
kt_get_a_sfreq(KaxTrackEntry &track) {
  KaxTrackAudio *audio;
  KaxAudioSamplingFreq *sfreq;

  audio = FINDFIRST(&track, KaxTrackAudio);
  if (NULL == audio)
    return 8000.0;

  sfreq = FINDFIRST(audio, KaxAudioSamplingFreq);
  if (NULL == sfreq)
    return 8000.0;

  return float(*sfreq);
}

float
kt_get_a_osfreq(KaxTrackEntry &track) {
  KaxTrackAudio *audio;
  KaxAudioOutputSamplingFreq *osfreq;

  audio = FINDFIRST(&track, KaxTrackAudio);
  if (NULL == audio)
    return 8000.0;

  osfreq = FINDFIRST(audio, KaxAudioOutputSamplingFreq);
  if (NULL == osfreq)
    return 8000.0;

  return float(*osfreq);
}

int
kt_get_a_bps(KaxTrackEntry &track) {
  KaxTrackAudio *audio;
  KaxAudioBitDepth *bps;

  audio = FINDFIRST(&track, KaxTrackAudio);
  if (NULL == audio)
    return -1;

  bps = FINDFIRST(audio, KaxAudioBitDepth);
  if (NULL == bps)
    return -1;

  return uint32(*bps);
}

int
kt_get_v_pixel_width(KaxTrackEntry &track) {
  KaxTrackVideo *video;
  KaxVideoPixelWidth *width;

  video = FINDFIRST(&track, KaxTrackVideo);
  if (NULL == video)
    return 0;

  width = FINDFIRST(video, KaxVideoPixelWidth);
  if (NULL == width)
    return 0;

  return uint32(*width);
}

int
kt_get_v_pixel_height(KaxTrackEntry &track) {
  KaxTrackVideo *video;
  KaxVideoPixelHeight *height;

  video = FINDFIRST(&track, KaxTrackVideo);
  if (NULL == video)
    return 0;

  height = FINDFIRST(video, KaxVideoPixelHeight);
  if (NULL == height)
    return 0;

  return uint32(*height);
}

// ------------------------------------------------------------------------

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
    mxerror("Cannot write track %lld with the CodecID '%s' to the file '%s' "
            "because track %lld with the CodecID '%s' is already being "
            "written to the same file.\n", tid, codec_id.c_str(),
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
xtr_base_c::handle_block(KaxBlock &block,
                         KaxBlockAdditions *additions,
                         int64_t timecode,
                         int64_t duration,
                         int64_t bref,
                         int64_t fref) {
  int i;

  for (i = 0; i < block.NumberFrames(); i++) {
    DataBuffer &data = block.GetBuffer(i);
    out->write(data.Buffer(), data.Size());
    bytes_written += data.Size();
  }
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
  // Audio formats
  if ((new_codec_id == MKV_A_AC3) ||
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

  return NULL;
}

