/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   MPEG4 part 2 video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include "common/common.h"
#include "common/endian.h"
#include "common/hacks.h"
#include "common/math.h"
#include "common/matroska.h"
#include "common/strings/formatting.h"
#include "merge/output_control.h"
#include "output/p_mpeg4_p2.h"

extern "C" {
#include <avilib.h>
}

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

  std::vector<video_frame_t> frames;
  mpeg4::p2::find_frame_types(packet->data->get(), packet->data->get_size(), frames);

  // Add a timecode and a duration if they've been given.
  if (-1 != packet->timecode)
    m_available_timecodes.push_back(packet->timecode);

  else if (0.0 == m_fps)
    mxerror_tid(ti.fname, ti.id, Y("Cannot convert non-native MPEG4 video frames into native ones if the source container "
                                   "provides neither timecodes nor a number of frames per second.\n"));

  if (-1 != packet->duration)
    m_available_durations.push_back(packet->duration);

  std::vector<video_frame_t>::iterator frame;
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
  for (i = 0; m_queued_frames.size() > i; ++i)
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
      mxverb(3, boost::format("mpeg4_p2::flush_frames(): Needed new timecode %1%\n") % m_previous_timecode);
    }

    while (m_available_durations.size() < m_queued_frames.size()) {
      m_available_durations.push_back((int64_t)(1000000000.0 / m_fps));
      mxverb(3, boost::format("mpeg4_p2::flush_frames(): Needed new duration %1%\n") % m_available_durations.back());
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

  if (1 < m_available_timecodes.size())
    m_available_timecodes.erase(m_available_timecodes.begin(), m_available_timecodes.end() - 1);
  if (1 < m_available_durations.size())
    m_available_durations.erase(m_available_durations.begin(), m_available_durations.end() - 1);

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
               % hvideo_display_width % hvideo_display_height);

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

      hvideo_display_width  = -1;
      hvideo_display_height = -1;

      generic_packetizer_c::set_headers();
      rerender_track_headers();

      mxinfo_tid(ti.fname, ti.id,
                 boost::format(Y("The extracted values for video width and height from the MPEG4 layer 2 video data bitstream differ from what the values "
                                 "in the source container. The ones from the video data bitstream (%1%x%2%) will be used.\n")) % xtr_width % xtr_height);
    }

  } else if (50 <= m_frames_output)
    m_aspect_ratio_extracted = true;
}

