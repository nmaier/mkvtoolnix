/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   AAC output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/aac.h"
#include "common/hacks.h"
#include "common/matroska.h"
#include "output/p_aac.h"

using namespace libmatroska;

aac_packetizer_c::aac_packetizer_c(generic_reader_c *p_reader,
                                   track_info_c &p_ti,
                                   int id,
                                   int profile,
                                   int samples_per_sec,
                                   int channels,
                                   bool emphasis_present,
                                   bool headerless)
  : generic_packetizer_c(p_reader, p_ti)
  , m_bytes_output(0)
  , m_packetno(0)
  , m_last_timecode(-1)
  , m_num_packets_same_tc(0)
  , m_bytes_skipped(0)
  , m_samples_per_sec(samples_per_sec)
  , m_channels(channels)
  , m_id(id)
  , m_profile(profile)
  , m_headerless(headerless)
  , m_emphasis_present(emphasis_present)
  , m_s2tc(1024 * 1000000000ll, m_samples_per_sec)
  , m_single_packet_duration(1 * m_s2tc)
{
  set_track_type(track_audio);
  set_track_default_duration(m_single_packet_duration);
}

aac_packetizer_c::~aac_packetizer_c() {
}

unsigned char *
aac_packetizer_c::get_aac_packet(unsigned long *header,
                                 aac_header_t *aacheader) {
  unsigned char *packet_buffer = m_byte_buffer.get_buffer();
  int size                     = m_byte_buffer.get_size();
  int pos                      = find_aac_header(packet_buffer, size, aacheader, m_emphasis_present);

  if (0 > pos) {
    if (10 < size) {
      m_bytes_skipped += size - 10;
      m_byte_buffer.remove(size - 10);
    }
    return NULL;
  }
  if ((pos + aacheader->bytes) > size)
    return NULL;

  m_bytes_skipped += pos;
  if (verbose && (0 < m_bytes_skipped))
    mxwarn_tid(m_ti.m_fname, m_ti.m_id, boost::format(Y("Skipping %1% bytes (no valid AAC header found). This might cause audio/video desynchronisation.\n")) % m_bytes_skipped);
  m_bytes_skipped = 0;

  unsigned char *buf;
  if ((aacheader->header_bit_size % 8) == 0)
    buf = (unsigned char *)safememdup(packet_buffer + pos + aacheader->header_byte_size, aacheader->data_byte_size);
  else {
    // Header is not byte aligned, i.e. MPEG-4 ADTS
    // This code is from mpeg4ip/server/mp4creator/aac.cpp
    buf = (unsigned char *)safemalloc(aacheader->data_byte_size);

    int up_shift       = aacheader->header_bit_size % 8;
    int down_shift     = 8 - up_shift;
    unsigned char *src = packet_buffer + pos + aacheader->header_bit_size / 8;

    int i;
    buf[0] = src[0] << up_shift;
    for (i = 1; i < aacheader->data_byte_size; i++) {
      buf[i - 1] |= (src[i] >> down_shift);
      buf[i]      = (src[i] << up_shift);
    }
  }

  m_byte_buffer.remove(pos + aacheader->bytes);

  return buf;
}

void
aac_packetizer_c::set_headers() {
  if (!hack_engaged(ENGAGE_OLD_AAC_CODECID))
    set_codec_id(MKV_A_AAC);

  else if (AAC_ID_MPEG4 == m_id) {
    if (AAC_PROFILE_MAIN == m_profile)
      set_codec_id(MKV_A_AAC_4MAIN);
    else if (AAC_PROFILE_LC == m_profile)
      set_codec_id(MKV_A_AAC_4LC);
    else if (AAC_PROFILE_SSR == m_profile)
      set_codec_id(MKV_A_AAC_4SSR);
    else if (AAC_PROFILE_LTP == m_profile)
      set_codec_id(MKV_A_AAC_4LTP);
    else if (AAC_PROFILE_SBR == m_profile)
      set_codec_id(MKV_A_AAC_4SBR);
    else
      mxerror_tid(m_ti.m_fname, m_ti.m_id, boost::format(Y("Unknown AAC MPEG-4 object type %1%.")) % m_profile);

  } else {
    if (AAC_PROFILE_MAIN == m_profile)
      set_codec_id(MKV_A_AAC_2MAIN);
    else if (AAC_PROFILE_LC == m_profile)
      set_codec_id(MKV_A_AAC_2LC);
    else if (AAC_PROFILE_SSR == m_profile)
      set_codec_id(MKV_A_AAC_2SSR);
    else if (AAC_PROFILE_SBR == m_profile)
      set_codec_id(MKV_A_AAC_2SBR);
    else
      mxerror_tid(m_ti.m_fname, m_ti.m_id, boost::format(Y("Unknown AAC MPEG-2 profile %1%.")) % m_profile);
  }

  set_audio_sampling_freq((float)m_samples_per_sec);
  set_audio_channels(m_channels);

  if ((NULL != m_ti.m_private_data) && (0 < m_ti.m_private_size))
    set_codec_private(m_ti.m_private_data, m_ti.m_private_size);

  else if (!hack_engaged(ENGAGE_OLD_AAC_CODECID)) {
    unsigned char buffer[5];
    int length = create_aac_data(buffer,
                                 AAC_PROFILE_SBR == m_profile ? AAC_PROFILE_LC : m_profile,
                                 m_channels, m_samples_per_sec,
                                 AAC_PROFILE_SBR == m_profile ? m_samples_per_sec * 2 : m_samples_per_sec,
                                 AAC_PROFILE_SBR == m_profile);
    set_codec_private(buffer, length);
  }

  generic_packetizer_c::set_headers();
}

int
aac_packetizer_c::process_headerless(packet_cptr packet) {
  int64_t new_timecode;

  if (-1 != packet->timecode) {
    new_timecode = packet->timecode;
    if (m_last_timecode == packet->timecode) {
      m_num_packets_same_tc++;
      new_timecode += m_num_packets_same_tc * m_s2tc;

    } else {
      m_last_timecode       = packet->timecode;
      m_num_packets_same_tc = 0;
    }

  } else
    new_timecode = m_packetno * m_s2tc;

  m_packetno++;
  packet->duration = m_single_packet_duration;
  packet->timecode = new_timecode;

  add_packet(packet);

  return FILE_STATUS_MOREDATA;
}

int
aac_packetizer_c::process(packet_cptr packet) {
  if (m_headerless)
    return process_headerless(packet);

  unsigned char *aac_packet;
  unsigned long header;
  aac_header_t aacheader;

  m_byte_buffer.add(packet->data->get_buffer(), packet->data->get_size());
  while ((aac_packet = get_aac_packet(&header, &aacheader)) != NULL) {
    add_packet(new packet_t(new memory_c(aac_packet, aacheader.data_byte_size, true), -1 == packet->timecode ? m_packetno * m_s2tc : packet->timecode, m_single_packet_duration));
    m_packetno++;
  }

  return FILE_STATUS_MOREDATA;
}

connection_result_e
aac_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                 std::string &error_message) {
  aac_packetizer_c *asrc = dynamic_cast<aac_packetizer_c *>(src);
  if (NULL == asrc)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_a_samplerate(m_samples_per_sec, asrc->m_samples_per_sec);
  connect_check_a_channels(m_channels, asrc->m_channels);
  if (m_profile != asrc->m_profile) {
    error_message = (boost::format(Y("The AAC profiles are different: %1% and %2%")) % m_profile % asrc->m_profile).str();
    return CAN_CONNECT_NO_PARAMETERS;
  }

  return CAN_CONNECT_YES;
}
