/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   AC3 output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/ac3.h"
#include "common/matroska.h"
#include "merge/output_control.h"
#include "merge/pr_generic.h"
#include "output/p_ac3.h"

using namespace libmatroska;

ac3_packetizer_c::ac3_packetizer_c(generic_reader_c *p_reader,
                                   track_info_c &p_ti,
                                   int samples_per_sec,
                                   int channels,
                                   int bsid)
  : generic_packetizer_c(p_reader, p_ti)
  , m_bytes_output(0)
  , m_packetno(0)
  , m_last_timecode(-1)
  , m_num_packets_same_tc(0)
  , m_s2tc(1536 * 1000000000ll, samples_per_sec)
  , m_single_packet_duration(1 * m_s2tc)
{
  m_first_ac3_header.m_sample_rate = samples_per_sec;
  m_first_ac3_header.m_bs_id       = bsid;
  m_first_ac3_header.m_channels    = channels;

  set_track_type(track_audio);
  set_track_default_duration(m_single_packet_duration);
  set_default_compression_method(COMPRESSION_AC3);
  enable_avi_audio_sync(true);
}

ac3_packetizer_c::~ac3_packetizer_c() {
}

ac3::frame_c
ac3_packetizer_c::get_frame() {
  ac3::frame_c frame = m_parser.get_frame();

  if (0 == frame.m_garbage_size)
    return frame;

  bool warning_printed = false;
  if (0 == m_packetno) {
    int64_t offset = handle_avi_audio_sync(frame.m_garbage_size, false);
    if (-1 != offset) {
      mxinfo_tid(m_ti.m_fname, m_ti.m_id,
                 boost::format(Y("This AC3 track contains %1% bytes of non-AC3 data at the beginning. "
                                 "This corresponds to a delay of %2%ms. "
                                 "This delay will be used instead of the non-AC3 data.\n"))
                 % frame.m_garbage_size % (offset / 1000000));

      warning_printed             = true;
      m_ti.m_tcsync.displacement += offset;
    }
  }

  if (!warning_printed)
    mxwarn_tid(m_ti.m_fname, m_ti.m_id,
               boost::format(Y("This AC3 track contains %1% bytes of non-AC3 data which were skipped. "
                               "The audio/video synchronization may have been lost.\n")) % frame.m_garbage_size);

  return frame;
}

void
ac3_packetizer_c::set_headers() {
  std::string id = MKV_A_AC3;

  if (m_first_ac3_header.is_eac3())
    id = MKV_A_EAC3;
  else if (9 == m_first_ac3_header.m_bs_id)
    id += "/BSID9";
  else if (10 == m_first_ac3_header.m_bs_id)
    id += "/BSID10";

  set_codec_id(id.c_str());
  set_audio_sampling_freq((float)m_first_ac3_header.m_sample_rate);
  set_audio_channels(m_first_ac3_header.m_channels);

  generic_packetizer_c::set_headers();
}

int
ac3_packetizer_c::process(packet_cptr packet) {
  if (-1 != packet->timecode)
    m_available_timecodes.push_back(std::make_pair(packet->timecode, m_parser.get_total_stream_position()));

  add_to_buffer(packet->data->get_buffer(), packet->data->get_size());
  flush_packets();

  return FILE_STATUS_MOREDATA;
}

void
ac3_packetizer_c::add_to_buffer(unsigned char *const buf,
                                int size) {
  m_parser.add_bytes(buf, size);
}

void
ac3_packetizer_c::flush() {
  m_parser.flush();
  flush_packets();
  generic_packetizer_c::flush();
}

void
ac3_packetizer_c::flush_packets() {
  while (m_parser.frame_available()) {
    ac3::frame_c frame = m_parser.get_frame();

    adjust_header_values(frame);

    int64_t new_timecode = calculate_timecode(frame.m_stream_position);
    add_packet(new packet_t(frame.m_data, new_timecode, m_single_packet_duration));
    ++m_packetno;
  }
}

