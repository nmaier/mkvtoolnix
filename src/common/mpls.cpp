/*
  mkvmerge -- utility for splicing together matroska files
  from component media subtypes

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html

  definitions and helper functions for BluRay playlist files (MPLS)

  Written by Moritz Bunkus <moritz@bunkus.org>.
*/
#include "common/common_pch.h"

#include <vector>

#include "common/debugging.h"
#include "common/mpls.h"
#include "common/strings/formatting.h"

namespace mtx { namespace mpls {

static timecode_c
mpls_time_to_timecode(uint64_t value) {
  return timecode_c::ns(value * 1000000ull / 45);
}

void
header_t::dump()
  const {
  mxinfo(boost::format("  header dump\n"
                       "    type_indicator1 & 2:          %1% / %2%\n"
                       "    playlist / chapter / ext pos: %3% / %4% / %5%\n")
         % type_indicator1 % type_indicator2
         % playlist_pos % chapter_pos % ext_pos);
}

void
playlist_t::dump()
  const {
  mxinfo(boost::format("  playlist dump\n"
                       "    list_count / sub_count: %1% / %2%\n"
                       "    duration:               %3%\n")
         % list_count % sub_count
         % duration);

  for (auto &item : items)
    item.dump();
}

void
play_item_t::dump()
  const {
  mxinfo(boost::format("    play item dump\n"
                       "      clip_id / codec_id:      %1% / %2%\n"
                       "      connection_condition:    %3%\n"
                       "      is_multi_angle / stc_id: %4% / %5%\n"
                       "      in_time / out_time:      %6% / %7%\n"
                       "      relative_in_time:        %8%\n")
         % clip_id % codec_id
         % connection_condition
         % is_multi_angle % stc_id
         % in_time % out_time
         % relative_in_time);

  stn.dump();
}

void
stn_t::dump()
  const {
  mxinfo(boost::format("      stn dump\n"
                       "        num_video / num_audio / num_pg:             %1% / %2% / %3%\n"
                       "        num_sec_video / num_sec_audio / num_pip_pg: %4% / %5% / %6%\n")
         % num_video % num_audio % num_pip_pg
         % num_secondary_video % num_secondary_audio % num_pip_pg);

  for (auto &stream : video_streams)
    stream.dump("video");

  for (auto &stream : audio_streams)
    stream.dump("audio");

  for (auto &stream : pg_streams)
    stream.dump("pg");
}

void
stream_t::dump(std::string const &type)
  const {
  mxinfo(boost::format("        %1% stream dump\n"
                       "          stream_type:                     %2%\n"
                       "          sub_path_id / sub_clip_id / pid: %3% / %4% / %|5$04x|\n"
                       "          coding_type:                     %|6$04x|\n"
                       "          format / rate:                   %7% / %8%\n"
                       "          char_code / language:            %9% / %10%\n")
         % type
         % stream_type
         % sub_path_id % sub_clip_id % pid
         % coding_type
         % format % rate
         % char_code % language);
}

parser_c::parser_c()
  : m_ok{}
  , m_debug{debugging_requested("mpls")}
  , m_header(header_t())
{
}

parser_c::~parser_c() {
}

bool
parser_c::parse(mm_io_c *file) {
  try {
    file->setFilePointer(0);
    int64_t file_size = file->get_size();
    auto content      = file->read(file_size);
    file->setFilePointer(0);

    m_bc = std::make_shared<bit_cursor_c>(content->get_buffer(), file_size);

    parse_header();
    parse_playlist();
    parse_chapters();

    m_bc.reset();

    m_ok = true;

  } catch (mtx::mpls::exception &ex) {
    mxdebug_if(m_debug, boost::format("MPLS exception: %1%\n") % ex.what());
  } catch (mtx::mm_io::exception &ex) {
    mxdebug_if(m_debug, boost::format("I/O exception: %1%\n") % ex.what());
  }

  if (m_debug)
    dump();

  return m_ok;
}

void
parser_c::parse_header() {
  m_header.type_indicator1 = fourcc_c{static_cast<uint32_t>(m_bc->get_bits(32))};
  m_header.type_indicator2 = fourcc_c{static_cast<uint32_t>(m_bc->get_bits(32))};
  m_header.playlist_pos    = m_bc->get_bits(32);
  m_header.chapter_pos     = m_bc->get_bits(32);
  m_header.ext_pos         = m_bc->get_bits(32);

  if (   (m_header.type_indicator1 != MPLS_FILE_MAGIC)
      || (   (m_header.type_indicator2 != MPLS_FILE_MAGIC2A)
          && (m_header.type_indicator2 != MPLS_FILE_MAGIC2B)))
    throw exception{boost::format("Wrong type indicator 1 (%1%) or 2 (%2%)") % m_header.type_indicator1 % m_header.type_indicator2};
}

void
parser_c::parse_playlist() {
  m_playlist.duration = timecode_c::ns(0);

  m_bc->set_bit_position(m_header.playlist_pos * 8);
  m_bc->skip_bits(32 + 16);     // playlist length, reserved bytes

  m_playlist.list_count = m_bc->get_bits(16);
  m_playlist.sub_count  = m_bc->get_bits(16);

  m_playlist.items.reserve(m_playlist.list_count);

  for (auto idx = 0u; idx < m_playlist.list_count; ++idx)
    m_playlist.items.push_back(parse_play_item());
}

play_item_t
parser_c::parse_play_item() {
  auto item     = play_item_t();

  auto length   = m_bc->get_bits(16);
  auto position = m_bc->get_bit_position() / 8u;

  item.clip_id  = read_string(5);
  item.codec_id = read_string(4);

  m_bc->skip_bits(11);          // reserved

  item.is_multi_angle       = m_bc->get_bit();
  item.connection_condition = m_bc->get_bits(4);
  item.stc_id               = m_bc->get_bits(8);
  item.in_time              = mpls_time_to_timecode(m_bc->get_bits(32) & 0x7ffffffful);
  item.out_time             = mpls_time_to_timecode(m_bc->get_bits(32) & 0x7ffffffful);
  item.relative_in_time     = m_playlist.duration;
  m_playlist.duration      += item.out_time - item.in_time;

  m_bc->skip_bits(12 * 8);      // UO_mask_table, random_access_flag, reserved, still_mode

  if (item.is_multi_angle) {
    unsigned int num_angles = m_bc->get_bits(8);
    m_bc->skip_bits(8);         // reserved, is_differend_audio, is_seamless_angle_change

    if (0 < num_angles)
      m_bc->skip_bits((num_angles - 1) * 10 * 8); // clip_id, clip_codec_id, stc_id
  }

  m_bc->skip_bits(16 + 16);     // STN length, reserved

  item.stn = parse_stn();

  m_bc->set_bit_position((position + length) * 8);

  return item;
}

stn_t
parser_c::parse_stn() {
  auto stn = stn_t();

  stn.num_video           = m_bc->get_bits(8);
  stn.num_audio           = m_bc->get_bits(8);
  stn.num_pg              = m_bc->get_bits(8);
  stn.num_ig              = m_bc->get_bits(8);
  stn.num_secondary_audio = m_bc->get_bits(8);
  stn.num_secondary_video = m_bc->get_bits(8);
  stn.num_pip_pg          = m_bc->get_bits(8);

  m_bc->skip_bits(5 * 8);       // reserved

  stn.video_streams.reserve(stn.num_video);
  stn.audio_streams.reserve(stn.num_audio);
  stn.pg_streams.reserve(stn.num_pg);

  for (auto idx = 0u; idx < stn.num_video; ++idx)
    stn.video_streams.push_back(parse_stream());

  for (auto idx = 0u; idx < stn.num_audio; ++idx)
    stn.audio_streams.push_back(parse_stream());

  for (auto idx = 0u; idx < stn.num_pg; ++idx)
    stn.pg_streams.push_back(parse_stream());

  return stn;
}

stream_t
parser_c::parse_stream() {
  auto str        = stream_t();

  auto length     = m_bc->get_bits(8);
  auto position   = m_bc->get_bit_position() / 8u;

  str.stream_type = m_bc->get_bits(8);

  if (1 == str.stream_type)
    str.pid = m_bc->get_bits(16);

  else if ((2 == str.stream_type) || (4 == str.stream_type)) {
    str.sub_path_id = m_bc->get_bits(8);
    str.sub_clip_id = m_bc->get_bits(8);
    str.pid         = m_bc->get_bits(16);

  } else if (3 == str.stream_type) {
    str.sub_path_id = m_bc->get_bits(8);
    str.pid         = m_bc->get_bits(16);

  } else if (m_debug)
    mxdebug(boost::format("Unknown stream type %1%\n") % str.stream_type);

  m_bc->set_bit_position((length + position) * 8);

  length          = m_bc->get_bits(8);
  position        = m_bc->get_bit_position() / 8u;

  str.coding_type = m_bc->get_bits(8);

  if ((0x01 == str.coding_type) || (0x02 == str.coding_type) || (0x1b == str.coding_type) || (0xea == str.coding_type)) {
    str.format = m_bc->get_bits(4);
    str.rate   = m_bc->get_bits(4);

  } else if (   (0x03 == str.coding_type) || (0x04 == str.coding_type) || (0x80 == str.coding_type) || (0x81 == str.coding_type) || (0x82 == str.coding_type)
             || (0x83 == str.coding_type) || (0x84 == str.coding_type) || (0x85 == str.coding_type) || (0x86 == str.coding_type)) {
    str.format   = m_bc->get_bits(4);
    str.rate     = m_bc->get_bits(4);
    str.language = read_string(3);

  } else if ((0x90 == str.coding_type) || (0x91 == str.coding_type))
    str.language = read_string(3);

  else if (0x92 == str.coding_type) {
    str.char_code = m_bc->get_bits(8);
    str.language  = read_string(3);

  } else
    mxdebug_if(m_debug, boost::format("Unrecognized coding type %|1$02x|\n") % str.coding_type);

  m_bc->set_bit_position((position + length) * 8);

  return str;
}

void
parser_c::parse_chapters() {
  m_bc->set_bit_position(m_header.chapter_pos * 8);
  m_bc->skip_bits(32);          // unknown
  size_t num_chapters = m_bc->get_bits(16);

  // 0 unknown
  // 1 type
  // 2, 3 stream_file_index
  // 4, 5, 6, 7 chapter_time
  // 8 - 13 unknown

  for (size_t idx = 0u; idx < num_chapters; ++idx) {
    m_bc->set_bit_position((m_header.chapter_pos + 4 + 2 + idx * 14) * 8);
    m_bc->skip_bits(8);         // unknown
    if (1 != m_bc->get_bits(8)) // chapter type: entry mark
      continue;

    size_t play_item_idx = m_bc->get_bits(16);
    if (play_item_idx >= m_playlist.items.size())
      continue;

    auto &play_item = m_playlist.items[play_item_idx];
    auto timecode   = mpls_time_to_timecode(m_bc->get_bits(32));
    m_chapters.push_back(timecode - play_item.in_time + play_item.relative_in_time);
  }
}

std::string
parser_c::read_string(unsigned int length) {
  std::string str(length, ' ');
  m_bc->get_bytes(reinterpret_cast<unsigned char *>(&str[0]), length);

  return str;
}

void
parser_c::dump()
  const {
  mxinfo(boost::format("MPLS class dump\n"
                       "  ok:           %1%\n"
                       "  num_chapters: %2%\n")
         % m_ok % m_chapters.size());
  for (auto &timecode : m_chapters)
    mxinfo(boost::format("    %1%\n") % timecode);

  m_header.dump();
  m_playlist.dump();
}

}}
