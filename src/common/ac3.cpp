/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper function for AC3 data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <cstring>

#include "common/ac3.h"
#include "common/bit_cursor.h"
#include "common/bswap.h"
#include "common/byte_buffer.h"
#include "common/checksums.h"
#include "common/endian.h"

ac3::frame_c::frame_c() {
  init();
}

void
ac3::frame_c::init() {
  m_sample_rate     = 0;
  m_bit_rate        = 0;
  m_channels        = 0;
  m_flags           = 0;
  m_bytes           = 0;
  m_bs_id           = 0;
  m_samples         = 0;
  m_frame_type      = 0;
  m_sub_stream_id   = 0;
  m_stream_position = 0;
  m_garbage_size    = 0;
  m_valid           = false;
  m_data.clear();
}

bool
ac3::frame_c::is_eac3()
  const {
  return (0x10 == m_bs_id) || !m_dependent_frames.empty();
}

void
ac3::frame_c::add_dependent_frame(frame_c const &frame,
                                  unsigned char const *buffer,
                                  size_t buffer_size) {
  m_data->add(buffer, buffer_size);
  m_dependent_frames.push_back(frame);
}

bool
ac3::frame_c::decode_header(unsigned char const *buffer,
                            size_t buffer_size) {
  bit_cursor_c bc(buffer, buffer_size);

  try {
    init();

    if (AC3_SYNC_WORD != bc.get_bits(16))
      return false;

    m_bs_id = bc.get_bits(29) & 0x1f;
    bc.set_bit_position(16);
    m_valid = 0x10 == m_bs_id ? decode_header_type_eac3(bc)
            : 0x0c <= m_bs_id ? false
            :                   decode_header_type_ac3(bc);

  } catch (mtx::mm_io::end_of_file_x &) {
  }

  return m_valid;
}

bool
ac3::frame_c::decode_header_type_eac3(bit_cursor_c &bc) {
  static const int sample_rates[] = { 48000, 44100, 32000, 24000, 22050, 16000 };
  static const int channels[]     = {     2,     1,     2,     3,     3,     4,     4,     5 };
  static const int samples[]      = {   256,   512,   768,  1536 };

  m_frame_type = bc.get_bits(2);

  if (EAC3_FRAME_TYPE_RESERVED == m_frame_type)
    return false;

  m_sub_stream_id = bc.get_bits(3);
  m_bytes         = (bc.get_bits(11) + 1) << 1;

  uint8_t fscod   = bc.get_bits(2);
  uint8_t fscod2  = bc.get_bits(2);

  if ((0x03 == fscod) && (0x03 == fscod2))
    return false;

  uint8_t acmod = bc.get_bits(3);
  uint8_t lfeon = bc.get_bit();

  m_sample_rate = sample_rates[0x03 == fscod ? 3 + fscod2 : fscod];
  m_channels    = channels[acmod] + lfeon;
  m_samples     = (0x03 == fscod) ? 1536 : samples[fscod2];

  return true;
}

bool
ac3::frame_c::decode_header_type_ac3(bit_cursor_c &bc) {
  static const uint16_t sample_rates[]     = { 48000, 44100, 32000 };
  static const uint8_t channel_modes[]     = {  2,  1,  2,  3,  3,  4,  4,   5 };
  static const uint16_t bit_rates[]        = { 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320, 384, 448, 512, 576, 640 };
  static const uint16_t frame_sizes[38][3] = {
    { 64,   69,   96   },
    { 64,   70,   96   },
    { 80,   87,   120  },
    { 80,   88,   120  },
    { 96,   104,  144  },
    { 96,   105,  144  },
    { 112,  121,  168  },
    { 112,  122,  168  },
    { 128,  139,  192  },
    { 128,  140,  192  },
    { 160,  174,  240  },
    { 160,  175,  240  },
    { 192,  208,  288  },
    { 192,  209,  288  },
    { 224,  243,  336  },
    { 224,  244,  336  },
    { 256,  278,  384  },
    { 256,  279,  384  },
    { 320,  348,  480  },
    { 320,  349,  480  },
    { 384,  417,  576  },
    { 384,  418,  576  },
    { 448,  487,  672  },
    { 448,  488,  672  },
    { 512,  557,  768  },
    { 512,  558,  768  },
    { 640,  696,  960  },
    { 640,  697,  960  },
    { 768,  835,  1152 },
    { 768,  836,  1152 },
    { 896,  975,  1344 },
    { 896,  976,  1344 },
    { 1024, 1114, 1536 },
    { 1024, 1115, 1536 },
    { 1152, 1253, 1728 },
    { 1152, 1254, 1728 },
    { 1280, 1393, 1920 },
    { 1280, 1394, 1920 },
  };

  bc.skip_bits(16);             // crc1
  uint8_t fscod = bc.get_bits(2);
  if (0x03 == fscod)
    return false;

  uint8_t frmsizecod = bc.get_bits(6);
  if (38 < frmsizecod)
    return false;

  bc.skip_bits(5 + 3);          // bsid, bsmod
  uint8_t acmod      = bc.get_bits(3);
  if ((acmod & 0x01) && (acmod != 0x01))
    bc.skip_bits(2);            // cmixlev
  if (acmod & 0x04)
    bc.skip_bits(2);            // surmixlev
  if (0x02 == acmod)
    bc.skip_bits(2);            // dsurmod
  uint8_t lfeon      = bc.get_bit();

  uint8_t sr_shift   = std::max(m_bs_id, 8u) - 8;

  m_sample_rate      = sample_rates[fscod] >> sr_shift;
  m_bit_rate         = (bit_rates[frmsizecod >> 1] * 1000) >> sr_shift;
  m_channels         = channel_modes[acmod] + lfeon;
  m_bytes            = frame_sizes[frmsizecod][fscod] << 1;

  m_samples          = 1536;
  m_frame_type       = EAC3_FRAME_TYPE_INDEPENDENT;

  return true;
}

