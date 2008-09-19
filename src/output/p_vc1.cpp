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

#include <cstring>

#include "avilib.h"
#include "common.h"
#include "hacks.h"
#include "matroska.h"
#include "mpeg4_common.h"
#include "output_control.h"
#include "p_vc1.h"

using namespace libmatroska;

vc1_video_packetizer_c::vc1_video_packetizer_c(generic_reader_c *n_reader, track_info_c &n_ti)
  : generic_packetizer_c(n_reader, n_ti)
  , m_previous_timecode(-1)
{
  relaxed_timecode_checking = true;

  if (get_cue_creation() == CUE_STRATEGY_UNSPECIFIED)
    set_cue_creation(CUE_STRATEGY_IFRAMES);

  set_track_type(track_video);
  set_codec_id(MKV_V_MSCOMP);

  // Dummy values
  m_seqhdr.pixel_width  = 123;
  m_seqhdr.pixel_height = 123;

  m_parser.set_keep_stream_headers_in_frames(false);
  m_parser.set_keep_markers_in_frames(false);
}

void
vc1_video_packetizer_c::set_headers() {
  int priv_size           = sizeof(alBITMAPINFOHEADER) + (NULL != m_raw_headers.get() ? m_raw_headers->get_size() + 1 : 0);
  memory_cptr buf         = memory_c::alloc(priv_size);
  alBITMAPINFOHEADER *bih = (alBITMAPINFOHEADER *)buf->get();

  memset(bih, 0, priv_size);
  memcpy(&bih->bi_compression, "WVC1", 4);

  put_uint32_le(&bih->bi_size,             priv_size);
  put_uint16_le(&bih->bi_planes,           1);
  put_uint16_le(&bih->bi_bit_count,        24);
  put_uint32_le(&bih->bi_width,            m_seqhdr.pixel_width);
  put_uint32_le(&bih->bi_height,           m_seqhdr.pixel_height);
  put_uint32_le(&bih->bi_size_image,       get_uint32_le(&bih->bi_width) * get_uint32_le(&bih->bi_height) * 3);
  put_uint32_le(&bih->bi_x_pels_per_meter, 1);
  put_uint32_le(&bih->bi_y_pels_per_meter, 1);

  set_video_pixel_width(m_seqhdr.pixel_width);
  set_video_pixel_height(m_seqhdr.pixel_height);

  if (NULL != m_raw_headers.get()) {
    if (m_seqhdr.display_info_flag) {
      int display_width  = m_seqhdr.display_width;
      int display_height = m_seqhdr.display_height;

      mxverb(2, "vc1: display width %d height %d aspect_ratio_flag %d ar_num %d ar_den %d\n",
             m_seqhdr.display_width, m_seqhdr.display_height, m_seqhdr.aspect_ratio_flag, m_seqhdr.aspect_ratio_width, m_seqhdr.aspect_ratio_height);

      if (m_seqhdr.aspect_ratio_flag && ((1 != m_seqhdr.aspect_ratio_width) || (1 != m_seqhdr.aspect_ratio_height))) {
        mxwarn(FMT_TID "VC1 sample/pixel aspect ratio information is not supported yet. Please provide the author Moritz Bunkus <moritz@bunkus.org> "
               "with a sample of the file you're muxing, or at least send him the following pieces of information: "
               "pixel_width=%d pixel_height=%d display_width=%d display_height=%d aspect_ratio_width=%d aspect_ratio_height=%d\n",
               ti.fname.c_str(), (int64_t)ti.id, m_seqhdr.pixel_width, m_seqhdr.pixel_height, m_seqhdr.display_width, m_seqhdr.display_height,
               m_seqhdr.aspect_ratio_width, m_seqhdr.aspect_ratio_height);
        // TODO!
      }

      set_video_display_width(display_width);
      set_video_display_height(display_height);

    } else {
      set_video_display_width(m_seqhdr.pixel_width);
      set_video_display_height(m_seqhdr.pixel_height);
    }

    if (default_duration_forced)
      m_parser.set_default_duration(get_track_default_duration());
    else
      set_track_default_duration(m_parser.get_default_duration());

    *(unsigned char *)(bih + 1) = m_raw_headers->get_size();
    memcpy(((unsigned char *)bih) + sizeof(alBITMAPINFOHEADER) + 1, m_raw_headers->get(), m_raw_headers->get_size());

  } else
    set_track_default_duration(1000000000ll / 25);

  set_codec_private(buf->get(), buf->get_size());

  generic_packetizer_c::set_headers();

  track_entry->EnableLacing(false);
}

int
vc1_video_packetizer_c::process(packet_cptr packet) {
  if (-1 != packet->timecode)
    m_parser.add_timecode(packet->timecode);

  m_parser.add_bytes(packet->data->get(), packet->data->get_size());

  if ((NULL == m_raw_headers.get()) && m_parser.are_headers_available())
    headers_found();

  flush_frames();

  return FILE_STATUS_MOREDATA;
}

void
vc1_video_packetizer_c::headers_found() {
  m_parser.get_sequence_header(m_seqhdr);

  memory_cptr raw_seqhdr     = m_parser.get_raw_sequence_header();
  memory_cptr raw_entrypoint = m_parser.get_raw_entrypoint();

  m_raw_headers              = memory_c::alloc(raw_seqhdr->get_size() + raw_entrypoint->get_size());

  memcpy(m_raw_headers->get(),                          raw_seqhdr->get(),     raw_seqhdr->get_size());
  memcpy(m_raw_headers->get() + raw_seqhdr->get_size(), raw_entrypoint->get(), raw_entrypoint->get_size());

  set_headers();
}

void
vc1_video_packetizer_c::flush() {
  m_parser.flush();
  flush_frames();
  generic_packetizer_c::flush();
}

void
vc1_video_packetizer_c::flush_frames() {
  while (m_parser.is_frame_available()) {
    vc1::frame_t frame(m_parser.get_frame());

    add_packet(new packet_t(frame.data->clone(), frame.timecode, frame.duration, frame.type == vc1::FRAME_TYPE_I ? -1 : m_previous_timecode));

    m_previous_timecode = frame.timecode;
  }
}

connection_result_e
vc1_video_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                       string &error_message) {
  vc1_video_packetizer_c *vsrc = dynamic_cast<vc1_video_packetizer_c *>(src);
  if (vsrc == NULL)
    return CAN_CONNECT_NO_FORMAT;

  return CAN_CONNECT_YES;
}

