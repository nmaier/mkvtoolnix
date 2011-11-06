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
  , m_bytes_skipped(0)
  , m_last_timecode(-1)
  , m_num_packets_same_tc(0)
  , m_samples_per_sec(samples_per_sec)
  , m_first_packet(true)
  , m_s2tc(1536 * 1000000000ll, m_samples_per_sec)
  , m_single_packet_duration(1 * m_s2tc)
{
  memset(&m_first_ac3_header, 0, sizeof(ac3_header_t));
  m_first_ac3_header.bsid     = bsid;
  m_first_ac3_header.channels = channels;

  set_track_type(track_audio);
  set_track_default_duration(m_single_packet_duration);
  set_default_compression_method(COMPRESSION_AC3);
  enable_avi_audio_sync(true);
}

ac3_packetizer_c::~ac3_packetizer_c() {
}

void
ac3_packetizer_c::add_to_buffer(unsigned char *buf,
                                int size) {
  m_byte_buffer.add(buf, size);
}

unsigned char *
ac3_packetizer_c::get_ac3_packet(unsigned long *header,
                                 ac3_header_t *ac3header) {
  unsigned char *packet_buffer = m_byte_buffer.get_buffer();
  size_t size                  = m_byte_buffer.get_size();

  if (NULL == packet_buffer)
    return NULL;

  int pos = find_ac3_header(packet_buffer, size, ac3header, m_first_packet || m_first_ac3_header.has_dependent_frames);

  if ((0 > pos) || (static_cast<size_t>(pos + ac3header->bytes) > size))
    return NULL;

  m_bytes_skipped += pos;
  if (0 < m_bytes_skipped) {
    bool warning_printed = false;
    if (0 == m_packetno) {
      int64_t offset = handle_avi_audio_sync(m_bytes_skipped, false);
      if (-1 != offset) {
        mxinfo_tid(m_ti.m_fname, m_ti.m_id,
                   boost::format(Y("This AC3 track contains %1% bytes of non-AC3 data at the beginning. "
                                   "This corresponds to a delay of %2%ms. "
                                   "This delay will be used instead of the non-AC3 data.\n"))
                   % m_bytes_skipped % (offset / 1000000));

        warning_printed             = true;
        m_ti.m_tcsync.displacement += offset;
      }
    }

    if (!warning_printed)
      mxwarn_tid(m_ti.m_fname, m_ti.m_id,
                 boost::format(Y("This AC3 track contains %1% bytes of non-AC3 data which were skipped. "
                                 "The audio/video synchronization may have been lost.\n")) % m_bytes_skipped);

    m_byte_buffer.remove(pos);
    packet_buffer   = m_byte_buffer.get_buffer();
    size            = m_byte_buffer.get_size();
    pos             = 0;
    m_bytes_skipped = 0;
  }

  unsigned char *buf = (unsigned char *)safememdup(packet_buffer, ac3header->bytes);

  m_byte_buffer.remove(ac3header->bytes);

  return buf;
}

void
ac3_packetizer_c::set_headers() {
  std::string id = MKV_A_AC3;

  if (9 == m_first_ac3_header.bsid)
    id += "/BSID9";
  else if (10 == m_first_ac3_header.bsid)
    id += "/BSID10";
  else if (16 == m_first_ac3_header.bsid)
    id = MKV_A_EAC3;

  set_codec_id(id.c_str());
  set_audio_sampling_freq((float)m_samples_per_sec);
  set_audio_channels(m_first_ac3_header.channels);

  generic_packetizer_c::set_headers();
}

int
ac3_packetizer_c::process(packet_cptr packet) {
  unsigned char *ac3_packet;
  unsigned long header;
  ac3_header_t ac3header;

  add_to_buffer(packet->data->get_buffer(), packet->data->get_size());
  while ((ac3_packet = get_ac3_packet(&header, &ac3header)) != NULL) {
    adjust_header_values(ac3header);

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

    add_packet(new packet_t(new memory_c(ac3_packet, ac3header.bytes, true), new_timecode, m_single_packet_duration));
    m_packetno++;
  }

  return FILE_STATUS_MOREDATA;
}

void
ac3_packetizer_c::adjust_header_values(ac3_header_t &ac3_header) {
  if (!m_first_packet)
    return;

  m_first_packet = false;

  memcpy(&m_first_ac3_header, &ac3_header, sizeof(ac3_header_t));

  if (16 == m_first_ac3_header.bsid)
    set_codec_id(MKV_A_EAC3);

  if (1536 != m_first_ac3_header.samples) {
    m_s2tc.set(1000000000ll * m_first_ac3_header.samples, m_samples_per_sec);
    m_single_packet_duration = 1 * m_s2tc;
    set_track_default_duration(m_single_packet_duration);
  }

  rerender_track_headers();
}

connection_result_e
ac3_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                 std::string &error_message) {
  ac3_packetizer_c *asrc = dynamic_cast<ac3_packetizer_c *>(src);
  if (NULL == asrc)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_a_samplerate(m_samples_per_sec, asrc->m_samples_per_sec);
  connect_check_a_channels(m_first_ac3_header.channels, asrc->m_first_ac3_header.channels);

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
ac3_bs_packetizer_c::add_to_buffer(unsigned char *buf,
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

  m_byte_buffer.add(new_buffer, size_add);

  safefree(new_buffer);
}
