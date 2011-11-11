/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   MP3 reader module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/error.h"
#include "common/hacks.h"
#include "input/r_mp3.h"
#include "output/p_mp3.h"

#define CHUNK_SIZE 16384

int
mp3_reader_c::probe_file(mm_io_c *in,
                         uint64_t,
                         int64_t probe_range,
                         int num_headers) {
  return (find_valid_headers(*in, probe_range, num_headers) != -1) ? 1 : 0;
}

mp3_reader_c::mp3_reader_c(const track_info_c &ti,
                           const mm_io_cptr &in)
  : generic_reader_c(ti, in)
  , m_chunk(memory_c::alloc(CHUNK_SIZE))
{
}

void
mp3_reader_c::read_headers() {
  try {
    int pos = find_valid_headers(*m_in, 2 * 1024 * 1024, 5);
    if (0 > pos)
      throw mtx::input::header_parsing_x();

    m_in->setFilePointer(pos, seek_beginning);
    m_in->read(m_chunk->get_buffer(), 4);

    decode_mp3_header(m_chunk->get_buffer(), &m_mp3header);

    m_in->setFilePointer(pos, seek_beginning);

    show_demuxer_info();

    if ((0 < pos) && verbose)
      mxwarn_fn(m_ti.m_fname, boost::format(Y("Skipping %1% bytes at the beginning (no valid MP3 header found).\n")) % pos);

    m_ti.m_id = 0;        // ID for this track.

  } catch (mtx::mm_io::exception &) {
    throw mtx::input::open_x();
  }
}

mp3_reader_c::~mp3_reader_c() {
}

void
mp3_reader_c::create_packetizer(int64_t) {
  if (!demuxing_requested('a', 0) || (NPTZR() != 0))
    return;

  add_packetizer(new mp3_packetizer_c(this, m_ti, m_mp3header.sampling_frequency, m_mp3header.channels, false));
  show_packetizer_info(0, PTZR0);
}

file_status_e
mp3_reader_c::read(generic_packetizer_c *,
                   bool) {
  int nread = m_in->read(m_chunk->get_buffer(), CHUNK_SIZE);
  if (0 >= nread)
    return flush_packetizers();

  PTZR0->process(new packet_t(new memory_c(m_chunk->get_buffer(), nread, false)));

  return FILE_STATUS_MOREDATA;
}

void
mp3_reader_c::identify() {
  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_AUDIO, (boost::format("MPEG-%1% layer %2%") % (m_mp3header.version == 1 ? "1" : m_mp3header.version == 2 ? "2" : "2.5") % m_mp3header.layer).str());
}

int
mp3_reader_c::find_valid_headers(mm_io_c &io,
                                 int64_t probe_range,
                                 int num_headers) {
  try {
    io.setFilePointer(0, seek_beginning);
    memory_cptr buf = memory_c::alloc(probe_range);
    int nread       = io.read(buf->get_buffer(), probe_range);
    io.setFilePointer(0, seek_beginning);

    return find_consecutive_mp3_headers(buf->get_buffer(), nread, num_headers);
  } catch (...) {
    return -1;
  }
}
