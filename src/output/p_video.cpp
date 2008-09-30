/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

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
#include "p_video.h"

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

// ----------------------------------------------------------------

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

// ----------------------------------------------------------------

mpeg4_p2_video_packetizer_c::
mpeg4_p2_video_packetizer_c(generic_reader_c *p_reader,
                            track_info_c &p_ti,
                            double fps,
                            int width,
                            int height,
                            bool input_is_native)
  : video_packetizer_c(p_reader, p_ti, MKV_V_MPEG4_ASP, fps, width, height)
  , m_timecodes_generated(0)
  , m_last_i_p_frame(-1)
  , m_previous_timecode(0)
  , m_aspect_ratio_extracted(false)
  , m_input_is_native(input_is_native)
  , m_output_is_native(hack_engaged(ENGAGE_NATIVE_MPEG4) || input_is_native)
  , m_size_extracted(false)
{

  if (!m_output_is_native) {
    set_codec_id(MKV_V_MSCOMP);
    check_fourcc();

  } else {
    set_codec_id(MKV_V_MPEG4_ASP);
    if (!m_input_is_native) {
      safefree(ti.private_data);
      ti.private_data = NULL;
      ti.private_size = 0;
    }
  }

  timecode_factory_application_mode = TFA_SHORT_QUEUEING;
}

int
mpeg4_p2_video_packetizer_c::process(packet_cptr packet) {
  if (!m_size_extracted)
    extract_size(packet->data->get(), packet->data->get_size());
  if (!m_aspect_ratio_extracted)
    extract_aspect_ratio(packet->data->get(), packet->data->get_size());

  if (m_input_is_native == m_output_is_native)
    return video_packetizer_c::process(packet);

  if (m_input_is_native)
    return process_native(packet);

  return process_non_native(packet);
}

int
mpeg4_p2_video_packetizer_c::process_non_native(packet_cptr packet) {
  if (NULL == ti.private_data) {
    memory_c *config_data = mpeg4::p2::parse_config_data(packet->data->get(), packet->data->get_size());
    if (NULL != config_data) {
      ti.private_data = (unsigned char *)safememdup(config_data->get(), config_data->get_size());
      ti.private_size = config_data->get_size();
      delete config_data;

      fix_codec_string();
      set_codec_private(ti.private_data, ti.private_size);
      rerender_track_headers();

    } else
      mxerror_tid(ti.fname, ti.id, Y("Could not find the codec configuration data in the first MPEG-4 part 2 video frame. This track cannot be stored in native mode.\n"));
  }

  vector<video_frame_t> frames;
  mpeg4::p2::find_frame_types(packet->data->get(), packet->data->get_size(), frames);

  // Add a timecode and a duration if they've been given.
  if (-1 != packet->timecode)
    m_available_timecodes.push_back(packet->timecode);

  else if (0.0 == m_fps)
    mxerror_tid(ti.fname, ti.id, Y("Cannot convert non-native MPEG4 video frames into native ones if the source container "
                                   "provides neither timecodes nor a number of frames per second.\n"));

  if (-1 != packet->duration)
    m_available_durations.push_back(packet->duration);

  vector<video_frame_t>::iterator frame;
  mxforeach(frame, frames) {
    // Maybe we can flush queued frames now. But only if we don't have
    // a B frame.
    if (FRAME_TYPE_B != frame->type)
      flush_frames_maybe(frame->type);

    // Add a timecode and a duration for each frame if none have been
    // given and we have a fixed number of FPS.
    if (-1 == packet->timecode) {
      m_available_timecodes.push_back((int64_t)(m_timecodes_generated * 1000000000.0 / m_fps));
      ++m_timecodes_generated;
    }
    if (-1 == packet->duration)
      m_available_durations.push_back((int64_t)(1000000000.0 / m_fps));

    frame->data = (unsigned char *)safememdup(packet->data->get() + frame->pos, frame->size);
    m_queued_frames.push_back(*frame);
  }

  m_previous_timecode = m_available_timecodes.back();

  return FILE_STATUS_MOREDATA;
}

