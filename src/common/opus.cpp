/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper functions for Opus data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/endian.h"
#include "common/opus.h"

namespace mtx { namespace opus {

id_header_t
id_header_t::decode(memory_cptr const &mem) {
  if (!mem || (sizeof(packed_id_header_t) > mem->get_size()))
    throw decode_error{boost::format("Size of memory too small: %1% < %2%") % (mem ? mem->get_size() : 0) % sizeof(packed_id_header_t)};

  auto *packed_header = reinterpret_cast<packed_id_header_t *>(mem->get_buffer());
  auto identifier     = std::string{packed_header->identifier, 8};
  if (identifier != "OpusHead")
    throw decode_error{boost::format("Identifier not 'OpusHead': %1%") % identifier};

  auto header              = id_header_t();

  header.channels          = packed_header->channels;
  header.pre_skip          = get_uint16_le(&packed_header->pre_skip);
  header.input_sample_rate = get_uint32_le(&packed_header->input_sample_rate);
  header.output_gain       = get_uint16_le(&packed_header->output_gain);

  return header;
}

toc_t
toc_t::decode(memory_cptr const &mem) {
  static std::vector<timecode_c> s_frame_durations{
      timecode_c::ms(10),   timecode_c::ms(20), timecode_c::ms(40), timecode_c::ms(60)
    , timecode_c::ms(10),   timecode_c::ms(20), timecode_c::ms(40), timecode_c::ms(60)
    , timecode_c::ms(10),   timecode_c::ms(20), timecode_c::ms(40), timecode_c::ms(60)
    , timecode_c::ms(10),   timecode_c::ms(20)
    , timecode_c::ms(10),   timecode_c::ms(20)
    , timecode_c::us(2500), timecode_c::ms(5), timecode_c::ms(10), timecode_c::ms(20)
    , timecode_c::us(2500), timecode_c::ms(5), timecode_c::ms(10), timecode_c::ms(20)
    , timecode_c::us(2500), timecode_c::ms(5), timecode_c::ms(10), timecode_c::ms(20)
    , timecode_c::us(2500), timecode_c::ms(5), timecode_c::ms(10), timecode_c::ms(20)
  };

  if (!mem || !mem->get_size())
    throw decode_error{"Packet contains no data"};

  auto toc              = toc_t();
  auto *buf             = mem->get_buffer();

  toc.config            = buf[0] >> 3;
  toc.stereo            = !!(buf[0] & 0x04);
  auto frame_count_type = buf[0] & 0x03;

  if (0 == frame_count_type)
    toc.frame_count = 1;

  else if (3 != frame_count_type)
    toc.frame_count = 2;

  else if (2 > mem->get_size())
    throw decode_error{"Packet too small: 1 < 2"};

  else
    toc.frame_count = buf[1] & 0x3f;

  toc.frame_duration  = s_frame_durations[toc.config];
  toc.packet_duration = s_frame_durations[toc.config] * timecode_c::factor(toc.frame_count);

  return toc;
}

}}