int64_t
ac3_packetizer_c::calculate_timecode(uint64_t stream_position) {
  auto itr = m_available_timecodes.begin();
  while (   (itr             != m_available_timecodes.end())
         && (stream_position >= itr->second))
    itr++;

  if (m_available_timecodes.begin() == itr) {
    m_last_timecode       = -1 == m_last_timecode ? 0 : m_last_timecode + 1 * m_s2tc;
    m_num_packets_same_tc = 0;

    return m_last_timecode;
  }

  int64_t new_timecode = (itr - 1)->first;
  m_available_timecodes.erase(m_available_timecodes.begin(), itr);

  if (m_last_timecode == new_timecode) {
    m_num_packets_same_tc++;
    return new_timecode + m_num_packets_same_tc * m_s2tc;
  }

  m_last_timecode       = new_timecode;
  m_num_packets_same_tc = 0;

  return new_timecode;
}

void
ac3_packetizer_c::adjust_header_values(ac3::frame_c &ac3_header) {
  if (0 != m_packetno)
    return;

  m_first_ac3_header = ac3_header;

  if (m_first_ac3_header.is_eac3())
    set_codec_id(MKV_A_EAC3);

  if (1536 != m_first_ac3_header.m_samples) {
    m_s2tc.set(1000000000ll * m_first_ac3_header.m_samples, m_first_ac3_header.m_sample_rate);
    m_single_packet_duration = 1 * m_s2tc;
    set_track_default_duration(m_single_packet_duration);
  }

  rerender_track_headers();
}

connection_result_e
ac3_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                 std::string &error_message) {
  ac3_packetizer_c *asrc = dynamic_cast<ac3_packetizer_c *>(src);
  if (!asrc)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_a_samplerate(m_first_ac3_header.m_sample_rate, asrc->m_first_ac3_header.m_sample_rate);
  connect_check_a_channels(  m_first_ac3_header.m_channels,    asrc->m_first_ac3_header.m_channels);

  return CAN_CONNECT_YES;
}

ac3_bs_packetizer_c::ac3_bs_packetizer_c(generic_reader_c *p_reader,
                                         track_info_c &p_ti,
                                         unsigned long samples_per_sec,
                                         int channels,
                                         int bsid)
  : ac3_packetizer_c(p_reader, p_ti, samples_per_sec, channels, bsid)
  , m_bsb(0)
  , m_bsb_present(false)
{
}

static bool s_warning_printed = false;

void
ac3_bs_packetizer_c::add_to_buffer(unsigned char *const buf,
                                   int size) {
  if (((size % 2) == 1) && !s_warning_printed) {
    mxwarn(Y("ac3_bs_packetizer::add_to_buffer(): Untested code ('size' is odd). "
             "If mkvmerge crashes or if the resulting file does not contain the complete and correct audio track, "
             "then please contact the author Moritz Bunkus at moritz@bunkus.org.\n"));
    s_warning_printed = true;
  }

  unsigned char *sendptr;
  int size_add;
  bool new_bsb_present = false;

  if (m_bsb_present) {
    size_add = 1;
    sendptr  = buf + size + 1;
  } else {
    size_add = 0;
    sendptr  = buf + size;
  }

  size_add += size;
  if ((size_add % 2) == 1) {
    size_add--;
    sendptr--;
    new_bsb_present = true;
  }

  unsigned char *new_buffer = (unsigned char *)safemalloc(size_add);
  unsigned char *dptr       = new_buffer;
  unsigned char *sptr       = buf;

  if (m_bsb_present) {
    dptr[1]  = m_bsb;
    dptr[0]  = sptr[0];
    dptr    += 2;
    sptr++;
  }

  while (sptr < sendptr) {
    dptr[0]  = sptr[1];
    dptr[1]  = sptr[0];
    dptr    += 2;
    sptr    += 2;
  }

  if (new_bsb_present)
    m_bsb = *sptr;

  m_bsb_present = new_bsb_present;

  m_parser.add_bytes(new_buffer, size_add);

  safefree(new_buffer);
}