void
mpeg4_p2_video_packetizer_c::fix_codec_string() {
  static const unsigned char start_code[4] = {0x00, 0x00, 0x01, 0xb2};

  if ((NULL == ti.private_data) || (0 == ti.private_size))
    return;

  int size = ti.private_size;
  int i;
  for (i = 0; 9 < size;) {
    if (memcmp(&ti.private_data[i], start_code, 4) != 0) {
      ++i;
      --size;
      continue;
    }

    i    += 8;
    size -= 8;
    if (strncasecmp((const char *)&ti.private_data[i - 4], "divx", 4) != 0)
      continue;

    unsigned char *end_pos = (unsigned char *)memchr(&ti.private_data[i], 0, size);
    if (NULL == end_pos)
      end_pos = &ti.private_data[i + size];

    --end_pos;
    if ('p' == *end_pos)
      *end_pos = 'n';

    return;
  }
}

int
mpeg4_p2_video_packetizer_c::process_native(packet_cptr packet) {
  // Not implemented yet.
  return FILE_STATUS_MOREDATA;
}

void
mpeg4_p2_video_packetizer_c::flush_frames_maybe(frame_type_e next_frame) {
  if ((0 == m_queued_frames.size()) || (FRAME_TYPE_B == next_frame))
    return;

  if ((FRAME_TYPE_I == next_frame) || (FRAME_TYPE_P == m_queued_frames[0].type)) {
    flush_frames(false);
    return;
  }

  int num_bframes = 0;
  int i;
  for (i = 0; m_queued_frames.size() > 0; ++i)
    if (FRAME_TYPE_B == m_queued_frames[i].type)
      ++num_bframes;

  if ((0 < num_bframes) || (2 <= m_queued_frames.size()))
    flush_frames(false);
}

/** \brief Handle frame sequences in which too few timecodes are available

   This function gets called if mkvmerge wants to flush its frame queue
   but it doesn't have enough timecodes and/or durations available for
   each queued frame. This can happen in two cases:

   1. A picture sequence is found that mkvmerge does not support. An
      example: Two frames have been read. The first contained a
      P and a B frame (that's OK so far), but the second one contained
      another P or I frame without an intermediate dummy frame.

   2. The end of the file has been reached but the frame queue contains
      more frames than the timecode queue. For example: The last frame
      contained two frames, a P and a B frame. Right afterwards the
      end of the file is reached. In this case a dummy frame is missing.

   Both cases can be solved if the source file provides a FPS for this
   track. Otherwise such frames have to be dropped.
*/
void
mpeg4_p2_video_packetizer_c::handle_missing_timecodes(bool end_of_file) {
  if (0.0 < m_fps) {
    while (m_available_timecodes.size() < m_queued_frames.size()) {
      m_previous_timecode = (int64_t)(m_previous_timecode +  1000000000.0 / m_fps);
      m_available_timecodes.push_back(m_previous_timecode);
      mxverb(3, boost::format(Y("mpeg4_p2::flush_frames(): Needed new timecode %1%\n")) % m_previous_timecode);
    }

    while (m_available_durations.size() < m_queued_frames.size()) {
      m_available_durations.push_back((int64_t)(1000000000.0 / m_fps));
      mxverb(3, boost::format(Y("mpeg4_p2::flush_frames(): Needed new duration %1%\n")) % m_available_durations.back());
    }

    return;
  }

  int num_dropped = 0;
  if (m_available_timecodes.size() < m_available_durations.size()) {
    num_dropped = m_queued_frames.size() - m_available_timecodes.size();
    m_available_durations.erase(m_available_durations.begin() + m_available_timecodes.size(), m_available_durations.end());

  } else {
    num_dropped = m_queued_frames.size() - m_available_durations.size();
    m_available_timecodes.erase(m_available_timecodes.begin() + m_available_durations.size(), m_available_timecodes.end());
  }

  int i;
  for (i = m_available_timecodes.size(); m_queued_frames.size() > i; ++i)
    safefree(m_queued_frames[i].data);
  m_queued_frames.erase(m_queued_frames.begin() + m_available_timecodes.size(), m_queued_frames.end());

  int64_t timecode = m_available_timecodes.empty() ? 0 : m_available_timecodes.front();

  mxwarn_tid(ti.fname, ti.id,
             boost::format(Y("During MPEG-4 part 2 B frame handling: The frame queue contains more frames than timecodes are available that "
                             "can be assigned to them (reason: %1%). Therefore %2% frame(s) had to be dropped. "
                             "The video might be broken around timecode %3%.\n"))
             % (end_of_file ? "The end of the source file was encountered." : "An unsupported sequence of frames was encountered.")
             % num_dropped % format_timecode(timecode, 3));
}

