/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   ALAC output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/alac.h"
#include "common/hacks.h"
#include "common/matroska.h"
#include "output/p_alac.h"

using namespace libmatroska;

alac_packetizer_c::alac_packetizer_c(generic_reader_c *p_reader,
                                     track_info_c &p_ti,
                                     memory_cptr const &magic_cookie,
                                     unsigned int sample_rate,
                                     unsigned int channels)
  : generic_packetizer_c{p_reader, p_ti}
  , m_magic_cookie{magic_cookie}
  , m_sample_rate{sample_rate}
  , m_channels{channels}
{
  set_codec_id(MKV_A_ALAC);

  set_track_type(track_audio);

  set_audio_sampling_freq(static_cast<double>(m_sample_rate));
  set_audio_channels(m_channels);

  set_codec_private(magic_cookie->get_buffer(), magic_cookie->get_size());
}

alac_packetizer_c::~alac_packetizer_c() {
}

int
alac_packetizer_c::process(packet_cptr packet) {
  add_packet(packet);
  return FILE_STATUS_MOREDATA;
}

connection_result_e
alac_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                  std::string &error_message) {
  alac_packetizer_c *alac_src = dynamic_cast<alac_packetizer_c *>(src);
  if (!alac_src)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_a_samplerate(m_sample_rate, alac_src->m_sample_rate);
  connect_check_a_channels(  m_channels,    alac_src->m_channels);

  if (!m_magic_cookie || !alac_src->m_magic_cookie || (*m_magic_cookie != *alac_src->m_magic_cookie)) {
    error_message = (boost::format(Y("The ALAC header data is different for the two tracks (lengths: %1% and %2%)"))
                     % (          m_magic_cookie ?           m_magic_cookie->get_size() : 0)
                     % (alac_src->m_magic_cookie ? alac_src->m_magic_cookie->get_size() : 0)).str();
    return CAN_CONNECT_MAYBE_CODECPRIVATE;
  }

  return CAN_CONNECT_YES;
}
