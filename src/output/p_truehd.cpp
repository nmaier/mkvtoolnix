/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   TRUEHD output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/hacks.h"
#include "common/matroska.h"
#include "common/truehd.h"
#include "merge/output_control.h"
#include "merge/pr_generic.h"
#include "output/p_truehd.h"

using namespace libmatroska;

truehd_packetizer_c::truehd_packetizer_c(generic_reader_c *p_reader,
                                         track_info_c &p_ti,
                                         truehd_frame_t::codec_e codec,
                                         int sampling_rate,
                                         int channels)
  : generic_packetizer_c(p_reader, p_ti)
  , m_first_frame(true)
  , m_current_samples_per_frame(0)
  , m_samples_output(0)
  , m_ref_timecode(0)
  , m_s2tc(1000000000ll, sampling_rate)
{
  m_first_truehd_header.m_codec         = codec;
  m_first_truehd_header.m_sampling_rate = sampling_rate;
  m_first_truehd_header.m_channels      = channels;

  set_track_type(track_audio);
}

truehd_packetizer_c::~truehd_packetizer_c() {
}

void
truehd_packetizer_c::set_headers() {
  set_codec_id(m_first_truehd_header.is_truehd() ? MKV_A_TRUEHD : MKV_A_MLP);
  set_audio_sampling_freq((float)m_first_truehd_header.m_sampling_rate);
  set_audio_channels(m_first_truehd_header.m_channels);

  generic_packetizer_c::set_headers();

  if (hack_engaged(ENGAGE_MERGE_TRUEHD_FRAMES))
    m_track_entry->EnableLacing(false);
}

int
truehd_packetizer_c::process(packet_cptr packet) {
  m_parser.add_data(packet->data->get_buffer(), packet->data->get_size());

  handle_frames();

  return FILE_STATUS_MOREDATA;
}

void
truehd_packetizer_c::handle_frames() {
  while (m_parser.frame_available()) {
    truehd_frame_cptr frame = m_parser.get_next_frame();

    if (frame->is_sync()) {
      adjust_header_values(frame);
      flush();

    }

    if (!frame->is_ac3())
      m_frames.push_back(frame);
  }
}

void
truehd_packetizer_c::adjust_header_values(truehd_frame_cptr &frame) {
  if (!m_first_frame)
    return;

  bool rerender_headers = false;
  if (frame->m_codec != m_first_truehd_header.m_codec) {
    rerender_headers              = true;
    m_first_truehd_header.m_codec = frame->m_codec;
    set_codec_id(m_first_truehd_header.is_truehd() ? MKV_A_TRUEHD : MKV_A_MLP);
  }

  if (frame->m_channels != m_first_truehd_header.m_channels) {
    rerender_headers                 = true;
    m_first_truehd_header.m_channels = frame->m_channels;
    set_audio_channels(m_first_truehd_header.m_channels);
  }

  if (frame->m_sampling_rate != m_first_truehd_header.m_sampling_rate) {
    rerender_headers                      = true;
    m_first_truehd_header.m_sampling_rate = frame->m_sampling_rate;
    set_audio_channels(m_first_truehd_header.m_sampling_rate);
  }

  m_first_truehd_header.m_samples_per_frame = frame->m_samples_per_frame;

  if (rerender_headers)
    rerender_track_headers();

  m_first_frame = false;

  m_s2tc.set(1000000000ll, m_first_truehd_header.m_sampling_rate);
}

void
truehd_packetizer_c::flush() {
  m_parser.parse(true);
  flush_frames();
}

void
truehd_packetizer_c::flush_frames() {
  if (m_frames.empty())
    return;

  if (hack_engaged(ENGAGE_MERGE_TRUEHD_FRAMES))
    flush_frames_merged();
  else
    flush_frames_separate();
}

void
truehd_packetizer_c::flush_frames_separate() {
  size_t i;
  for (i = 0; m_frames.size() > i; ++i) {
    if (m_frames[i]->is_sync())
      m_current_samples_per_frame = m_frames[i]->m_samples_per_frame;

    int samples       = 0 == m_frames[i]->m_samples_per_frame ? m_current_samples_per_frame : m_frames[i]->m_samples_per_frame;
    int64_t timecode  = m_samples_output * m_s2tc;
    int64_t duration  = samples          * m_s2tc;
    m_samples_output += samples;

    add_packet(new packet_t(m_frames[i]->m_data, timecode, duration, m_frames[i]->is_sync() ? -1 : m_ref_timecode));

    m_ref_timecode = timecode;
  }

  m_frames.clear();
}

void
truehd_packetizer_c::flush_frames_merged() {
  size_t full_size     = 0;
  unsigned int samples = 0;
  size_t i;
  for (i = 0; m_frames.size() > i; ++i) {
    if (m_frames[i]->is_sync())
      m_current_samples_per_frame = m_frames[i]->m_samples_per_frame;

    full_size += m_frames[i]->m_data->get_size();
    samples   += 0 == m_frames[i]->m_samples_per_frame ? m_current_samples_per_frame : m_frames[i]->m_samples_per_frame;
  }

  memory_cptr data = memory_c::alloc(full_size);

  unsigned int offset = 0;
  for (i = 0; m_frames.size() > i; ++i) {
    memcpy(data->get_buffer() + offset, m_frames[i]->m_data->get_buffer(), m_frames[i]->m_data->get_size());
    offset += m_frames[i]->m_data->get_size();
  }

  int64_t timecode  = m_samples_output * m_s2tc;
  int64_t duration  = samples          * m_s2tc;
  m_samples_output += samples;

  add_packet(new packet_t(data, timecode, duration));

  m_frames.clear();
}

connection_result_e
truehd_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                 std::string &error_message) {
  truehd_packetizer_c *asrc = dynamic_cast<truehd_packetizer_c *>(src);
  if (!asrc)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_a_samplerate(m_first_truehd_header.m_sampling_rate, asrc->m_first_truehd_header.m_sampling_rate);
  connect_check_a_channels(m_first_truehd_header.m_channels, asrc->m_first_truehd_header.m_channels);

  return CAN_CONNECT_YES;
}

