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
ac3_reader_c::probe_file(mm_io_c *io,
                         uint64_t size,
                         int64_t probe_size,
                         int num_headers) {
  try {
    io->setFilePointer(0, seek_beginning);
    skip_id3v2_tag(*io);
    return (find_valid_headers(io, probe_size, num_headers) != -1) ? 1 : 0;

  } catch (...) {
    return 0;
  }
}

ac3_reader_c::ac3_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti),
  chunk(memory_c::alloc(AC3_READ_SIZE)) {

  try {
    io                 = new mm_file_io_c(m_ti.m_fname);
    size               = io->get_size();

    int tag_size_start = skip_id3v2_tag(*io);
    int tag_size_end   = id3_tag_present_at_end(*io);

    if (0 > tag_size_start)
      tag_size_start = 0;
    if (0 < tag_size_end)
      size -= tag_size_end;

    size_t init_read_len = std::min(size - tag_size_start, (int64_t)AC3_READ_SIZE);

    if (io->read(chunk->get_buffer(), init_read_len) != init_read_len)
      throw error_c(boost::format(Y("ac3_reader: Could not read %1% bytes.")) % AC3_READ_SIZE);

    io->setFilePointer(tag_size_start, seek_beginning);

  } catch (...) {
    throw error_c(Y("ac3_reader: Could not open the source file."));
  }

  if (0 > find_ac3_header(chunk->get_buffer(), AC3_READ_SIZE, &ac3header, true))
    throw error_c(boost::format(Y("ac3_reader: No valid AC3 packet found in the first %1% bytes.\n")) % AC3_READ_SIZE);

  bytes_processed = 0;
  m_ti.m_id       = 0;          // ID for this track.

  show_demuxer_info();
}

ac3_reader_c::~ac3_reader_c() {
  delete io;
}

void
ac3_reader_c::create_packetizer(int64_t) {
  if (!demuxing_requested('a', 0) || (NPTZR() != 0))
    return;

  add_packetizer(new ac3_packetizer_c(this, m_ti, ac3header.sample_rate, ac3header.channels, ac3header.bsid));
  mxinfo_tid(m_ti.m_fname, 0, boost::format(Y("Using the %1%AC3 output module.\n")) % (16 == ac3header.bsid ? "E" : ""));
}

file_status_e
ac3_reader_c::read(generic_packetizer_c *,
                   bool) {
  int64_t remaining_bytes = size - io->getFilePointer();
  int64_t read_len        = std::min((int64_t)AC3_READ_SIZE, remaining_bytes);
  int num_read            = io->read(chunk->get_buffer(), read_len);

  if (0 < num_read) {
    PTZR0->process(new packet_t(new memory_c(chunk->get_buffer(), num_read, false)));
    bytes_processed += num_read;

    if (0 < (remaining_bytes - num_read))
      return FILE_STATUS_MOREDATA;
  }

  return flush_packetizers();
}

int
ac3_reader_c::get_progress() {
  return 100 * bytes_processed / size;
}

void
ac3_reader_c::identify() {
  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_AUDIO, 16 == ac3header.bsid ? "EAC3" : "AC3");
}

int
ac3_reader_c::find_valid_headers(mm_io_c *io,
                                 int64_t probe_range,
                                 int num_headers) {
  try {
    memory_cptr buf(memory_c::alloc(probe_range));

    io->setFilePointer(0, seek_beginning);
    skip_id3v2_tag(*io);

    int num_read = io->read(buf->get_buffer(), probe_range);
    int pos      = find_consecutive_ac3_headers(buf->get_buffer(), num_read, num_headers);

    io->setFilePointer(0, seek_beginning);

    return pos;

  } catch (...) {
    return -1;
  }
}
