/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include "common/common.h"
#include "common/endian.h"
#include "common/hacks.h"
#include "common/math.h"
#include "common/matroska.h"
#include "merge/output_control.h"
#include "output/p_video.h"

extern "C" {
#include <avilib.h>
}

using namespace libmatroska;

video_packetizer_c::video_packetizer_c(generic_reader_c *p_reader,
                                       track_info_c &p_ti,
                                       const char *codec_id,
                                       double fps,
                                       int width,
                                       int height)
  : generic_packetizer_c(p_reader, p_ti)
  , m_fps(fps)
  , m_width(width)
  , m_height(height)
  , m_frames_output(0)
  , m_ref_timecode(-1)
  , m_duration_shift(0) {

  if (get_cue_creation() == CUE_STRATEGY_UNSPECIFIED)
    set_cue_creation(CUE_STRATEGY_IFRAMES);

  set_track_type(track_video);

  if (NULL != codec_id)
    set_codec_id(codec_id);
  else
    set_codec_id(MKV_V_MSCOMP);

  set_codec_private(ti.private_data, ti.private_size);
  check_fourcc();
}

void
video_packetizer_c::check_fourcc() {
  if (   (hcodec_id == MKV_V_MSCOMP)
      && (NULL != ti.private_data)
      && (sizeof(alBITMAPINFOHEADER) <= ti.private_size)
      && !ti.fourcc.empty()) {

    memcpy(&((alBITMAPINFOHEADER *)ti.private_data)->bi_compression, ti.fourcc.c_str(), 4);
    set_codec_private(ti.private_data, ti.private_size);
  }
}

void
video_packetizer_c::set_headers() {
  if (0.0 < m_fps)
    set_track_default_duration((int64_t)(1000000000.0 / m_fps));

  set_video_pixel_width(m_width);
  set_video_pixel_height(m_height);

  generic_packetizer_c::set_headers();

  track_entry->EnableLacing(false);
}

// Semantics:
// bref == -1: I frame, no references
// bref == -2: P or B frame, use timecode of last I / P frame as the bref
// bref > 0:   P or B frame, use this bref as the backward reference
//             (absolute reference, not relative!)
// fref == 0:  P frame, no forward reference
// fref > 0:   B frame with given forward reference (absolute reference,
//             not relative!)
int
video_packetizer_c::process(packet_cptr packet) {
  if ((0.0 == m_fps) && (-1 == packet->timecode))
    mxerror_tid(ti.fname, ti.id, boost::format(Y("The FPS is 0.0 but the reader did not provide a timecode for a packet. %1%\n")) % BUGMSG);

  if (-1 == packet->timecode)
    packet->timecode = (int64_t)(1000000000.0 * m_frames_output / m_fps) + m_duration_shift;

  if (-1 == packet->duration) {
    if (0.0 == m_fps)
      packet->duration = 0;
    else
      packet->duration = (int64_t)(1000000000.0 / m_fps);

  } else if (0.0 != packet->duration)
    m_duration_shift += packet->duration - (int64_t)(1000000000.0 / m_fps);

  ++m_frames_output;

  if (VFT_IFRAME == packet->bref)
    // Add a key frame and save its timecode so that we can reference it later.
    m_ref_timecode = packet->timecode;

  else {
    // P or B frame. Use our last timecode if the bref is -2, or the provided
    // one otherwise. The forward ref is always taken from the reader.
    if (VFT_PFRAMEAUTOMATIC == packet->bref)
      packet->bref = m_ref_timecode;
    if (VFT_NOBFRAME == packet->fref)
      m_ref_timecode = packet->timecode;
  }

  add_packet(packet);

  return FILE_STATUS_MOREDATA;
}

connection_result_e
video_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                   string &error_message) {
  video_packetizer_c *vsrc = dynamic_cast<video_packetizer_c *>(src);
  if (NULL == vsrc)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_v_width(m_width, vsrc->m_width);
  connect_check_v_height(m_height, vsrc->m_height);
  connect_check_codec_id(hcodec_id, vsrc->hcodec_id);

  if (   ((NULL == ti.private_data) && (NULL != vsrc->ti.private_data))
      || ((NULL != ti.private_data) && (NULL == vsrc->ti.private_data))
      || (ti.private_size != vsrc->ti.private_size)) {
    error_message = (boost::format(Y("The codec's private data does not match (lengths: %1% and %2%).")) % ti.private_size % vsrc->ti.private_size).str();
    return CAN_CONNECT_MAYBE_CODECPRIVATE;
  }

  return CAN_CONNECT_YES;
}

