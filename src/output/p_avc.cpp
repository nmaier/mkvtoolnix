/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   MPEG 4 part 10 ES video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "hacks.h"
#include "matroska.h"
#include "mpeg4_common.h"
#include "output_control.h"
#include "p_avc.h"

using namespace libmatroska;
using namespace mpeg4::p10;

mpeg4_p10_es_video_packetizer_c::
mpeg4_p10_es_video_packetizer_c(generic_reader_c *n_reader,
                                memory_cptr avcc,
                                int width,
                                int height,
                                track_info_c &n_ti):
  generic_packetizer_c(n_reader, n_ti),
  m_avcc(avcc),
  m_width(width),
  m_height(height),
  m_allow_timecode_generation(true),
  m_first_frame(true) {

  relaxed_timecode_checking = true;

  if (get_cue_creation() == CUE_STRATEGY_UNSPECIFIED)
    set_cue_creation(CUE_STRATEGY_IFRAMES);

  set_track_type(track_video);

  set_codec_id(MKV_V_MPEG4_AVC);

  set_codec_private(m_avcc->get(), m_avcc->get_size());
  extract_aspect_ratio();
}

void
mpeg4_p10_es_video_packetizer_c::set_headers() {
  if (m_allow_timecode_generation) {
    if (-1 == htrack_default_duration)
      htrack_default_duration = 40000000;
    m_parser.enable_timecode_generation(htrack_default_duration);
  }

  set_video_pixel_width(m_width);
  set_video_pixel_height(m_height);

  generic_packetizer_c::set_headers();

  track_entry->EnableLacing(false);
}

int
mpeg4_p10_es_video_packetizer_c::process(packet_cptr packet) {
  try {
    if (!m_allow_timecode_generation)
      m_parser.add_timecode(packet->timecode);
    m_parser.add_bytes(packet->data->get(), packet->data->get_size());
    flush_frames();

  } catch (error_c &error) {
    mxerror(FMT_TID "mkvmerge encountered broken or unparsable data in  "
            "this AVC/h.264 video track.\n"
            "Either your file is damaged (which mkvmerge cannot cope with "
            "yet)\n"
            "or this is a bug in mkvmerge itself. The error message was:\n"
            "%s\n",
            ti.fname.c_str(), (int64_t)ti.id, error.get_error().c_str());
  }

  return FILE_STATUS_MOREDATA;
}

void
mpeg4_p10_es_video_packetizer_c::extract_aspect_ratio() {
  uint32_t num, den;
  unsigned char *priv;

  priv = ti.private_data;
  if (mpeg4::p10::extract_par(ti.private_data, ti.private_size, num, den) &&
      (0 != num) && (0 != den)) {
    if (!ti.aspect_ratio_given && !ti.display_dimensions_given) {
      double par = (double)num / (double)den;

      if (par >= 1) {
        ti.display_width = irnd(m_width * par);
        ti.display_height = m_height;

      } else {
        ti.display_width = m_width;
        ti.display_height = irnd(m_height / par);

      }

      ti.display_dimensions_given = true;
      mxinfo("Track " LLD " of '%s': Extracted the aspect ratio information "
             "from the MPEG-4 layer 10 (AVC) video data and set the display "
             "dimensions to %u/%u.\n", (int64_t)ti.id, ti.fname.c_str(),
             (uint32_t)ti.display_width, (uint32_t)ti.display_height);
    }

    set_codec_private(ti.private_data, ti.private_size);
  }

  if (priv != ti.private_data)
    safefree(priv);
}

void
mpeg4_p10_es_video_packetizer_c::flush() {
  m_parser.flush();
  flush_frames();
  generic_packetizer_c::flush();
}

void
mpeg4_p10_es_video_packetizer_c::flush_frames() {
  while (m_parser.frame_available()) {
    avc_frame_t frame(m_parser.get_frame());
    if (m_first_frame && !frame.m_keyframe)
      mxerror(FMT_TID "This AVC/h.264 track does not start with a key frame. "
              "Such files are not supported by mkvmerge.\n",
              ti.fname.c_str(), (int64_t)ti.id);
    add_packet(new packet_t(frame.m_data, frame.m_start,
                            frame.m_end > frame.m_start ?
                            frame.m_end - frame.m_start :
                            htrack_default_duration,
                            frame.m_keyframe ? -1 :
                            frame.m_start + frame.m_ref1));
    m_first_frame = false;
  }
}

void
mpeg4_p10_es_video_packetizer_c::
enable_timecode_generation(bool enable,
                           int64_t default_duration) {
  m_allow_timecode_generation = enable;
  if (enable) {
    m_parser.enable_timecode_generation(default_duration);
    set_track_default_duration(default_duration);
  }
}

void
mpeg4_p10_es_video_packetizer_c::dump_debug_info() {
}

connection_result_e
mpeg4_p10_es_video_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                                string &error_message) {
  mpeg4_p10_es_video_packetizer_c *vsrc;

  vsrc = dynamic_cast<mpeg4_p10_es_video_packetizer_c *>(src);
  if (vsrc == NULL)
    return CAN_CONNECT_NO_FORMAT;
  connect_check_v_width(m_width, vsrc->m_width);
  connect_check_v_height(m_height, vsrc->m_height);
  connect_check_codec_id(hcodec_id, vsrc->hcodec_id);

  if (((ti.private_data == NULL) && (vsrc->ti.private_data != NULL)) ||
      ((ti.private_data != NULL) && (vsrc->ti.private_data == NULL)) ||
      (ti.private_size != vsrc->ti.private_size)) {
    error_message = mxsprintf("The codec's private data does not match "
                              "(lengths: %d and %d).", ti.private_size,
                              vsrc->ti.private_size);
    return CAN_CONNECT_MAYBE_CODECPRIVATE;
  }
  return CAN_CONNECT_YES;
}