void
mpeg4_p2_video_packetizer_c::flush_frames(bool end_of_file) {
  if (m_queued_frames.empty())
    return;

  if ((m_available_timecodes.size() < m_queued_frames.size()) ||
      (m_available_durations.size() < m_queued_frames.size()))
    handle_missing_timecodes(end_of_file);

  // Very first frame?
  if (-1 == m_last_i_p_frame) {
    video_frame_t &frame = m_queued_frames.front();
    m_last_i_p_frame     = m_available_timecodes[0];

    add_packet(new packet_t(new memory_c(frame.data, frame.size, true), m_last_i_p_frame, m_available_durations[0], -1, -1));
    m_available_timecodes.erase(m_available_timecodes.begin(), m_available_timecodes.begin() + 1);
    m_available_durations.erase(m_available_durations.begin(), m_available_durations.begin() + 1);
    m_queued_frames.pop_front();
    flush_frames(end_of_file);

    return;
  }

  m_queued_frames[0].timecode = m_available_timecodes[m_queued_frames.size() - 1];
  m_queued_frames[0].duration = m_available_durations[m_queued_frames.size() - 1];
  m_queued_frames[0].fref     = -1;

  if (FRAME_TYPE_I == m_queued_frames[0].type)
    m_queued_frames[0].bref   = -1;
  else
    m_queued_frames[0].bref   = m_last_i_p_frame;

  int i;
  for (i = 1; m_queued_frames.size() > i; ++i) {
    m_queued_frames[i].timecode = m_available_timecodes[i - 1];
    m_queued_frames[i].duration = m_available_durations[i - 1];
    m_queued_frames[i].fref     = m_queued_frames[0].timecode;
    m_queued_frames[i].bref     = m_last_i_p_frame;
  }

  m_last_i_p_frame = m_queued_frames[0].timecode;

  for (i = 0; i < m_queued_frames.size(); ++i)
    add_packet(new packet_t(new memory_c(m_queued_frames[i].data, m_queued_frames[i].size, true),
                            m_queued_frames[i].timecode, m_queued_frames[i].duration, m_queued_frames[i].bref, m_queued_frames[i].fref));

  m_available_timecodes.erase(m_available_timecodes.begin(), m_available_timecodes.begin() + m_queued_frames.size());
  m_available_durations.erase(m_available_durations.begin(), m_available_durations.begin() + m_queued_frames.size());

  m_queued_frames.clear();
}

void
mpeg4_p2_video_packetizer_c::flush() {
  flush_frames(true);
  generic_packetizer_c::flush();
}

void
mpeg4_p2_video_packetizer_c::extract_aspect_ratio(const unsigned char *buffer,
                                                  int size) {
  if (ti.aspect_ratio_given || ti.display_dimensions_given) {
    m_aspect_ratio_extracted = true;
    return;
  }

  uint32_t num, den;
  if (mpeg4::p2::extract_par(buffer, size, num, den)) {
    m_aspect_ratio_extracted = true;
    ti.aspect_ratio_given    = true;
    ti.aspect_ratio          = (float)hvideo_pixel_width / (float)hvideo_pixel_height * (float)num / (float)den;

    generic_packetizer_c::set_headers();
    rerender_track_headers();
    mxinfo_tid(ti.fname, ti.id,
               boost::format(Y("Extracted the aspect ratio information from the MPEG4 layer 2 video data and set the display dimensions to %1%/%2%.\n"))
               % ti.display_width % ti.display_height);

  } else if (50 <= m_frames_output)
    m_aspect_ratio_extracted = true;
}

