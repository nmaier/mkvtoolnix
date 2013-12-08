/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Opus packetizer

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/matroska.h"
#include "common/opus.h"
#include "merge/connection_checks.h"
#include "output/p_opus.h"

using namespace libmatroska;

opus_packetizer_c::opus_packetizer_c(generic_reader_c *reader,
                                     track_info_c &ti)
  : generic_packetizer_c(reader, ti)
  , m_debug{"opus|opus_packetizer"}
  , m_next_calculated_timecode{timecode_c::ns(0)}
  , m_id_header(mtx::opus::id_header_t::decode(ti.m_private_data))
{
  mxdebug_if(m_debug, boost::format("ID header: %1%\n") % m_id_header);

  set_track_type(track_audio);
  set_codec_private(m_ti.m_private_data);
}

opus_packetizer_c::~opus_packetizer_c() {
}

void
opus_packetizer_c::set_headers() {
  set_codec_id((boost::format("%1%") % MKV_A_OPUS).str());

  set_track_seek_pre_roll(timecode_c::samples(m_id_header.pre_skip, 48000));
  set_codec_delay(timecode_c::ms(80));

  set_audio_sampling_freq(m_id_header.input_sample_rate);
  set_audio_channels(m_id_header.channels);

  generic_packetizer_c::set_headers();
}

int
opus_packetizer_c::process(packet_cptr packet) {
  try {
    auto toc = mtx::opus::toc_t::decode(packet->data);
    mxdebug_if(m_debug, boost::format("TOC: %1%\n") % toc);

    if (!packet->has_timecode() || (timecode_c::ns(packet->timecode) == m_previous_provided_timecode))
      packet->timecode             = m_next_calculated_timecode.to_ns();
    else
      m_previous_provided_timecode = timecode_c::ns(packet->timecode);

    packet->duration               = toc.packet_duration.to_ns();
    m_next_calculated_timecode     = timecode_c::ns(packet->timecode + packet->duration);

    add_packet(packet);

  } catch (mtx::opus::exception &ex) {
    mxdebug_if(m_debug, boost::format("Exception: %1%\n") % ex.what());
  }

  return FILE_STATUS_MOREDATA;
}

connection_result_e
opus_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                    std::string &error_message) {
  opus_packetizer_c *vsrc = dynamic_cast<opus_packetizer_c *>(src);
  if (!vsrc)
    return CAN_CONNECT_NO_FORMAT;

  connect_check_a_samplerate(m_id_header.input_sample_rate, vsrc->m_id_header.input_sample_rate);
  connect_check_a_channels(m_id_header.channels,            vsrc->m_id_header.channels);
  connect_check_codec_private(vsrc);

  return CAN_CONNECT_YES;
}

bool
opus_packetizer_c::is_compatible_with(output_compatibility_e compatibility) {
  // TODO: check for WebM compatibility
  return (OC_MATROSKA == compatibility); // || (OC_WEBM == compatibility);
}
