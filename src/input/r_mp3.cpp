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

int
mp3_reader_c::probe_file(mm_io_c *io,
                         uint64_t,
                         int64_t probe_range,
                         int num_headers) {
  return (find_valid_headers(io, probe_range, num_headers) != -1) ? 1 : 0;
}

mp3_reader_c::mp3_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti) {
  unsigned char buf[16384];

  try {
    io   = new mm_file_io_c(m_ti.m_fname);
    size = io->get_size();

    int pos = find_valid_headers(io, 2 * 1024 * 1024, 5);
    if (0 > pos)
      throw error_c(Y("Could not find a valid MP3 packet."));

    io->setFilePointer(pos, seek_beginning);
    io->read(buf, 4);

    decode_mp3_header(buf, &mp3header);

    io->setFilePointer(pos, seek_beginning);

    show_demuxer_info();

    if ((0 < pos) && verbose)
      mxwarn_fn(m_ti.m_fname, boost::format(Y("Skipping %1% bytes at the beginning (no valid MP3 header found).\n")) % pos);

    bytes_processed = 0;
    m_ti.m_id       = 0;        // ID for this track.
  } catch (...) {
    throw error_c(Y("mp3_reader: Could not open the source file."));
  }
}

mp3_reader_c::~mp3_reader_c() {
  delete io;
}

void
mp3_reader_c::create_packetizer(int64_t) {
  if (!demuxing_requested('a', 0) || (NPTZR() != 0))
    return;

  add_packetizer(new mp3_packetizer_c(this, m_ti, mp3header.sampling_frequency, mp3header.channels, false));
  show_packetizer_info(0, PTZR0);
}

file_status_e
mp3_reader_c::read(generic_packetizer_c *,
                   bool) {
  int nread = io->read(chunk, 16384);
  if (0 >= nread)
    return flush_packetizers();

  PTZR0->process(new packet_t(new memory_c(chunk, nread, false)));
  bytes_processed += nread;

  return FILE_STATUS_MOREDATA;
}

int
mp3_reader_c::get_progress() {
  return 100 * bytes_processed / size;
}

void
mp3_reader_c::identify() {
  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_AUDIO, (boost::format("MPEG-%1% layer %2%")
                                             % (mp3header.version == 1 ? "1" : mp3header.version == 2 ? "2" : "2.5")
                                             % mp3header.layer).str());
}

int
mp3_reader_c::find_valid_headers(mm_io_c *io,
                                 int64_t probe_range,
                                 int num_headers) {
  try {
    io->setFilePointer(0, seek_beginning);
    unsigned char *buf = (unsigned char *)safemalloc(probe_range);
    int nread          = io->read(buf, probe_range);
    int pos            = find_consecutive_mp3_headers(buf, nread, num_headers);
    safefree(buf);
    io->setFilePointer(0, seek_beginning);

    return pos;
  } catch (...) {
    return -1;
  }
}