int
ac3::frame_c::find_in(memory_cptr const &buffer) {
  return find_in(buffer->get_buffer(), buffer->get_size());
}

int
ac3::frame_c::find_in(unsigned char const *buffer,
                      size_t buffer_size) {
  for (size_t offset = 0; offset < buffer_size; ++offset)
    if (decode_header(&buffer[offset], buffer_size - offset))
      return offset;
  return -1;
}

std::string
ac3::frame_c::to_string(bool verbose)
  const {
  if (!verbose)
    return (boost::format("position %1% BS ID %2% size %3% EAC3 %4%") % m_stream_position % m_bs_id % m_bytes % is_eac3()).str();

  const std::string &frame_type = !is_eac3()                                  ? "---"
                                : m_frame_type == EAC3_FRAME_TYPE_INDEPENDENT ? "independent"
                                : m_frame_type == EAC3_FRAME_TYPE_DEPENDENT   ? "dependent"
                                : m_frame_type == EAC3_FRAME_TYPE_AC3_CONVERT ? "AC3 convert"
                                : m_frame_type == EAC3_FRAME_TYPE_RESERVED    ? "reserved"
                                :                                               "unknown";

  std::string output = (boost::format("position %1% size %3% garbage %2% BS ID %4% EAC3 %15% sample rate %5% bit rate %6% channels %7% flags %8% samples %9% type %10% (%13%) "
                                      "sub stream ID %11% has dependent frames %12% total size %14%")
                        % m_stream_position
                        % m_garbage_size
                        % m_bytes
                        % m_bs_id
                        % m_sample_rate
                        % m_bit_rate
                        % m_channels
                        % m_flags
                        % m_samples
                        % m_frame_type
                        % m_sub_stream_id
                        % m_dependent_frames.size()
                        % frame_type
                        % (m_data.is_set() ? m_data->get_size() : 0)
                        % is_eac3()
                        ).str();

  for (auto &frame : m_dependent_frames)
    output += (boost::format(" { %1% }") % frame.to_string(verbose)).str();

  return output;
}

// ------------------------------------------------------------

ac3::parser_c::parser_c()
  : m_parsed_stream_position(0)
  , m_total_stream_position(0)
  , m_garbage_size(0)
{
}

void
ac3::parser_c::add_bytes(memory_cptr const &mem) {
  add_bytes(mem->get_buffer(), mem->get_size());
}

void
ac3::parser_c::add_bytes(unsigned char *const buffer,
                         size_t size) {
  m_buffer.add(buffer, size);
  m_total_stream_position += size;
  parse(false);
}

void
ac3::parser_c::flush() {
  parse(true);
}

size_t
ac3::parser_c::frame_available()
  const {
  return m_frames.size();
}

ac3::frame_c
ac3::parser_c::get_frame() {
  frame_c frame = m_frames.front();
  m_frames.pop_front();
  return frame;
}

uint64_t
ac3::parser_c::get_total_stream_position()
  const {
  return m_total_stream_position;
}

uint64_t
ac3::parser_c::get_parsed_stream_position()
  const {
  return m_parsed_stream_position;
}

void
ac3::parser_c::parse(bool end_of_stream) {
  unsigned char *const buffer = m_buffer.get_buffer();
  size_t buffer_size          = m_buffer.get_size();
  size_t position             = 0;

  while ((position + 8) < buffer_size) {
    ac3::frame_c frame;

    if (!frame.decode_header(&buffer[position], buffer_size - position)) {
      ++position;
      ++m_garbage_size;
      continue;
    }

    if ((position + frame.m_bytes) > buffer_size)
      break;

    frame.m_stream_position = m_parsed_stream_position + position;

    if (!m_current_frame.m_valid || (EAC3_FRAME_TYPE_DEPENDENT != frame.m_frame_type)) {
      if (m_current_frame.m_valid)
        m_frames.push_back(m_current_frame);

      m_current_frame        = frame;
      m_current_frame.m_data = memory_c::clone(&buffer[position], frame.m_bytes);

    } else
      m_current_frame.add_dependent_frame(frame, &buffer[position], frame.m_bytes);

    m_current_frame.m_garbage_size += m_garbage_size;
    m_garbage_size                  = 0;
    position                       += frame.m_bytes;
  }

  m_buffer.remove(position);
  m_parsed_stream_position += position;

  if (m_current_frame.m_valid && end_of_stream) {
    m_frames.push_back(m_current_frame);
    m_current_frame.init();
  }
}

