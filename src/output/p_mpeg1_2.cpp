/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   MPEG1/2 video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/endian.h"
#include "common/hacks.h"
#include "common/math.h"
#include "common/matroska.h"
#include "common/mpeg1_2.h"
#include "common/mpeg4_p2.h"
#include "common/strings/formatting.h"
#include "merge/output_control.h"
#include "output/p_mpeg1_2.h"

mpeg1_2_video_packetizer_c::
mpeg1_2_video_packetizer_c(generic_reader_c *p_reader,
                           track_info_c &p_ti,
                           int version,
                           double fps,
                           int width,
                           int height,
                           int dwidth,
                           int dheight,
                           bool framed)
  : video_packetizer_c(p_reader, p_ti, "V_MPEG1", fps, width, height)
  , m_framed{framed}
  , m_aspect_ratio_extracted{true}
  , m_num_removed_stuffing_bytes{}
  , m_debug_stuffing_removal{debugging_requested("mpeg1_2|mpeg1_2_stuffing_removal")}
{

  set_codec_id((boost::format("V_MPEG%1%") % version).str());
  if (!display_dimensions_or_aspect_ratio_set()) {
    if ((0 < dwidth) && (0 < dheight))
      set_video_display_dimensions(dwidth, dheight, PARAMETER_SOURCE_BITSTREAM);
    else
      m_aspect_ratio_extracted = false;
  }

  m_timecode_factory_application_mode = TFA_SHORT_QUEUEING;

  // m_parser.SeparateSequenceHeaders();
}

mpeg1_2_video_packetizer_c::~mpeg1_2_video_packetizer_c() {
  mxdebug_if(m_debug_stuffing_removal, boost::format("Total number of stuffing bytes removed: %1%\n") % m_num_removed_stuffing_bytes);
}

int
mpeg1_2_video_packetizer_c::process_framed(packet_cptr packet) {
  if (0 == packet->data->get_size())
    return FILE_STATUS_MOREDATA;

  if (4 > packet->data->get_size())
    return video_packetizer_c::process(packet);

  put_sequence_headers_into_codec_state(packet);
  remove_stuffing_bytes(packet->data);

  return video_packetizer_c::process(packet);
}

bool
mpeg1_2_video_packetizer_c::put_sequence_headers_into_codec_state(packet_cptr packet) {
  unsigned char *buf  = packet->data->get_buffer();
  size_t  pos         = 4;
  size_t  size        = packet->data->get_size();
  unsigned int marker = get_uint32_be(buf);

  while ((pos < size) && (MPEGVIDEO_SEQUENCE_HEADER_START_CODE != marker)) {
    marker <<= 8;
    marker  |= buf[pos];
    ++pos;
  }

  if ((MPEGVIDEO_SEQUENCE_HEADER_START_CODE != marker) || ((pos + 4) >= size))
    return false;

  size_t start  = pos - 4;
  marker        = get_uint32_be(&buf[pos]);
  pos          += 4;

  while ((pos < size) && ((MPEGVIDEO_EXT_START_CODE == marker) || (0x00000100 != (marker & 0xffffff00)))) {
    marker <<= 8;
    marker  |= buf[pos];
    ++pos;
  }

  if (pos >= size)
    return false;

  pos            -= 4;
  size_t sh_size  = pos - start;

  if (!m_hcodec_private) {
    set_codec_private(&buf[start], sh_size);
    rerender_track_headers();
  }

  if (!m_seq_hdr || (sh_size != m_seq_hdr->get_size()) || memcmp(&buf[start], m_seq_hdr->get_buffer(), sh_size)) {
    m_seq_hdr           = memory_c::clone(&buf[start], sh_size);
    packet->codec_state = memory_c::clone(&buf[start], sh_size);
  }

  if (hack_engaged(ENGAGE_USE_CODEC_STATE_ONLY)) {
    memmove(&buf[start], &buf[pos], size - pos);
    packet->data->set_size(size - sh_size);
  }

  return true;
}

