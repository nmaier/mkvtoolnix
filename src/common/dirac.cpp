/** MPEG video helper functions (MPEG 1, 2 and 4)

   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <string.h>

#include "common/bit_cursor.h"
#include "common/dirac.h"
#include "common/endian.h"

dirac::sequence_header_t::sequence_header_t() {
  memset(this, 0, sizeof(dirac::sequence_header_t));
}

dirac::frame_t::frame_t()
  : timecode(-1)
  , duration(0)
  , contains_sequence_header(false)
{
}

void
dirac::frame_t::init() {
  data                     = memory_cptr(NULL);
  timecode                 = -1;
  duration                 = 0;
  contains_sequence_header = false;
}

static unsigned int
read_uint(bit_cursor_c &bc) {
  int count          = 0;
  unsigned int value = 0;

  while (!bc.get_bit()) {
    ++count;
    value <<= 1;
    value  |= bc.get_bit();
  }

  return (1 << count) - 1 + value;
}

bool
dirac::parse_sequence_header(const unsigned char *buf,
                             int size,
                             dirac::sequence_header_t &seqhdr) {
  struct standard_video_format {
    unsigned int pixel_width;
    unsigned int pixel_height;
    bool         interlaced;
    bool         top_field_first;
    unsigned int frame_rate_numerator;
    unsigned int frame_rate_denominator;
    unsigned int aspect_ratio_numerator;
    unsigned int aspect_ratio_denominator;
    unsigned int clean_width;
    unsigned int clean_height;
    unsigned int left_offset;
    unsigned int top_offset;
  };

  static const standard_video_format standard_video_formats[17] = {
    {  640,  480, false, false, 24000, 1001,  1,  1,  640,  480, 0, 0, },
    {  176,  120, false, false, 15000, 1001, 10, 11,  176,  120, 0, 0, },
    {  176,  144, false,  true,    25,    2, 12, 11,  176,  144, 0, 0, },
    {  352,  240, false, false, 15000, 1001, 10, 11,  352,  240, 0, 0, },
    {  352,  288, false,  true,    25,    2, 12, 11,  352,  288, 0, 0, },
    {  704,  480, false, false, 15000, 1001, 10, 11,  704,  480, 0, 0, },
    {  704,  576, false,  true,    25,    2, 12, 11,  704,  576, 0, 0, },
    {  720,  480,  true, false, 30000, 1001, 10, 11,  704,  480, 8, 0, },
    {  720,  576,  true,  true,    25,    1, 12, 11,  704,  576, 8, 0, },
    { 1280,  720, false,  true, 60000, 1001,  1,  1, 1280,  720, 0, 0, },
    { 1280,  720, false,  true,    50,    1,  1,  1, 1280,  720, 0, 0, },
    { 1920, 1080,  true,  true, 30000, 1001,  1,  1, 1920, 1080, 0, 0, },
    { 1920, 1080,  true,  true,    25,    1,  1,  1, 1920, 1080, 0, 0, },
    { 1920, 1080, false,  true, 60000, 1001,  1,  1, 1920, 1080, 0, 0, },
    { 1920, 1080, false,  true,    50,    1,  1,  1, 1920, 1080, 0, 0, },
    { 2048, 1080, false,  true,    24,    1,  1,  1, 2048, 1080, 0, 0, },
    { 4096, 2160, false,  true,    24,    1,  1,  1, 2048, 1536, 0, 0, },
  };

  static const struct { int numerator, denominator; } standard_frame_rates[11] = {
    {     0,    0 },
    { 24000, 1001 },
    {    24,    1 },
    {    25,    1 },
    { 30000, 1001 },
    {    30,    1 },
    {    50,    1 },
    { 60000, 1001 },
    {    60,    1 },
    { 15000, 1001 },
    {    25,    2 },
  };

  static const struct { int numerator, denominator; } standard_aspect_ratios[7] = {
    {  0,  0 },
    {  1,  1 },
    { 10, 11 },
    { 12, 11 },
    { 40, 33 },
    { 16, 11 },
    {  4,  3 },
  };

  try {
    bit_cursor_c bc(buf, size);
    dirac::sequence_header_t hdr;

    bc.skip_bits((4 + 1 + 4 + 4) * 8); // Marker, type, next offset, previous offset

    hdr.major_version     = read_uint(bc);
    hdr.minor_version     = read_uint(bc);
    hdr.profile           = read_uint(bc);
    hdr.level             = read_uint(bc);

    hdr.base_video_format = read_uint(bc);
    if (17 < hdr.base_video_format)
      hdr.base_video_format = 0;

    const standard_video_format &svf = standard_video_formats[hdr.base_video_format];

    hdr.pixel_width                  = svf.pixel_width;
    hdr.pixel_height                 = svf.pixel_height;
    hdr.interlaced                   = svf.interlaced;
    hdr.top_field_first              = svf.top_field_first;
    hdr.frame_rate_numerator         = svf.frame_rate_numerator;
    hdr.frame_rate_denominator       = svf.frame_rate_denominator;
    hdr.aspect_ratio_numerator       = svf.aspect_ratio_numerator;
    hdr.aspect_ratio_denominator     = svf.aspect_ratio_denominator;
    hdr.clean_width                  = svf.clean_width;
    hdr.clean_height                 = svf.clean_height;
    hdr.left_offset                  = svf.left_offset;
    hdr.top_offset                   = svf.top_offset;

    if (bc.get_bit()) {
      hdr.pixel_width  = read_uint(bc);
      hdr.pixel_height = read_uint(bc);
    }

    if (bc.get_bit())
      hdr.chroma_format = read_uint(bc);

    if (bc.get_bit()) {
      hdr.interlaced = bc.get_bit();
      if (hdr.interlaced)
        hdr.top_field_first = bc.get_bit();
    }

    if (bc.get_bit()) {
      int index = read_uint(bc);
      if (0 == index) {
        hdr.frame_rate_numerator   = read_uint(bc);
        hdr.frame_rate_denominator = read_uint(bc);
      } else if (11 >= index) {
        hdr.frame_rate_numerator   = standard_frame_rates[index].numerator;
        hdr.frame_rate_denominator = standard_frame_rates[index].denominator;
      }
    }

    if (bc.get_bit()) {
      int index = read_uint(bc);
      if (0 == index) {
        hdr.aspect_ratio_numerator   = read_uint(bc);
        hdr.aspect_ratio_denominator = read_uint(bc);
      } else if (7 >= index) {
        hdr.aspect_ratio_numerator   = standard_aspect_ratios[index].numerator;
        hdr.aspect_ratio_denominator = standard_aspect_ratios[index].denominator;
      }
    }

    if (bc.get_bit()) {
      hdr.clean_width  = read_uint(bc);
      hdr.clean_height = read_uint(bc);
      hdr.left_offset  = read_uint(bc);
      hdr.top_offset   = read_uint(bc);
    }

    memcpy(&seqhdr, &hdr, sizeof(dirac::sequence_header_t));

    return true;
  } catch (...) {
    return false;
  }
}

//
//  -------------------------------------------------
//

dirac::es_parser_c::es_parser_c()
  : m_stream_pos(0)
  , m_seqhdr_found(false)
  , m_previous_timecode(0)
  , m_num_timecodes(0)
  , m_num_repeated_fields(0)
  , m_default_duration_forced(false)
  , m_default_duration(1000000000ll / 25)
{
}

dirac::es_parser_c::~es_parser_c() {
}

void
dirac::es_parser_c::add_bytes(unsigned char *buffer,
                              size_t size) {
  memory_slice_cursor_c cursor;

  bool previous_found         = false;
  size_t previous_pos         = 0;
  int64_t previous_stream_pos = m_stream_pos;

  if (m_unparsed_buffer.is_set() && (0 != m_unparsed_buffer->get_size()))
    cursor.add_slice(m_unparsed_buffer);
  cursor.add_slice(buffer, size);

  if (3 <= cursor.get_remaining_size()) {
    uint32_t marker = (1 << 24) | ((unsigned int)cursor.get_char() << 16) | ((unsigned int)cursor.get_char() << 8) | (unsigned int)cursor.get_char();

    while (1) {
      if (DIRAC_SYNC_WORD == marker) {
        if (!previous_found) {
          previous_found = true;
          previous_pos   = cursor.get_position() - 4;
          m_stream_pos   = previous_stream_pos + previous_pos;

          if (!cursor.char_available())
            break;

          marker <<= 8;
          marker  |= (unsigned int)cursor.get_char();

          continue;
        }

        unsigned char offset_buffer[4];
        cursor.copy(offset_buffer, previous_pos + 4 + 1, 4);
        uint32_t next_offset = get_uint32_be(offset_buffer);

        if ((0 == next_offset) || ((previous_pos + next_offset) <= (cursor.get_position() - 4))) {
          int new_size = cursor.get_position() - 4 - previous_pos;

          memory_cptr packet = memory_c::alloc(new_size);
          cursor.copy(packet->get_buffer(), previous_pos, new_size);

          handle_unit(packet);

          previous_pos = cursor.get_position() - 4;
          m_stream_pos = previous_stream_pos + previous_pos;
        }
      }

      if (!cursor.char_available())
        break;

      marker <<= 8;
      marker  |= (unsigned int)cursor.get_char();
    }
  }

  unsigned int new_size = cursor.get_size() - previous_pos;
  if (0 != new_size) {
    memory_cptr new_unparsed_buffer = memory_c::alloc(new_size);
    cursor.copy(new_unparsed_buffer->get_buffer(), previous_pos, new_size);
    m_unparsed_buffer = new_unparsed_buffer;

  } else
    m_unparsed_buffer = memory_cptr(NULL);
}

void
dirac::es_parser_c::flush() {
  if (m_unparsed_buffer.is_set() && (4 <= m_unparsed_buffer->get_size())) {
    uint32_t marker = get_uint32_be(m_unparsed_buffer->get_buffer());
    if (DIRAC_SYNC_WORD == marker)
      handle_unit(memory_c::clone(m_unparsed_buffer->get_buffer(), m_unparsed_buffer->get_size()));
  }

  m_unparsed_buffer = memory_cptr(NULL);

  flush_frame();
}

void
dirac::es_parser_c::handle_unit(memory_cptr packet) {
  unsigned char type = packet->get_buffer()[4];

  if (DIRAC_UNIT_SEQUENCE_HEADER == type)
    handle_sequence_header_unit(packet);

  else if (dirac::is_auxiliary_data(type))
    handle_auxiliary_data_unit(packet);

  else if (DIRAC_UNIT_PADDING == type)
    handle_padding_unit(packet);

  else if (DIRAC_UNIT_END_OF_SEQUENCE == type)
    handle_end_of_sequence_unit(packet);

  else if (dirac::is_picture(type))
    handle_picture_unit(packet);

  else
    handle_unknown_unit(packet);
}

void
dirac::es_parser_c::handle_auxiliary_data_unit(memory_cptr packet) {
  add_pre_frame_extra_data(packet);
}

void
dirac::es_parser_c::handle_end_of_sequence_unit(memory_cptr packet) {
  add_post_frame_extra_data(packet);
  flush_frame();
}

void
dirac::es_parser_c::handle_picture_unit(memory_cptr packet) {
  flush_frame();

  if (!m_seqhdr_found)
    return;

  m_current_frame        = frame_cptr(new frame_t);
  m_current_frame->data  = packet;
  m_current_frame->data->grab();
}

void
dirac::es_parser_c::handle_sequence_header_unit(memory_cptr packet) {
  flush_frame();

  add_pre_frame_extra_data(packet);

  dirac::sequence_header_t seqhdr;
  if (!dirac::parse_sequence_header(packet->get_buffer(), packet->get_size(), seqhdr))
    return;

  m_seqhdr_changed = !m_seqhdr_found || (packet->get_size() != m_raw_seqhdr->get_size()) || memcmp(packet->get_buffer(), m_raw_seqhdr->get_buffer(), packet->get_size());

  memcpy(&m_seqhdr, &seqhdr, sizeof(dirac::sequence_header_t));
  m_raw_seqhdr   = memory_cptr(packet->clone());
  m_seqhdr_found = true;

  if (!m_default_duration_forced && (0 != m_seqhdr.frame_rate_numerator) && (0 != m_seqhdr.frame_rate_denominator))
    m_default_duration = 1000000000ll * m_seqhdr.frame_rate_denominator / m_seqhdr.frame_rate_numerator;
}

void
dirac::es_parser_c::handle_padding_unit(memory_cptr) {
  // Intentionally do nothing.
}

void
dirac::es_parser_c::handle_unknown_unit(memory_cptr packet) {
  add_post_frame_extra_data(packet);
}

void
dirac::es_parser_c::flush_frame() {
  if (!m_current_frame.is_set())
    return;

  if (!m_pre_frame_extra_data.empty() || !m_post_frame_extra_data.empty())
    combine_extra_data_with_packet();

  m_current_frame->timecode = get_next_timecode();
  m_current_frame->duration = get_default_duration();

  m_frames.push_back(m_current_frame);

  m_current_frame = frame_cptr(NULL);
}

void
dirac::es_parser_c::combine_extra_data_with_packet() {
  int extra_size = 0;

  for (auto &mem : m_pre_frame_extra_data)
    extra_size += mem->get_size();
  for (auto &mem : m_post_frame_extra_data)
    extra_size += mem->get_size();

  memory_cptr new_packet = memory_c::alloc(extra_size + m_current_frame->data->get_size());
  unsigned char *ptr     = new_packet->get_buffer();

  for (auto &mem : m_pre_frame_extra_data) {
    memcpy(ptr, mem->get_buffer(), mem->get_size());
    ptr += mem->get_size();

    if (DIRAC_UNIT_SEQUENCE_HEADER == mem->get_buffer()[4])
      m_current_frame->contains_sequence_header = true;
  }

  memcpy(ptr, m_current_frame->data->get_buffer(), m_current_frame->data->get_size());
  ptr += m_current_frame->data->get_size();

  for (auto &mem : m_post_frame_extra_data) {
    memcpy(ptr, mem->get_buffer(), mem->get_size());
    ptr += mem->get_size();
  }

  m_pre_frame_extra_data.clear();
  m_post_frame_extra_data.clear();

  m_current_frame->data = new_packet;
}

int64_t
dirac::es_parser_c::get_next_timecode() {
  if (!m_timecodes.empty()) {
    m_previous_timecode   = m_timecodes.front();
    m_num_timecodes       = 0;
    m_timecodes.pop_front();
  }

  ++m_num_timecodes;

  return m_previous_timecode + (m_num_timecodes - 1) * m_default_duration;
}

int64_t
dirac::es_parser_c::peek_next_calculated_timecode() {
  return m_previous_timecode + m_num_timecodes * m_default_duration;
}

void
dirac::es_parser_c::add_pre_frame_extra_data(memory_cptr packet) {
  m_pre_frame_extra_data.push_back(memory_cptr(packet->clone()));
}

void
dirac::es_parser_c::add_post_frame_extra_data(memory_cptr packet) {
  m_post_frame_extra_data.push_back(memory_cptr(packet->clone()));
}

