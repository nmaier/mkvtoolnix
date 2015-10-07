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

#include "common/codec.h"
#include "common/endian.h"
#include "common/hacks.h"
#include "common/math.h"
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
  , m_debug_stuffing_removal{"mpeg1_2|mpeg1_2_stuffing_removal"}
{

  set_codec_id((boost::format("V_MPEG%1%") % version).str());
  if (!display_dimensions_or_aspect_ratio_set()) {
    if ((0 < dwidth) && (0 < dheight))
      set_video_display_dimensions(dwidth, dheight, OPTION_SOURCE_BITSTREAM);
    else
      m_aspect_ratio_extracted = false;
  }

  m_timecode_factory_application_mode = TFA_SHORT_QUEUEING;

  // m_parser.SeparateSequenceHeaders();
}

mpeg1_2_video_packetizer_c::~mpeg1_2_video_packetizer_c() {
  mxdebug_if(m_debug_stuffing_removal, boost::format("Total number of stuffing bytes removed: %1%\n") % m_num_removed_stuffing_bytes);
}

void
mpeg1_2_video_packetizer_c::remove_stuffing_bytes_and_handle_sequence_headers(packet_cptr packet) {
  mxdebug_if(m_debug_stuffing_removal, boost::format("Starting stuff removal, frame size %1%\n") % packet->data->get_size());

  auto buf              = packet->data->get_buffer();
  auto size             = packet->data->get_size();
  size_t pos            = 4;
  size_t start_code_pos = 0;
  size_t stuffing_start = 0;
  auto marker           = get_uint32_be(buf);
  uint32_t chunk_type   = 0;
  bool seq_hdr_found    = false;

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

  memory_cptr new_seq_hdr;
  auto seq_hdr_copier = [&](bool at_end) {
    if ((MPEGVIDEO_SEQUENCE_HEADER_START_CODE != chunk_type) && (MPEGVIDEO_EXT_START_CODE != chunk_type))
      return;

    if (MPEGVIDEO_SEQUENCE_HEADER_START_CODE == chunk_type)
      seq_hdr_found = true;

    else if (!seq_hdr_found)
      return;

    auto bytes_to_copy = at_end ? pos - start_code_pos : pos - 4 - start_code_pos;

    if (!new_seq_hdr)
      new_seq_hdr = memory_c::clone(&buf[start_code_pos], bytes_to_copy);

    else
      new_seq_hdr->add(&buf[start_code_pos], bytes_to_copy);

    if (!hack_engaged(ENGAGE_USE_CODEC_STATE_ONLY))
      return;

    memmove(&buf[start_code_pos], &buf[pos - 4], size - pos + 4);
    pos  -= bytes_to_copy;
    size -= bytes_to_copy;
  };

  while (pos < size) {
    if ((MPEGVIDEO_SLICE_START_CODE_LOWER <= marker) && (MPEGVIDEO_SLICE_START_CODE_UPPER >= marker)) {
      mxdebug_if(m_debug_stuffing_removal, boost::format("  Slice start code at %1%\n") % (pos - 4));

      mid_remover();
      seq_hdr_copier(false);

      chunk_type     = MPEGVIDEO_SLICE_START_CODE_LOWER;
      stuffing_start = 0;
      start_code_pos = pos - 4;

    } else if ((marker & 0xffffff00) == 0x00000100) {
      mxdebug_if(m_debug_stuffing_removal, boost::format("  Non-slice start code at %1%\n") % (pos - 4));

      mid_remover();
      seq_hdr_copier(false);

      chunk_type     = marker;
      stuffing_start = 0;
      start_code_pos = pos - 4;

    } else if ((MPEGVIDEO_SLICE_START_CODE_LOWER == chunk_type) && !stuffing_start && (marker == 0x00000000)) {
      mxdebug_if(m_debug_stuffing_removal, boost::format("    Start at %1%\n") % (pos - 3));

      stuffing_start = pos - 3;
    }

    marker <<= 8;
    marker  |= buf[pos];
    ++pos;
  }

  if ((marker & 0xffffff00) == 0x00000100) {
    seq_hdr_copier(false);
    mid_remover();

  } else if (stuffing_start && (stuffing_start < size)) {
    mxdebug_if(m_debug_stuffing_removal, boost::format("    Stuffing at the end: chunk_type 0x%|1$08x| stuffing_start %2% stuffing_size %3%\n") % chunk_type % stuffing_start % (stuffing_start ? size - stuffing_start : 0));

    m_num_removed_stuffing_bytes += size - stuffing_start;
    size                          = stuffing_start;
  }

  packet->data->set_size(size);

  if (!new_seq_hdr)
    return;

  if (!m_hcodec_private) {
    set_codec_private(new_seq_hdr);
    rerender_track_headers();
  }

  if (!m_seq_hdr || (*new_seq_hdr != *m_seq_hdr)) {
    m_seq_hdr           = new_seq_hdr;
    packet->codec_state = new_seq_hdr->clone();
  }
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
mpeg1_2_video_packetizer_c::process_framed(packet_cptr packet) {
  if (0 == packet->data->get_size())
    return FILE_STATUS_MOREDATA;

  if (4 > packet->data->get_size())
    return video_packetizer_c::process(packet);

  remove_stuffing_bytes_and_handle_sequence_headers(packet);

  return video_packetizer_c::process(packet);
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

      remove_stuffing_bytes_and_handle_sequence_headers(new_packet);

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

  set_video_display_dimensions((0 >= ar) || (1 == ar) ? m_width : (int)(m_height * ar), m_height, OPTION_SOURCE_BITSTREAM);

  rerender_track_headers();
  m_aspect_ratio_extracted = true;
}

void
mpeg1_2_video_packetizer_c::create_private_data() {
  MPEGChunk *raw_seq_hdr = m_parser.GetRealSequenceHeader();
  if (raw_seq_hdr) {
    set_codec_private(memory_c::clone(raw_seq_hdr->GetPointer(), raw_seq_hdr->GetSize()));
    rerender_track_headers();
  }
}
