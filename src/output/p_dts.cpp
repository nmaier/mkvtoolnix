/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   DTS output module

   Written by Peter Niemayer <niemayer@isg.de>.
   Modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/dts.h"
#include "common/matroska.h"
#include "merge/connection_checks.h"
#include "output/p_dts.h"

using namespace libmatroska;

dts_packetizer_c::dts_packetizer_c(generic_reader_c *p_reader,
                                   track_info_c &p_ti,
                                   const dts_header_t &dtsheader)
  : generic_packetizer_c(p_reader, p_ti)
  , m_samples_written(0)
  , m_bytes_written(0)
  , m_packet_buffer(128 * 1024)
  , m_first_header(dtsheader)
  , m_previous_header(dtsheader)
  , m_skipping_is_normal(false)
{
  set_track_type(track_audio);
}

dts_packetizer_c::~dts_packetizer_c() {
}

unsigned char *
dts_packetizer_c::get_dts_packet(dts_header_t &dtsheader,
                                 bool flushing) {
  if (0 == m_packet_buffer.get_size())
    return nullptr;

  const unsigned char *buf = m_packet_buffer.get_buffer();
  int buf_size             = m_packet_buffer.get_size();
  int pos                  = find_dts_sync_word(buf, buf_size);

  if (0 > pos) {
    if (4 < buf_size)
      m_packet_buffer.remove(buf_size - 4);
    return nullptr;
  }

  if (0 < pos) {
    m_packet_buffer.remove(pos);
    buf      = m_packet_buffer.get_buffer();
    buf_size = m_packet_buffer.get_size();
  }

  pos = find_dts_header(buf, buf_size, &dtsheader, flushing);

  if ((0 > pos) || (static_cast<int>(pos + dtsheader.frame_byte_size) > buf_size))
    return nullptr;

  if ((1 < verbose) && (dtsheader != m_previous_header)) {
    mxinfo(Y("DTS header information changed! - New format:\n"));
    print_dts_header(&dtsheader);
    m_previous_header = dtsheader;
  }

  if (verbose && (0 < pos) && !m_skipping_is_normal) {
    int i;
    bool all_zeroes = true;

    for (i = 0; i < pos; ++i)
      if (buf[i]) {
        all_zeroes = false;
        break;
      }

    if (!all_zeroes)
      mxwarn_tid(m_ti.m_fname, m_ti.m_id, boost::format(Y("Skipping %1% bytes (no valid DTS header found). This might cause audio/video desynchronisation.\n")) % pos);
  }

  unsigned char *packet_buf = (unsigned char *)safememdup(buf + pos, dtsheader.frame_byte_size);

  m_packet_buffer.remove(pos + dtsheader.frame_byte_size);

  return packet_buf;
}

void
dts_packetizer_c::set_headers() {
  set_codec_id(MKV_A_DTS);
  set_audio_sampling_freq((float)m_first_header.core_sampling_frequency);
  if (   (dts_header_t::LFE_64  == m_first_header.lfe_type)
      || (dts_header_t::LFE_128 == m_first_header.lfe_type))
    set_audio_channels(m_first_header.audio_channels + 1);
  else
    set_audio_channels(m_first_header.audio_channels);

  set_track_default_duration(get_dts_packet_length_in_nanoseconds(&m_first_header));

  generic_packetizer_c::set_headers();
}

int
dts_packetizer_c::process(packet_cptr packet) {
  if (-1 != packet->timecode)
    m_available_timecodes.push_back(packet->timecode);

  m_packet_buffer.add(packet->data->get_buffer(), packet->data->get_size());

  process_available_packets(false);

  return FILE_STATUS_MOREDATA;
}

void
dts_packetizer_c::process_available_packets(bool flushing) {
  dts_header_t dtsheader;
  unsigned char *dts_packet;

  while ((dts_packet = get_dts_packet(dtsheader, flushing))) {
    int64_t new_timecode;
    if (!m_available_timecodes.empty()) {
      m_samples_written = 0;
      new_timecode      = m_available_timecodes.front();
      m_available_timecodes.pop_front();

    } else
      new_timecode = static_cast<int64_t>(m_samples_written * 1000000000.0 / static_cast<double>(dtsheader.core_sampling_frequency));

    add_packet(new packet_t(new memory_c(dts_packet, dtsheader.frame_byte_size, true), new_timecode, (int64_t)get_dts_packet_length_in_nanoseconds(&dtsheader)));

    m_bytes_written   += dtsheader.frame_byte_size;
    m_samples_written += get_dts_packet_length_in_core_samples(&dtsheader);
  }
}

void
dts_packetizer_c::flush_impl() {
  process_available_packets(true);
}

connection_result_e
dts_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                 std::string &error_message) {
  dts_packetizer_c *dsrc = dynamic_cast<dts_packetizer_c *>(src);
  if (!dsrc)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_a_samplerate(m_first_header.core_sampling_frequency, dsrc->m_first_header.core_sampling_frequency);
  connect_check_a_channels(m_first_header.audio_channels, dsrc->m_first_header.audio_channels);

  return CAN_CONNECT_YES;
}
