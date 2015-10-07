/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Vorbis packetizer

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include "common/codec.h"
#include "merge/connection_checks.h"
#include "merge/output_control.h"
#include "output/p_vorbis.h"

using namespace libmatroska;

vorbis_packetizer_c::vorbis_packetizer_c(generic_reader_c *p_reader,
                                         track_info_c &p_ti,
                                         unsigned char *d_header,
                                         int l_header,
                                         unsigned char *d_comments,
                                         int l_comments,
                                         unsigned char *d_codecsetup,
                                         int l_codecsetup)
  : generic_packetizer_c(p_reader, p_ti)
  , m_previous_bs(0)
  , m_samples(0)
  , m_previous_samples_sum(0)
  , m_previous_timecode(0)
  , m_timecode_offset(0)
{
  m_headers.push_back(memory_cptr(new memory_c((unsigned char *)safememdup(d_header,     l_header),     l_header)));
  m_headers.push_back(memory_cptr(new memory_c((unsigned char *)safememdup(d_comments,   l_comments),   l_comments)));
  m_headers.push_back(memory_cptr(new memory_c((unsigned char *)safememdup(d_codecsetup, l_codecsetup), l_codecsetup)));

  ogg_packet ogg_headers[3];
  memset(ogg_headers, 0, 3 * sizeof(ogg_packet));
  ogg_headers[0].packet   = m_headers[0]->get_buffer();
  ogg_headers[1].packet   = m_headers[1]->get_buffer();
  ogg_headers[2].packet   = m_headers[2]->get_buffer();
  ogg_headers[0].bytes    = l_header;
  ogg_headers[1].bytes    = l_comments;
  ogg_headers[2].bytes    = l_codecsetup;
  ogg_headers[0].b_o_s    = 1;
  ogg_headers[1].packetno = 1;
  ogg_headers[2].packetno = 2;

  vorbis_info_init(&m_vi);
  vorbis_comment_init(&m_vc);

  int i;
  for (i = 0; 3 > i; ++i)
    if (vorbis_synthesis_headerin(&m_vi, &m_vc, &ogg_headers[i]) < 0)
      throw mtx::output::vorbis_x(Y("Error: vorbis_packetizer: Could not extract the stream's parameters from the first packets.\n"));

  set_track_type(track_audio);
  if (g_use_durations)
    set_track_default_duration((int64_t)(1024000000000.0 / m_vi.rate));
}

vorbis_packetizer_c::~vorbis_packetizer_c() {
  vorbis_info_clear(&m_vi);
  vorbis_comment_clear(&m_vc);
}

void
vorbis_packetizer_c::set_headers() {

  set_codec_id(MKV_A_VORBIS);

  set_codec_private(lace_memory_xiph(m_headers));

  set_audio_sampling_freq((float)m_vi.rate);
  set_audio_channels(m_vi.channels);

  generic_packetizer_c::set_headers();
}

int
vorbis_packetizer_c::process(packet_cptr packet) {
  ogg_packet op;

  // Remember the very first timecode we received.
  if ((0 == m_samples) && (0 < packet->timecode))
    m_timecode_offset = packet->timecode;

  // Update the number of samples we have processed so that we can
  // calculate the timecode on the next call.
  op.packet                  = packet->data->get_buffer();
  op.bytes                   = packet->data->get_size();
  int64_t this_bs            = vorbis_packet_blocksize(&m_vi, &op);
  int64_t samples_here       = (this_bs + m_previous_bs) / 4;
  m_previous_bs              = this_bs;
  m_samples                 += samples_here;

  int64_t expected_timecode  = m_previous_timecode + m_previous_samples_sum * 1000000000 / m_vi.rate + m_timecode_offset;
  int64_t chosen_timecode;

  if (packet->timecode > (expected_timecode + 100000000)) {
    chosen_timecode        = packet->timecode;
    packet->duration       = packet->timecode - (m_previous_timecode + m_previous_samples_sum * 1000000000 / m_vi.rate + m_timecode_offset);
    m_previous_timecode    = packet->timecode;
    m_previous_samples_sum = 0;

  } else {
    chosen_timecode  = expected_timecode;
    packet->duration = (int64_t)(samples_here * 1000000000 / m_vi.rate);
  }

  m_previous_samples_sum += samples_here;

  mxverb(2,
         boost::format("Vorbis: samples_here at %1% (orig %2% expected %3%): %4% (m_previous_samples_sum: %5%)\n")
         % chosen_timecode % packet->timecode % expected_timecode % samples_here % m_previous_samples_sum);
  packet->timecode = chosen_timecode;
  add_packet(packet);

  return FILE_STATUS_MOREDATA;
}

connection_result_e
vorbis_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                    std::string &error_message) {
  vorbis_packetizer_c *vsrc = dynamic_cast<vorbis_packetizer_c *>(src);
  if (!vsrc)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_a_samplerate(m_vi.rate,   vsrc->m_vi.rate);
  connect_check_a_channels(m_vi.channels, vsrc->m_vi.channels);

  if ((m_headers[2]->get_size() != vsrc->m_headers[2]->get_size()) || memcmp(m_headers[2]->get_buffer(), vsrc->m_headers[2]->get_buffer(), m_headers[2]->get_size())) {
    error_message = Y("The Vorbis codebooks are different; such tracks cannot be concatenated without reencoding");
    return CAN_CONNECT_NO_FORMAT;
  }

  return CAN_CONNECT_YES;
}

bool
vorbis_packetizer_c::is_compatible_with(output_compatibility_e compatibility) {
  return (OC_MATROSKA == compatibility) || (OC_WEBM == compatibility);
}
