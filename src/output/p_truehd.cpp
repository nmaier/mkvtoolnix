/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   TRUEHD output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <vector>

#include "truehd_common.h"
#include "matroska.h"
#include "output_control.h"
#include "pr_generic.h"
#include "p_truehd.h"

using namespace libmatroska;

truehd_packetizer_c::truehd_packetizer_c(generic_reader_c *p_reader,
                                         track_info_c &p_ti,
                                         int sampling_rate,
                                         int channels)
  throw (error_c)
  : generic_packetizer_c(p_reader, p_ti)
  , m_first_frame(true)
  , m_samples_output(0)
{
  m_first_truehd_header.m_sampling_rate = sampling_rate;
  m_first_truehd_header.m_channels      = channels;

  set_track_type(track_audio);
}

truehd_packetizer_c::~truehd_packetizer_c() {
}

void
truehd_packetizer_c::set_headers() {
  set_codec_id(MKV_A_TRUEHD);
  set_audio_sampling_freq((float)m_first_truehd_header.m_sampling_rate);
  set_audio_channels(m_first_truehd_header.m_channels);

  generic_packetizer_c::set_headers();

  track_entry->EnableLacing(false);
}

int
truehd_packetizer_c::process(packet_cptr packet) {
  m_parser.add_data(packet->data->get(), packet->data->get_size());

  while (m_parser.frame_available()) {
    truehd_frame_cptr frame = m_parser.get_next_frame();
    if (truehd_frame_t::sync == frame->m_type) {
      adjust_header_values(frame);
      flush();

    } else if (truehd_frame_t::ac3 != frame->m_type)
      m_frames.push_back(frame);
  }

  return FILE_STATUS_MOREDATA;
}

void
truehd_packetizer_c::adjust_header_values(truehd_frame_cptr &frame) {
  if (!m_first_frame)
    return;

  bool rerender_headers = false;
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
}

void
truehd_packetizer_c::flush() {
  if (m_frames.empty())
    return;

  int full_size = 0;
  int i;
  for (i = 0; m_frames.size() > i; ++i)
    full_size += m_frames[i]->m_data->get_size();

  memory_cptr data = memory_c::alloc(full_size);

  int offset = 0;
  for (i = 0; m_frames.size() > i; ++i) {
    memcpy(data->get() + offset, m_frames[i]->m_data->get(), m_frames[i]->m_data->get_size());
    offset += m_frames[i]->m_data->get_size();
  }

  int64_t timecode  = m_samples_output                                            * 1000000000ll / m_first_truehd_header.m_sampling_rate;
  int64_t duration  = m_frames.size() * m_first_truehd_header.m_samples_per_frame * 1000000000ll / m_first_truehd_header.m_sampling_rate;
  m_samples_output += m_frames.size() * m_first_truehd_header.m_samples_per_frame;

  add_packet(new packet_t(data, timecode, duration));

  m_frames.clear();
}

connection_result_e
truehd_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                 string &error_message) {
  truehd_packetizer_c *asrc = dynamic_cast<truehd_packetizer_c *>(src);
  if (NULL == asrc)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_a_samplerate(m_first_truehd_header.m_sampling_rate, asrc->m_first_truehd_header.m_sampling_rate);
  connect_check_a_channels(m_first_truehd_header.m_channels, asrc->m_first_truehd_header.m_channels);

  return CAN_CONNECT_YES;
}

