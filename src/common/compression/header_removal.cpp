/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   header removal compressor

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/ac3.h"
#include "common/dirac.h"
#include "common/dts.h"
#include "common/endian.h"
#include "common/compression/header_removal.h"

header_removal_compressor_c::header_removal_compressor_c()
  : compressor_c(COMPRESSION_HEADER_REMOVAL)
{
}

memory_cptr
header_removal_compressor_c::do_decompress(memory_cptr const &buffer) {
  if (!m_bytes || (0 == m_bytes->get_size()))
    return buffer;

  memory_cptr new_buffer = memory_c::alloc(buffer->get_size() + m_bytes->get_size());

  memcpy(new_buffer->get_buffer(),                       m_bytes->get_buffer(), m_bytes->get_size());
  memcpy(new_buffer->get_buffer() + m_bytes->get_size(), buffer->get_buffer(),  buffer->get_size());

  return new_buffer;
}

memory_cptr
header_removal_compressor_c::do_compress(memory_cptr const &buffer) {
  if (!m_bytes || (0 == m_bytes->get_size()))
    return buffer;

  size_t size = m_bytes->get_size();
  if (buffer->get_size() < size)
    throw mtx::compression_x(boost::format(Y("Header removal compression not possible because the buffer contained %1% bytes "
                                             "which is less than the size of the headers that should be removed, %2%.")) % buffer->get_size() % size);

  unsigned char *buffer_ptr = buffer->get_buffer();
  unsigned char *bytes_ptr  = m_bytes->get_buffer();

  if (memcmp(buffer_ptr, bytes_ptr, size)) {
    std::string b_buffer, b_bytes;
    size_t i;

    for (i = 0; size > i; ++i) {
      b_buffer += (boost::format(" %|1$02x|") % static_cast<unsigned int>(buffer_ptr[i])).str();
      b_bytes  += (boost::format(" %|1$02x|") % static_cast<unsigned int>(bytes_ptr[i])).str();
    }
    throw mtx::compression_x(boost::format(Y("Header removal compression not possible because the buffer did not start with the bytes that should be removed. "
                                             "Wanted bytes:%1%; found:%2%.")) % b_bytes % b_buffer);
  }

  return memory_c::clone(buffer->get_buffer() + size, buffer->get_size() - size);
}

void
header_removal_compressor_c::set_track_headers(KaxContentEncoding &c_encoding) {
  compressor_c::set_track_headers(c_encoding);

  // Set compression parameters.
  GetChild<KaxContentCompSettings>(GetChild<KaxContentCompression>(c_encoding)).CopyBuffer(m_bytes->get_buffer(), m_bytes->get_size());
}

// ------------------------------------------------------------

analyze_header_removal_compressor_c::analyze_header_removal_compressor_c()
  : compressor_c(COMPRESSION_ANALYZE_HEADER_REMOVAL)
  , m_packet_counter(0)
{
}

analyze_header_removal_compressor_c::~analyze_header_removal_compressor_c() {
  if (!m_bytes)
    mxinfo("Analysis failed: no packet encountered\n");

  else if (m_bytes->get_size() == 0)
    mxinfo("Analysis complete but no similarities found.\n");

  else {
    mxinfo(boost::format("Analysis complete. %1% identical byte(s) at the start of each of the %2% packet(s). Hex dump of the content:\n") % m_bytes->get_size() % m_packet_counter);
    mxhexdump(0, m_bytes->get_buffer(), m_bytes->get_size());
  }
}

memory_cptr
analyze_header_removal_compressor_c::do_decompress(memory_cptr const &buffer) {
  mxerror("analyze_header_removal_compressor_c::do_decompress(): not supported\n");
  return buffer;
}

memory_cptr
analyze_header_removal_compressor_c::do_compress(memory_cptr const &buffer) {
  ++m_packet_counter;

  if (!m_bytes) {
    m_bytes = buffer->clone();
    return buffer;
  }

  unsigned char *current = buffer->get_buffer();
  unsigned char *saved   = m_bytes->get_buffer();
  size_t i, new_size     = 0;

  for (i = 0; i < std::min(buffer->get_size(), m_bytes->get_size()); ++i, ++new_size)
    if (current[i] != saved[i])
      break;

  m_bytes->set_size(new_size);

  return buffer;
}

void
analyze_header_removal_compressor_c::set_track_headers(KaxContentEncoding &) {
}

// ------------------------------------------------------------

mpeg4_p2_compressor_c::mpeg4_p2_compressor_c() {
  memory_cptr bytes = memory_c::alloc(3);
  put_uint24_be(bytes->get_buffer(), 0x000001);
  set_bytes(bytes);
}

mpeg4_p10_compressor_c::mpeg4_p10_compressor_c() {
  memory_cptr bytes      = memory_c::alloc(1);
  bytes->get_buffer()[0] = 0;
  set_bytes(bytes);
}

dirac_compressor_c::dirac_compressor_c() {
  memory_cptr bytes = memory_c::alloc(4);
  put_uint32_be(bytes->get_buffer(), DIRAC_SYNC_WORD);
  set_bytes(bytes);
}

dts_compressor_c::dts_compressor_c() {
  memory_cptr bytes = memory_c::alloc(4);
  put_uint32_be(bytes->get_buffer(), DTS_HEADER_MAGIC);
  set_bytes(bytes);
}

ac3_compressor_c::ac3_compressor_c() {
  memory_cptr bytes = memory_c::alloc(2);
  put_uint16_be(bytes->get_buffer(), AC3_SYNC_WORD);
  set_bytes(bytes);
}

mp3_compressor_c::mp3_compressor_c() {
  memory_cptr bytes = memory_c::alloc(1);
  bytes->get_buffer()[0] = 0xff;
  set_bytes(bytes);
}
