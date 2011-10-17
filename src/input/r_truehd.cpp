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
#include "input/r_truehd.h"
#include "output/p_truehd.h"

#define TRUEHD_READ_SIZE (1024 * 1024)

int
truehd_reader_c::probe_file(mm_io_c *io,
                            uint64_t size) {
  try {
    io->setFilePointer(0, seek_beginning);
    skip_id3v2_tag(*io);
    return find_valid_headers(io, TRUEHD_READ_SIZE, 2) ? 1 : 0;

  } catch (...) {
    return 0;
  }
}

truehd_reader_c::truehd_reader_c(track_info_c &_ti)
  throw (error_c)
  : generic_reader_c(_ti)
  , m_chunk(memory_c::alloc(TRUEHD_READ_SIZE))
  , m_bytes_processed(0)
  , m_file_size(0)
{

  try {
    m_io               = mm_io_cptr(new mm_file_io_c(m_ti.m_fname));
    m_file_size        = m_io->get_size();

    int tag_size_start = skip_id3v2_tag(*m_io);
    int tag_size_end   = id3_tag_present_at_end(*m_io);

    if (0 > tag_size_start)
      tag_size_start = 0;
    if (0 < tag_size_end)
      m_file_size -= tag_size_end;

    size_t init_read_len = std::min(m_file_size - tag_size_start, (int64_t)TRUEHD_READ_SIZE);

    if (m_io->read(m_chunk->get_buffer(), init_read_len) != init_read_len)
      throw error_c(boost::format(Y("truehd_reader: Could not read %1% bytes.")) % TRUEHD_READ_SIZE);

    m_io->setFilePointer(tag_size_start, seek_beginning);

    truehd_parser_c parser;
    parser.add_data(m_chunk->get_buffer(), init_read_len);
    m_header  = parser.get_next_frame();
    m_ti.m_id = 0;                  // ID for this track.

    show_demuxer_info();

  } catch (...) {
    throw error_c(Y("truehd_reader: Could not open the source file."));
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
  int64_t remaining_bytes = m_file_size - m_io->getFilePointer();
  int64_t read_len        = std::min((int64_t)TRUEHD_READ_SIZE, remaining_bytes);
  int num_read            = m_io->read(m_chunk->get_buffer(), read_len);

  if (0 <= num_read) {
    PTZR0->process(new packet_t(new memory_c(m_chunk->get_buffer(), num_read, false)));
    m_bytes_processed += num_read;
  }

  return (0 >= (remaining_bytes - num_read)) ? flush_packetizers() : FILE_STATUS_MOREDATA;
}

int
truehd_reader_c::get_progress() {
  return 100 * m_bytes_processed / m_file_size;
}

void
truehd_reader_c::identify() {
  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_AUDIO, m_header->is_truehd() ? "TrueHD" : "MLP");
}

bool
truehd_reader_c::find_valid_headers(mm_io_c *io,
                                    int64_t probe_range,
                                    int num_headers) {
  try {
    memory_cptr buf(memory_c::alloc(probe_range));

    io->setFilePointer(0, seek_beginning);
    skip_id3v2_tag(*io);

    int num_read = io->read(buf->get_buffer(), probe_range);

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