int
ac3::parser_c::find_consecutive_frames(unsigned char const *buffer,
                                       size_t buffer_size,
                                       size_t num_required_headers) {
  size_t base = 0;
  bool debug  = debugging_requested("ac3_consecutive_frames");

  do {
    mxdebug_if(debug, boost::format("Starting search for %2% headers with base %1%, buffer size %3%\n") % base % num_required_headers % buffer_size);

    size_t position = base;

    ac3::frame_c first_frame;
    while (((position + 8) < buffer_size) && !first_frame.decode_header(&buffer[position], buffer_size - position))
      ++position;

    mxdebug_if(debug, boost::format("First frame at %1% valid %2%\n") % position % first_frame.m_valid);

    if (!first_frame.m_valid)
      return -1;

    size_t offset            = position + first_frame.m_bytes;
    size_t num_headers_found = 1;

    while (   (num_headers_found < num_required_headers)
           && (offset            < buffer_size)) {

      ac3::frame_c current_frame;
      if (!current_frame.decode_header(&buffer[offset], buffer_size - offset))
        break;

      if (8 > current_frame.m_bytes) {
        mxdebug_if(debug, boost::format("Current frame at %1% has invalid size %2%\n") % offset % current_frame.m_bytes);
        break;
      }

      if (   (current_frame.m_bs_id       != first_frame.m_bs_id)
          && (current_frame.m_channels    != first_frame.m_channels)
          && (current_frame.m_sample_rate != first_frame.m_sample_rate)) {
        mxdebug_if(debug,
                   boost::format("Current frame at %7% differs from first frame. (first/current) BS ID: %1%/%2% channels: %3%/%4% sample rate: %5%/%6%\n")
                   % first_frame.m_bs_id % current_frame.m_bs_id % first_frame.m_channels % current_frame.m_channels % first_frame.m_sample_rate % current_frame.m_sample_rate % offset);
        break;
      }

      mxdebug_if(debug, boost::format("Current frame at %1% equals first frame, found %2%\n") % offset % (num_headers_found + 1));

      ++num_headers_found;
      offset += current_frame.m_bytes;
    }

    if (num_headers_found == num_required_headers) {
      mxdebug_if(debug, boost::format("Found required number of headers at %1%\n") % position);
      return position;
    }

    base = position + 2;
  } while (base < buffer_size);

  return -1;
}

// ------------------------------------------------------------

/*
   The functions mul_poly, pow_poly and verify_ac3_crc were taken
   or adopted from the ffmpeg project, file "libavcodec/ac3enc.c".

   The license here is the GPL.
 */

#define CRC16_POLY ((1 << 0) | (1 << 2) | (1 << 15) | (1 << 16))

static unsigned int
mul_poly(unsigned int a,
         unsigned int b,
         unsigned int poly) {
  unsigned int c = 0;
  while (a) {
    if (a & 1)
      c ^= b;
    a = a >> 1;
    b = b << 1;
    if (b & (1 << 16))
      b ^= poly;
  }
  return c;
}

static unsigned int
pow_poly(unsigned int a,
         unsigned int n,
         unsigned int poly) {
  unsigned int r = 1;
  while (n) {
    if (n & 1)
      r = mul_poly(r, a, poly);
    a = mul_poly(a, a, poly);
    n >>= 1;
  }
  return r;
}

bool
verify_ac3_checksum(const unsigned char *buf,
                    size_t size) {
  ac3::frame_c frame;

  if (!frame.decode_header(buf, size))
    return false;

  if (size < frame.m_bytes)
    return false;

  uint16_t expected_crc = get_uint16_be(&buf[2]);

  int frame_size_words  = frame.m_bytes >> 1;
  int frame_size_58     = (frame_size_words >> 1) + (frame_size_words >> 3);

  uint16_t actual_crc   = bswap_16(crc_calc(crc_get_table(CRC_16_ANSI), 0, buf + 4, 2 * frame_size_58 - 4));
  unsigned int crc_inv  = pow_poly((CRC16_POLY >> 1), (16 * frame_size_58) - 16, CRC16_POLY);
  actual_crc            = mul_poly(crc_inv, actual_crc, CRC16_POLY);

  return expected_crc == actual_crc;
}
