/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   TrueHD demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <algorithm>

#include <avilib.h>

#include "common/error.h"
#include "common/id3.h"
#include "common/mm_io_x.h"
#include "input/r_truehd.h"
#include "output/p_truehd.h"

#define TRUEHD_READ_SIZE (1024 * 1024)

int
truehd_reader_c::probe_file(mm_io_c *in,
                            uint64_t /* size */) {
  try {
    in->setFilePointer(0, seek_beginning);
    skip_id3v2_tag(*in);
    return find_valid_headers(*in, TRUEHD_READ_SIZE, 2) ? 1 : 0;

  } catch (...) {
    return 0;
  }
}

truehd_reader_c::truehd_reader_c(const track_info_c &ti,
                                 const mm_io_cptr &in)
  : generic_reader_c(ti, in)
  , m_chunk(memory_c::alloc(TRUEHD_READ_SIZE))
{
}

void
truehd_reader_c::read_headers() {
  try {
    int tag_size_start = skip_id3v2_tag(*m_in);
    int tag_size_end   = id3_tag_present_at_end(*m_in);

    if (0 > tag_size_start)
      tag_size_start = 0;
    if (0 < tag_size_end)
      m_size -= tag_size_end;

    size_t init_read_len = std::min(m_size - tag_size_start, static_cast<uint64_t>(TRUEHD_READ_SIZE));

    if (m_in->read(m_chunk->get_buffer(), init_read_len) != init_read_len)
      throw mtx::input::header_parsing_x();

    m_in->setFilePointer(tag_size_start, seek_beginning);

    truehd_parser_c parser;
    parser.add_data(m_chunk->get_buffer(), init_read_len);
    m_header  = parser.get_next_frame();
    m_ti.m_id = 0;                  // ID for this track.

    show_demuxer_info();

  } catch (mtx::mm_io::exception &) {
    throw mtx::input::open_x();
  }
}

truehd_reader_c::~truehd_reader_c() {
}

void
truehd_reader_c::create_packetizer(int64_t) {
  if (!demuxing_requested('a', 0) || (NPTZR() != 0))
    return;

  add_packetizer(new truehd_packetizer_c(this, m_ti, m_header->m_codec, m_header->m_sampling_rate, m_header->m_channels));
  show_packetizer_info(0, PTZR0);
}

file_status_e
truehd_reader_c::read(generic_packetizer_c *,
                      bool) {
  int64_t remaining_bytes = m_size - m_in->getFilePointer();
  int64_t read_len        = std::min((int64_t)TRUEHD_READ_SIZE, remaining_bytes);
  int num_read            = m_in->read(m_chunk->get_buffer(), read_len);

  if (0 <= num_read)
    PTZR0->process(new packet_t(new memory_c(m_chunk->get_buffer(), num_read, false)));

  return (0 >= (remaining_bytes - num_read)) ? flush_packetizers() : FILE_STATUS_MOREDATA;
}

void
truehd_reader_c::identify() {
  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_AUDIO, m_header->is_truehd() ? "TrueHD" : "MLP");
}

bool
truehd_reader_c::find_valid_headers(mm_io_c &in,
                                    int64_t probe_range,
                                    int num_headers) {
  try {
    memory_cptr buf(memory_c::alloc(probe_range));

    in.setFilePointer(0, seek_beginning);
    skip_id3v2_tag(in);

    int num_read = in.read(buf->get_buffer(), probe_range);

    truehd_parser_c parser;
    parser.add_data(buf->get_buffer(), num_read);

    int num_sync_frames = 0;
    while (parser.frame_available()) {
      truehd_frame_cptr frame = parser.get_next_frame();
      if (frame->is_sync())
        ++num_sync_frames;
    }

    return num_sync_frames >= num_headers;

  } catch (...) {
    return false;
  }
}
