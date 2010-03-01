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

#include "common/os.h"

#include "common/dts.h"
#include "common/matroska.h"
#include "merge/output_control.h"
#include "merge/pr_generic.h"
#include "output/p_dts.h"

using namespace libmatroska;

dts_packetizer_c::dts_packetizer_c(generic_reader_c *p_reader,
                                   track_info_c &p_ti,
                                   const dts_header_t &dtsheader,
                                   bool get_first_header_later)
  throw (error_c)
  : generic_packetizer_c(p_reader, p_ti)
  , m_samples_written(0)
  , m_bytes_written(0)
  , m_packet_buffer(NULL)
  , m_buffer_size(0)
  , m_get_first_header_later(get_first_header_later)
  , m_first_header(dtsheader)
  , m_previous_header(dtsheader)
  , m_skipping_is_normal(false)
{
  set_track_type(track_audio);
}

dts_packetizer_c::~dts_packetizer_c() {
  safefree(m_packet_buffer);
}

void
dts_packetizer_c::add_to_buffer(unsigned char *buf,
                                int size) {
  unsigned char *new_buffer = (unsigned char *)saferealloc(m_packet_buffer, m_buffer_size + size);

  memcpy(new_buffer + m_buffer_size, buf, size);
  m_packet_buffer  = new_buffer;
  m_buffer_size   += size;
}

bool
dts_packetizer_c::dts_packet_available() {
  if (NULL == m_packet_buffer)
    return false;

  dts_header_t dtsheader;
  int pos = find_dts_header(m_packet_buffer, m_buffer_size, &dtsheader, m_get_first_header_later ? false : !m_first_header.dts_hd);

  return 0 <= pos;
}

void
dts_packetizer_c::remove_dts_packet(int pos,
                                    int framesize) {
  unsigned char *temp_buf;

  int new_size = m_buffer_size - (pos + framesize);
  if (0 != new_size)
    temp_buf = (unsigned char *)safememdup(&m_packet_buffer[pos + framesize], new_size);
  else
    temp_buf = NULL;
  safefree(m_packet_buffer);
  m_packet_buffer = temp_buf;
  m_buffer_size   = new_size;
}

unsigned char *
dts_packetizer_c::get_dts_packet(dts_header_t &dtsheader) {
  if (NULL == m_packet_buffer)
    return NULL;

  int pos = find_dts_header(m_packet_buffer, m_buffer_size, &dtsheader, m_get_first_header_later ? false : !m_first_header.dts_hd);
  if (0 > pos)
    return NULL;

  if ((pos + dtsheader.frame_byte_size) > m_buffer_size)
    return NULL;

  if (m_get_first_header_later) {
    m_first_header           = dtsheader;
    m_previous_header        = dtsheader;
    m_get_first_header_later = false;

    if (!m_reader->m_appending)
      set_headers();

    rerender_track_headers();
  }

  if ((1 < verbose) && (dtsheader != m_previous_header)) {
    mxinfo(Y("DTS header information changed! - New format:\n"));
    print_dts_header(&dtsheader);
    m_previous_header = dtsheader;
  }

  if (verbose && (0 < pos) && !m_skipping_is_normal) {
    int i;
    bool all_zeroes = true;

    for (i = 0; i < pos; ++i)
      if (m_packet_buffer[i]) {
        all_zeroes = false;
        break;
      }

    if (!all_zeroes)
      mxwarn_tid(m_ti.m_fname, m_ti.m_id, boost::format(Y("Skipping %1% bytes (no valid DTS header found). This might cause audio/video desynchronisation.\n")) % pos);
  }

  unsigned char *buf = (unsigned char *)safememdup(m_packet_buffer + pos, dtsheader.frame_byte_size);

  remove_dts_packet(pos, dtsheader.frame_byte_size);

  return buf;
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

  generic_packetizer_c::set_headers();
}

int
dts_packetizer_c::process(packet_cptr packet) {
  dts_header_t dtsheader;
  unsigned char *dts_packet;

  add_to_buffer(packet->data->get_buffer(), packet->data->get_size());
  while ((dts_packet = get_dts_packet(dtsheader)) != NULL) {
    int64_t new_timecode = -1 == packet->timecode ? (int64_t)(((double)m_samples_written * 1000000000.0) / ((double)dtsheader.core_sampling_frequency)) : packet->timecode;

    add_packet(new packet_t(new memory_c(dts_packet, dtsheader.frame_byte_size, true), new_timecode, (int64_t)get_dts_packet_length_in_nanoseconds(&dtsheader)));

    m_bytes_written   += dtsheader.frame_byte_size;
    m_samples_written += get_dts_packet_length_in_core_samples(&dtsheader);
  }

  return FILE_STATUS_MOREDATA;
}

connection_result_e
dts_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                 std::string &error_message) {
  dts_packetizer_c *dsrc = dynamic_cast<dts_packetizer_c *>(src);
  if (NULL == dsrc)
    return CAN_CONNECT_NO_FORMAT;

  if (m_get_first_header_later)
    return CAN_CONNECT_MAYBE_CODECPRIVATE;

  connect_check_a_samplerate(m_first_header.core_sampling_frequency, dsrc->m_first_header.core_sampling_frequency);
  connect_check_a_channels(m_first_header.audio_channels, dsrc->m_first_header.audio_channels);

  return CAN_CONNECT_YES;
}
