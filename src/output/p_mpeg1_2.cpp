/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   MPEG1/2 video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include "common/common.h"
#include "common/endian.h"
#include "common/hacks.h"
#include "common/math.h"
#include "common/matroska.h"
#include "common/mpeg1_2.h"
#include "common/mpeg4_p2.h"
#include "common/string_formatting.h"
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
  , m_framed(framed)
  , m_aspect_ratio_extracted(false)
{

  set_codec_id((boost::format("V_MPEG%1%") % version).str());
  if (!ti.aspect_ratio_given && !ti.display_dimensions_given) {
    if ((0 < dwidth) && (0 < dheight)) {
      m_aspect_ratio_extracted    = true;
      ti.display_dimensions_given = true;
      ti.display_width            = dwidth;
      ti.display_height           = dheight;
    }
  } else
    m_aspect_ratio_extracted      = true;

  timecode_factory_application_mode = TFA_SHORT_QUEUEING;

  if (hack_engaged(ENGAGE_USE_CODEC_STATE))
    m_parser.SeparateSequenceHeaders();
}

int
mpeg1_2_video_packetizer_c::process_framed(packet_cptr packet) {
  if (0 == packet->data->get_size())
    return FILE_STATUS_MOREDATA;

  if (!hack_engaged(ENGAGE_USE_CODEC_STATE) || (4 > packet->data->get_size()))
    return video_packetizer_c::process(packet);

  unsigned char *buf = packet->data->get();
  int pos            = 4;
  int size           = packet->data->get_size();
  int marker         = get_uint32_be(buf);

  while ((pos < size) && (MPEGVIDEO_SEQUENCE_START_CODE != marker)) {
    marker <<= 8;
    marker  |= buf[pos];
    ++pos;
  }

  if ((MPEGVIDEO_SEQUENCE_START_CODE != marker) || ((pos + 4) >= size))
    return video_packetizer_c::process(packet);

  int start  = pos - 4;
  marker     = get_uint32_be(&buf[pos]);
  pos       += 4;

  while ((pos < size) && ((MPEGVIDEO_EXT_START_CODE == marker) || (0x00000100 != (marker & 0xffffff00)))) {
    marker <<= 8;
    marker  |= buf[pos];
    ++pos;
  }

  if (pos >= size)
    return video_packetizer_c::process(packet);

  pos         -= 4;
  int sh_size  = pos - start;

  if ((NULL == m_seq_hdr.get()) || (sh_size != m_seq_hdr->get_size()) || memcmp(&buf[start], m_seq_hdr->get(), sh_size)) {
    m_seq_hdr           = clone_memory(&buf[start], sh_size);
    packet->codec_state = clone_memory(&buf[start], sh_size);
  }

  memmove(&buf[start], &buf[pos], size - pos);
  packet->data->set_size(size - sh_size);

  return video_packetizer_c::process(packet);
}

int
mpeg1_2_video_packetizer_c::process(packet_cptr packet) {
  if (0.0 > m_fps)
    extract_fps(packet->data->get(), packet->data->get_size());

  if (!m_aspect_ratio_extracted)
    extract_aspect_ratio(packet->data->get(), packet->data->get_size());

  if (m_framed)
    return process_framed(packet);

  int state = m_parser.GetState();
  if ((MPV_PARSER_STATE_EOS == state) || (MPV_PARSER_STATE_ERROR == state))
    return FILE_STATUS_DONE;

  memory_cptr old_memory  = packet->data;
  unsigned char *data_ptr = old_memory->get();
  int new_bytes           = old_memory->get_size();

  do {
    int bytes_to_add = (m_parser.GetFreeBufferSpace() < new_bytes) ? m_parser.GetFreeBufferSpace() : new_bytes;
    if (0 < bytes_to_add) {
      m_parser.WriteData(data_ptr, bytes_to_add);
      data_ptr  += bytes_to_add;
      new_bytes -= bytes_to_add;
    }

    state = m_parser.GetState();
    while (MPV_PARSER_STATE_FRAME == state) {
      MPEGFrame *frame = m_parser.ReadFrame();
      if (NULL == frame)
        break;

      packet_t *new_packet    = new packet_t(new memory_c(frame->data, frame->size, true), frame->timecode, frame->duration, frame->firstRef, frame->secondRef);
      new_packet->time_factor = MPEG2_PICTURE_TYPE_FRAME == frame->pictureStructure ? 1 : 2;

      if (   (NULL != frame->seqHdrData)
          && (   (NULL == m_seq_hdr.get())
              || (frame->seqHdrDataSize != m_seq_hdr->get_size())
              || memcmp(frame->seqHdrData, m_seq_hdr->get(), frame->seqHdrDataSize))) {
        m_seq_hdr               = memory_cptr(new memory_c(frame->seqHdrData, frame->seqHdrDataSize, true));
        frame->seqHdrData       = NULL;
        new_packet->codec_state = clone_memory(m_seq_hdr);
      }

      video_packetizer_c::process(packet_cptr(new_packet));

      frame->data = NULL;
      delete frame;

      state = m_parser.GetState();
    }
  } while (0 < new_bytes);

  return FILE_STATUS_MOREDATA;
}

void
mpeg1_2_video_packetizer_c::flush() {
  m_parser.SetEOS();
  generic_packetizer_c::process(new packet_t(new memory_c((unsigned char *)"", 0, false)));
  video_packetizer_c::flush();
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

  if (ti.aspect_ratio_given || ti.display_dimensions_given)
    return;

  if (!mpeg1_2::extract_ar(buffer, size, ar))
    return;

  ti.display_dimensions_given = true;
  if ((0 >= ar) || (1 == ar))
    set_video_display_width(m_width);
  else
    set_video_display_width((int)(m_height * ar));
  set_video_display_height(m_height);

  rerender_track_headers();
  m_aspect_ratio_extracted = true;
}

void
mpeg1_2_video_packetizer_c::create_private_data() {
  MPEGChunk *raw_seq_hdr = m_parser.GetRealSequenceHeader();
  if (NULL != raw_seq_hdr) {
    set_codec_private(raw_seq_hdr->GetPointer(), raw_seq_hdr->GetSize());
    rerender_track_headers();
  }
}