void
mpeg1_2_video_packetizer_c::remove_stuffing_bytes(memory_cptr data) {
  mxdebug_if(m_debug_stuffing_removal, boost::format("Starting stuff removal, frame size %1%\n") % data->get_size());

  auto buf              = data->get_buffer();
  auto size             = data->get_size();
  size_t pos            = 4;
  size_t stuffing_start = 0;
  auto marker           = get_uint32_be(buf);
  bool in_slice         = false;
  auto num_keep         = 1;

  auto mid_remover      = [&]() {
    if (!stuffing_start || (stuffing_start >= (pos - 4)))
      return;

    auto num_stuffing_bytes       = pos - 4 - stuffing_start;
    m_num_removed_stuffing_bytes += num_stuffing_bytes;

    ::memmove(&buf[stuffing_start], &buf[pos - 4], size - pos + 4);

    pos  -= num_stuffing_bytes;
    size -= num_stuffing_bytes;

    mxdebug_if(m_debug_stuffing_removal, boost::format("    Stuffing in the middle: %1%\n") % num_stuffing_bytes);
  };

  while (pos < size) {
    if ((MPEGVIDEO_SLICE_START_CODE_LOWER <= marker) && (MPEGVIDEO_SLICE_START_CODE_UPPER >= marker)) {
      mxdebug_if(m_debug_stuffing_removal, boost::format("  Slice start code at %1%\n") % (pos - 4));

      mid_remover();

      in_slice       = true;
      stuffing_start = 0;

    } else if ((marker & 0xffffff00) == 0x00000100) {
      mxdebug_if(m_debug_stuffing_removal, boost::format("  Non-slice start code at %1%\n") % (pos - 4));

      mid_remover();

      in_slice       = false;
      stuffing_start = 0;

    } else if (in_slice && !stuffing_start && (marker == 0x00000000)) {
      mxdebug_if(m_debug_stuffing_removal, boost::format("    Start at %1%\n") % (pos - 4 + num_keep));

      stuffing_start = pos - 4 + num_keep;

    }

    marker <<= 8;
    marker  |= buf[pos];
    ++pos;
  }

  if ((marker & 0xffffff00) == 0x00000100)
    mid_remover();

  else if (stuffing_start && (stuffing_start < size)) {
    mxdebug_if(m_debug_stuffing_removal, boost::format("    Stuffing at the end: in_slice %1% stuffing_start %2% stuffing_size %3%\n") % in_slice % stuffing_start % (stuffing_start ? size - stuffing_start : 0));

    m_num_removed_stuffing_bytes += size - stuffing_start;
    size                          = stuffing_start;
  }

  data->set_size(size);
}

int
mpeg1_2_video_packetizer_c::process(packet_cptr packet) {
  if (0.0 > m_fps)
    extract_fps(packet->data->get_buffer(), packet->data->get_size());

  if (!m_aspect_ratio_extracted)
    extract_aspect_ratio(packet->data->get_buffer(), packet->data->get_size());

  return m_framed ? process_framed(packet) : process_unframed(packet);
}

int
mpeg1_2_video_packetizer_c::process_unframed(packet_cptr packet) {
  int state = m_parser.GetState();
  if ((MPV_PARSER_STATE_EOS == state) || (MPV_PARSER_STATE_ERROR == state))
    return FILE_STATUS_DONE;

  auto old_memory = packet->data;
  auto data_ptr   = old_memory->get_buffer();
  int new_bytes   = old_memory->get_size();

  if (packet->has_timecode())
    m_parser.AddTimecode(packet->timecode);

  do {
    int bytes_to_add = std::min<int>(m_parser.GetFreeBufferSpace(), new_bytes);
    if (0 < bytes_to_add) {
      m_parser.WriteData(data_ptr, bytes_to_add);
      data_ptr  += bytes_to_add;
      new_bytes -= bytes_to_add;
    }

    state = m_parser.GetState();
    while (MPV_PARSER_STATE_FRAME == state) {
      auto frame = std::shared_ptr<MPEGFrame>(m_parser.ReadFrame());
      if (!frame)
        break;

      if (!m_hcodec_private)
        create_private_data();

      packet_cptr new_packet  = packet_cptr(new packet_t(new memory_c(frame->data, frame->size, true), frame->timecode, frame->duration, frame->firstRef, frame->secondRef));
      new_packet->time_factor = MPEG2_PICTURE_TYPE_FRAME == frame->pictureStructure ? 1 : 2;

      put_sequence_headers_into_codec_state(new_packet);
      remove_stuffing_bytes(new_packet->data);

      video_packetizer_c::process(new_packet);

      frame->data = nullptr;
      state       = m_parser.GetState();
    }
  } while (0 < new_bytes);

  return FILE_STATUS_MOREDATA;
}

void
mpeg1_2_video_packetizer_c::flush_impl() {
  m_parser.SetEOS();
  generic_packetizer_c::process(new packet_t(new memory_c((unsigned char *)"", 0, false)));
}

void
mpeg1_2_video_packetizer_c::extract_fps(const unsigned char *buffer,
                                        int size) {
  int idx = mpeg1_2::extract_fps_idx(buffer, size);
  if (0 > idx)
    return;

  m_fps = mpeg1_2::get_fps(idx);
  if (0 < m_fps) {
    set_track_default_duration((int64_t)(1000000000.0 / m_fps));
    rerender_track_headers();
  } else
    m_fps = 0.0;
}

void
mpeg1_2_video_packetizer_c::extract_aspect_ratio(const unsigned char *buffer,
                                                 int size) {
  float ar;

  if (display_dimensions_or_aspect_ratio_set())
    return;

  if (!mpeg1_2::extract_ar(buffer, size, ar))
    return;

  set_video_display_dimensions((0 >= ar) || (1 == ar) ? m_width : (int)(m_height * ar), m_height, PARAMETER_SOURCE_BITSTREAM);

  rerender_track_headers();
  m_aspect_ratio_extracted = true;
}

void
mpeg1_2_video_packetizer_c::create_private_data() {
  MPEGChunk *raw_seq_hdr = m_parser.GetRealSequenceHeader();
  if (raw_seq_hdr) {
    set_codec_private(raw_seq_hdr->GetPointer(), raw_seq_hdr->GetSize());
    rerender_track_headers();
  }
}