void
mpeg4_p2_video_packetizer_c::extract_size(const unsigned char *buffer,
                                          int size) {
  uint32_t xtr_width, xtr_height;

  if (mpeg4::p2::extract_size(buffer, size, xtr_width, xtr_height)) {
    m_size_extracted = true;

    if (!reader->appending && ((xtr_width != hvideo_pixel_width) || (xtr_height != hvideo_pixel_height))) {
      set_video_pixel_width(xtr_width);
      set_video_pixel_height(xtr_height);

      if (!m_output_is_native && (sizeof(alBITMAPINFOHEADER) <= ti.private_size)) {
        alBITMAPINFOHEADER *bih = (alBITMAPINFOHEADER *)ti.private_data;
        put_uint32_le(&bih->bi_width,  xtr_width);
        put_uint32_le(&bih->bi_height, xtr_height);
        set_codec_private(ti.private_data, ti.private_size);
      }

      generic_packetizer_c::set_headers();
      rerender_track_headers();

      mxinfo_tid(ti.fname, ti.id,
                 boost::format(Y("The extracted values for video width and height from the MPEG4 layer 2 video data bitstream differ from what the values "
                                 "in the source container. The ones from the video data bitstream (%1%x%2%) will be used.\n")) % xtr_width % xtr_height);
    }

  } else if (50 <= m_frames_output)
    m_aspect_ratio_extracted = true;
}

// ----------------------------------------------------------------

mpeg4_p10_video_packetizer_c::
mpeg4_p10_video_packetizer_c(generic_reader_c *p_reader,
                             track_info_c &p_ti,
                             double fps,
                             int width,
                             int height)
  : video_packetizer_c(p_reader, p_ti, MKV_V_MPEG4_AVC, fps, width, height)
  , m_nalu_size_len_src(0)
  , m_nalu_size_len_dst(0)
  , m_max_nalu_size(0)
{

  relaxed_timecode_checking = true;

  if ((NULL != ti.private_data) && (0 < ti.private_size)) {
    extract_aspect_ratio();
    setup_nalu_size_len_change();
  }
}

void
mpeg4_p10_video_packetizer_c::extract_aspect_ratio() {
  uint32_t num, den;
  unsigned char *priv = ti.private_data;

  if (mpeg4::p10::extract_par(ti.private_data, ti.private_size, num, den) && (0 != num) && (0 != den)) {
    if (!ti.aspect_ratio_given && !ti.display_dimensions_given) {
      double par = (double)num / (double)den;

      if (1 <= par) {
        ti.display_width  = irnd(m_width * par);
        ti.display_height = m_height;

      } else {
        ti.display_width  = m_width;
        ti.display_height = irnd(m_height / par);

      }

      ti.display_dimensions_given = true;
      mxinfo_tid(ti.fname, ti.id,
                 boost::format(Y("Extracted the aspect ratio information from the MPEG-4 layer 10 (AVC) video data and set the display dimensions to %1%/%2%.\n"))
                 % ti.display_width % ti.display_height);
    }

    set_codec_private(ti.private_data, ti.private_size);
  }

  if (priv != ti.private_data)
    safefree(priv);
}

int
mpeg4_p10_video_packetizer_c::process(packet_cptr packet) {
  if (VFT_PFRAMEAUTOMATIC == packet->bref) {
    packet->fref = -1;
    packet->bref = m_ref_timecode;
  }

  m_ref_timecode = packet->timecode;

  if (m_nalu_size_len_dst)
    change_nalu_size_len(packet);

  add_packet(packet);

  return FILE_STATUS_MOREDATA;
}

connection_result_e
mpeg4_p10_video_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                             string &error_message) {
  mpeg4_p10_video_packetizer_c *vsrc = dynamic_cast<mpeg4_p10_video_packetizer_c *>(src);
  if (NULL == vsrc)
    return CAN_CONNECT_NO_FORMAT;

  connection_result_e result = video_packetizer_c::can_connect_to(src, error_message);
  if (CAN_CONNECT_YES != result)
    return result;

  if ((NULL != ti.private_data) && memcmp(ti.private_data, vsrc->ti.private_data, ti.private_size)) {
    error_message = (boost::format(Y("The codec's private data does not match. Both have the same length (%1%) but different content.")) % ti.private_size).str();
    return CAN_CONNECT_MAYBE_CODECPRIVATE;
  }

  return CAN_CONNECT_YES;
}

