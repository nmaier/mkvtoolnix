/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/debugging.h"
#include "common/endian.h"
#include "common/hacks.h"
#include "common/math.h"
#include "common/matroska.h"
#include "common/mpeg4_p2.h"
#include "merge/output_control.h"
#include "output/p_video.h"

#include <avilib.h>

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
  , m_duration_shift(0)
  , m_rederive_frame_types(debugging_requested("rederive_frame_types"))
  , m_codec_type(video_packetizer_c::ct_unknown)
{
  if (get_cue_creation() == CUE_STRATEGY_UNSPECIFIED)
    set_cue_creation(CUE_STRATEGY_IFRAMES);

  set_track_type(track_video);

  if (NULL != codec_id)
    set_codec_id(codec_id);
  else
    set_codec_id(MKV_V_MSCOMP);

  set_codec_private(m_ti.m_private_data, m_ti.m_private_size);
  check_fourcc();
}

void
video_packetizer_c::check_fourcc() {
  if (   (m_hcodec_id                != MKV_V_MSCOMP)
      || (NULL                       == m_ti.m_private_data)
      || (sizeof(alBITMAPINFOHEADER) >  m_ti.m_private_size))
    return;

  if (!m_ti.m_fourcc.empty()) {
    memcpy(&((alBITMAPINFOHEADER *)m_ti.m_private_data)->bi_compression, m_ti.m_fourcc.c_str(), 4);
    set_codec_private(m_ti.m_private_data, m_ti.m_private_size);
  }

  char fourcc[5];
  memcpy(fourcc, &((alBITMAPINFOHEADER *)m_ti.m_private_data)->bi_compression, 4);
  fourcc[4] = 0;

  if (mpeg4::p2::is_v3_fourcc(fourcc))
    m_codec_type = video_packetizer_c::ct_div3;

  else if (mpeg4::p2::is_fourcc(fourcc))
    m_codec_type = video_packetizer_c::ct_mpeg4_p2;
}

void
video_packetizer_c::set_headers() {
  if (0.0 < m_fps)
    set_track_default_duration((int64_t)(1000000000.0 / m_fps));

  set_video_pixel_width(m_width);
  set_video_pixel_height(m_height);

  generic_packetizer_c::set_headers();

  m_track_entry->EnableLacing(false);
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
    mxerror_tid(m_ti.m_fname, m_ti.m_id, boost::format(Y("The FPS is 0.0 but the reader did not provide a timecode for a packet. %1%\n")) % BUGMSG);

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

  if (m_rederive_frame_types)
    rederive_frame_type(packet);

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

void
video_packetizer_c::rederive_frame_type(packet_cptr &packet) {
  switch (m_codec_type) {
    case video_packetizer_c::ct_div3:
      rederive_frame_type_div3(packet);
      break;

    case video_packetizer_c::ct_mpeg4_p2:
      rederive_frame_type_mpeg4_p2(packet);
      break;

    default:
      break;
  }
}

void
video_packetizer_c::rederive_frame_type_div3(packet_cptr &packet) {
  if (1 >= packet->data->get_size())
    return;

  packet->bref = packet->data->get_buffer()[0] & 0x40 ? VFT_PFRAMEAUTOMATIC : VFT_IFRAME;
  packet->fref = VFT_NOBFRAME;
}

void
video_packetizer_c::rederive_frame_type_mpeg4_p2(packet_cptr &packet) {
  size_t idx, size    = packet->data->get_size();
  unsigned char *data = packet->data->get_buffer();

  for (idx = 0; idx < size - 5; ++idx) {
    if ((0x00 == data[idx]) && (0x00 == data[idx + 1]) && (0x01 == data[idx + 2])) {
      if ((0 == data[idx + 3]) || (0xb0 == data[idx + 3])) {
        packet->bref = VFT_IFRAME;
        packet->fref = VFT_NOBFRAME;
        return;
      }

      if (0xb6 == data[idx + 3]) {
        packet->bref = 0x00 == (data[idx + 4] & 0xc0) ? VFT_IFRAME : VFT_PFRAMEAUTOMATIC;
        packet->fref = VFT_NOBFRAME;
        return;
      }

      idx += 2;
    }
  }
}

connection_result_e
video_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                   std::string &error_message) {
  video_packetizer_c *vsrc = dynamic_cast<video_packetizer_c *>(src);
  if (NULL == vsrc)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_v_width(m_width,      vsrc->m_width);
  connect_check_v_height(m_height,    vsrc->m_height);
  connect_check_codec_id(m_hcodec_id, vsrc->m_hcodec_id);

  if (   ((NULL == m_ti.m_private_data) && (NULL != vsrc->m_ti.m_private_data))
      || ((NULL != m_ti.m_private_data) && (NULL == vsrc->m_ti.m_private_data))
      || (m_ti.m_private_size != vsrc->m_ti.m_private_size)) {
    error_message = (boost::format(Y("The codec's private data does not match (lengths: %1% and %2%).")) % m_ti.m_private_size % vsrc->m_ti.m_private_size).str();
    return CAN_CONNECT_MAYBE_CODECPRIVATE;
  }

  return CAN_CONNECT_YES;
}

