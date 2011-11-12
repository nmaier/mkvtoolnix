/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   AC3 demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <algorithm>

#include <avilib.h>

#include "common/error.h"
#include "common/id3.h"
#include "input/r_ac3.h"
#include "output/p_ac3.h"

#define AC3_READ_SIZE 16384

int
ac3_reader_c::probe_file(mm_io_c *in,
                         uint64_t size,
                         int64_t probe_size,
                         int num_headers) {
  try {
    in->setFilePointer(0, seek_beginning);
    skip_id3v2_tag(*in);
    return (find_valid_headers(*in, probe_size, num_headers) != -1) ? 1 : 0;

  } catch (...) {
    return 0;
  }
}

ac3_reader_c::ac3_reader_c(const track_info_c &ti,
                           const mm_io_cptr &in)
  : generic_reader_c(ti, in)
  , m_chunk(memory_c::alloc(AC3_READ_SIZE))
{
}

void
ac3_reader_c::read_headers() {
  try {
    int tag_size_start = skip_id3v2_tag(*m_in);
    int tag_size_end   = id3_tag_present_at_end(*m_in);

    if (0 > tag_size_start)
      tag_size_start = 0;
    if (0 < tag_size_end)
      m_size -= tag_size_end;

    size_t init_read_len = std::min(m_size - tag_size_start, static_cast<uint64_t>(AC3_READ_SIZE));

    if (m_in->read(m_chunk->get_buffer(), init_read_len) != init_read_len)
      throw mtx::input::header_parsing_x();

    m_in->setFilePointer(tag_size_start, seek_beginning);

  } catch (mtx::mm_io::exception &) {
    throw mtx::input::open_x();
  }

  if (0 > find_ac3_header(m_chunk->get_buffer(), AC3_READ_SIZE, &m_ac3header, true))
    throw mtx::input::header_parsing_x();

  m_ti.m_id       = 0;          // ID for this track.

  show_demuxer_info();
}

ac3_reader_c::~ac3_reader_c() {
}

void
ac3_reader_c::create_packetizer(int64_t) {
  if (!demuxing_requested('a', 0) || (NPTZR() != 0))
    return;

  add_packetizer(new ac3_packetizer_c(this, m_ti, m_ac3header.sample_rate, m_ac3header.channels, m_ac3header.bsid));
  show_packetizer_info(0, PTZR0);
}

file_status_e
ac3_reader_c::read(generic_packetizer_c *,
                   bool) {
  uint64_t remaining_bytes = m_size - m_in->getFilePointer();
  uint64_t read_len        = std::min(static_cast<uint64_t>(AC3_READ_SIZE), remaining_bytes);
  int num_read             = m_in->read(m_chunk->get_buffer(), read_len);

  if (0 < num_read)
    PTZR0->process(new packet_t(new memory_c(m_chunk->get_buffer(), num_read, false)));

  return (0 != num_read) && (0 < (remaining_bytes - num_read)) ? FILE_STATUS_MOREDATA : flush_packetizers();
}

void
ac3_reader_c::identify() {
  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_AUDIO, 16 == m_ac3header.bsid ? "EAC3" : "AC3");
}

int
ac3_reader_c::find_valid_headers(mm_io_c &in,
                                 int64_t probe_range,
                                 int num_headers) {
  try {
    memory_cptr buf(memory_c::alloc(probe_range));

    in.setFilePointer(0, seek_beginning);
    skip_id3v2_tag(in);

    int num_read = in.read(buf->get_buffer(), probe_range);
    int pos      = find_consecutive_ac3_headers(buf->get_buffer(), num_read, num_headers);

    in.setFilePointer(0, seek_beginning);

    return pos;

  } catch (...) {
    return -1;
  }
}