void
mpeg4_p10_video_packetizer_c::setup_nalu_size_len_change() {
  if (!ti.private_data || (5 > ti.private_size))
    return;

  m_nalu_size_len_src = (ti.private_data[4] & 0x03) + 1;

  if (!ti.nalu_size_length || (ti.nalu_size_length == m_nalu_size_len_src))
    return;

  m_nalu_size_len_dst = ti.nalu_size_length;
  ti.private_data[4]  = (ti.private_data[4] & 0xfc) | (m_nalu_size_len_dst - 1);
  m_max_nalu_size     = 1ll << (8 * m_nalu_size_len_dst);

  set_codec_private(ti.private_data, ti.private_size);

  mxverb(2, boost::format(Y("mpeg4_p10: Adjusting NALU size length from %1% to %2%\n")) % m_nalu_size_len_src % m_nalu_size_len_dst);
}

void
mpeg4_p10_video_packetizer_c::change_nalu_size_len(packet_cptr packet) {
  unsigned char *src = packet->data->get();
  int size           = packet->data->get_size();

  if (!src || !size)
    return;

  vector<int> nalu_sizes;

  int src_pos = 0;

  // Find all NALU sizes in this packet.
  while (src_pos < size) {
    if ((size - src_pos) < m_nalu_size_len_src)
      break;

    int nalu_size = 0;
    int i;
    for (i = 0; i < m_nalu_size_len_src; ++i)
      nalu_size = (nalu_size << 8) + src[src_pos + i];

    if ((size - src_pos - m_nalu_size_len_src) < nalu_size)
      nalu_size = size - src_pos - m_nalu_size_len_src;

    if (nalu_size > m_max_nalu_size)
      mxerror_tid(ti.fname, ti.id, boost::format(Y("The chosen NALU size length of %1% is too small. Try using '4'.\n")) % m_nalu_size_len_dst);

    src_pos += m_nalu_size_len_src + nalu_size;

    nalu_sizes.push_back(nalu_size);
  }

  // Allocate memory if the new NALU size length is greater
  // than the previous one. Otherwise reuse the existing memory.
  if (m_nalu_size_len_dst > m_nalu_size_len_src) {
    int new_size = size + nalu_sizes.size() * (m_nalu_size_len_dst - m_nalu_size_len_src);
    packet->data = memory_cptr(new memory_c((unsigned char *)safemalloc(new_size), new_size, true));
  }

  // Copy the NALUs and write the new sized length field.
  unsigned char *dst = packet->data->get();
  src_pos            = 0;
  int dst_pos        = 0;

  int i;
  for (i = 0; nalu_sizes.size() > i; ++i) {
    int nalu_size = nalu_sizes[i];

    int shift;
    for (shift = 0; shift < m_nalu_size_len_dst; ++shift)
      dst[dst_pos + shift] = (nalu_size >> (8 * (m_nalu_size_len_dst - 1 - shift))) & 0xff;

    memmove(&dst[dst_pos + m_nalu_size_len_dst], &src[src_pos + m_nalu_size_len_src], nalu_size);

    src_pos += m_nalu_size_len_src + nalu_size;
    dst_pos += m_nalu_size_len_dst + nalu_size;
  }

  packet->data->set_size(dst_pos);
}

// ----------------------------------------------------------------

theora_video_packetizer_c::
theora_video_packetizer_c(generic_reader_c *p_reader,
                          track_info_c &p_ti,
                          double fps,
                          int width,
                          int height)
  : video_packetizer_c(p_reader, p_ti, MKV_V_THEORA, fps, width, height)
{
}

int
theora_video_packetizer_c::process(packet_cptr packet) {
  if (packet->data->get_size() && (0x00 == (packet->data->get()[0] & 0x40)))
    packet->bref = VFT_IFRAME;
  else
    packet->bref = VFT_PFRAMEAUTOMATIC;

  packet->fref   = VFT_NOBFRAME;

  return video_packetizer_c::process(packet);
}

