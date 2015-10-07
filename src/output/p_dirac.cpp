/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Dirac video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/codec.h"
#include "common/math.h"
#include "merge/output_control.h"
#include "output/p_dirac.h"

using namespace libmatroska;

dirac_video_packetizer_c::dirac_video_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti)
  : generic_packetizer_c(p_reader, p_ti)
  , m_headers_found(false)
  , m_previous_timecode(-1)
{
  set_track_type(track_video);
  set_codec_id(MKV_V_DIRAC);

  // Dummy values
  m_seqhdr.pixel_width  = 123;
  m_seqhdr.pixel_height = 123;
}

void
dirac_video_packetizer_c::set_headers() {
  set_video_pixel_width(m_seqhdr.pixel_width);
  set_video_pixel_height(m_seqhdr.pixel_height);

  if (m_headers_found) {
    int display_width  = m_seqhdr.pixel_width;
    int display_height = m_seqhdr.pixel_height;

    if ((0 != m_seqhdr.aspect_ratio_numerator) && (0 != m_seqhdr.aspect_ratio_denominator)) {
      if (m_seqhdr.aspect_ratio_numerator > m_seqhdr.aspect_ratio_denominator)
        display_width  = irnd(display_width  * m_seqhdr.aspect_ratio_numerator   / m_seqhdr.aspect_ratio_denominator);
      else
        display_height = irnd(display_height * m_seqhdr.aspect_ratio_denominator / m_seqhdr.aspect_ratio_numerator);
    }

    set_video_display_width(display_width);
    set_video_display_height(display_height);

    if (m_default_duration_forced)
      m_parser.set_default_duration(get_track_default_duration());
    else
      set_track_default_duration(m_parser.get_default_duration());

  } else
    set_track_default_duration(1000000000ll * 1001 / 30000);

  generic_packetizer_c::set_headers();

  m_track_entry->EnableLacing(false);
}

int
dirac_video_packetizer_c::process(packet_cptr packet) {
  if (-1 != packet->timecode)
    m_parser.add_timecode(packet->timecode);

  m_parser.add_bytes(packet->data->get_buffer(), packet->data->get_size());

  if (!m_headers_found && m_parser.are_headers_available())
    headers_found();

  flush_frames();

  return FILE_STATUS_MOREDATA;
}

void
dirac_video_packetizer_c::headers_found() {
  m_headers_found = true;

  m_parser.get_sequence_header(m_seqhdr);

  if (!m_reader->m_appending)
    set_headers();
}

void
dirac_video_packetizer_c::flush_impl() {
  m_parser.flush();
  flush_frames();
}

void
dirac_video_packetizer_c::flush_frames() {
  while (m_parser.is_frame_available()) {
    dirac::frame_cptr frame = m_parser.get_frame();

    add_packet(new packet_t(frame->data, frame->timecode, frame->duration, frame->contains_sequence_header ? -1 : m_previous_timecode));

    m_previous_timecode = frame->timecode;
  }
}

connection_result_e
dirac_video_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                         std::string &) {
  dirac_video_packetizer_c *vsrc = dynamic_cast<dirac_video_packetizer_c *>(src);
  if (!vsrc)
    return CAN_CONNECT_NO_FORMAT;

  return CAN_CONNECT_YES;
}
