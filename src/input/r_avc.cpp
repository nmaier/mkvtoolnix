/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   AVC/h2.64 ES demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <algorithm>
#include <deque>
#include <list>
#include <memory>

#include "byte_buffer.h"
#include "common.h"
#include "error.h"
#include "output_control.h"
#include "r_avc.h"
#include "p_avc.h"

using namespace std;

#define PROBESIZE 4
#define READ_SIZE 1024 * 1024
#define MAX_PROBE_BUFFERS 50

using namespace mpeg4::p10;

int
avc_es_reader_c::probe_file(mm_io_c *io,
                            int64_t size) {
  try {
    if (PROBESIZE > size)
      return 0;

    memory_cptr buf(new memory_c((unsigned char *)safemalloc(READ_SIZE), READ_SIZE));
    int num_read, i;
    bool first = true;

    avc_es_parser_c parser;
    parser.ignore_nalu_size_length_errors();
    parser.set_nalu_size_length(4);
    parser.enable_timecode_generation(40000000);

    io->setFilePointer(0, seek_beginning);
    for (i = 0; MAX_PROBE_BUFFERS > i; ++i) {
      num_read = io->read(buf->get(), READ_SIZE);
      if (4 > num_read)
        return 0;

      // MPEG TS starts with 0x47.
      if (first && (0x47 == buf->get()[0]))
        return 0;
      first = false;

      parser.add_bytes(buf->get(), num_read);

      if (parser.headers_parsed())
        return 1;
    }

  } catch (error_c &err) {
    mxinfo(boost::format(Y("Error %1%\n")) % err.get_error());

  } catch (...) {
    mxinfo(Y("have an xcptn\n"));
  }

  return 0;
}

avc_es_reader_c::avc_es_reader_c(track_info_c &n_ti)
  throw (error_c):
  generic_reader_c(n_ti),
  m_bytes_processed(0),
  m_buffer(new memory_c((unsigned char *)safemalloc(READ_SIZE), READ_SIZE)) {

  try {
    m_io   = counted_ptr<mm_io_c>(new mm_file_io_c(ti.fname));
    m_size = m_io->get_size();

    avc_es_parser_c parser;
    parser.ignore_nalu_size_length_errors();
    parser.enable_timecode_generation(40000000);

    if (map_has_key(ti.nalu_size_lengths, 0))
      parser.set_nalu_size_length(ti.nalu_size_lengths[0]);
    else if (map_has_key(ti.nalu_size_lengths, -1))
      parser.set_nalu_size_length(ti.nalu_size_lengths[-1]);

    int num_read, i;

    for (i = 0; MAX_PROBE_BUFFERS > i; ++i) {
      num_read = m_io->read(m_buffer->get(), READ_SIZE);
      if (0 == num_read)
        throw error_c(Y("avc_es_reader: Should not have happened."));
      parser.add_bytes(m_buffer->get(), num_read);
      if (parser.headers_parsed())
        break;
    }

    if (parser.headers_parsed())
      parser.flush();

    m_avcc   = parser.get_avcc();
    m_width  = parser.get_width();
    m_height = parser.get_height();

    m_io->setFilePointer(0, seek_beginning);

  } catch (...) {
    throw error_c(Y("avc_es_reader: Could not open the source file."));
  }

  if (verbose)
    mxinfo_fn(ti.fname, Y("Using the AVC/h.264 ES demultiplexer.\n"));
}

void
avc_es_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;

  add_packetizer(new mpeg4_p10_es_video_packetizer_c(this, ti, m_avcc, m_width, m_height));

  mxinfo_tid(ti.fname, 0, Y("Using the MPEG-4 part 10 ES video output module.\n"));
}

file_status_e
avc_es_reader_c::read(generic_packetizer_c *,
                      bool) {
  int num_read;

  if (m_bytes_processed >= m_size)
    return FILE_STATUS_DONE;

  num_read = m_io->read(m_buffer->get(), READ_SIZE);
  if (0 >= num_read)
    return FILE_STATUS_DONE;

  PTZR0->process(new packet_t(new memory_c(m_buffer->get(), num_read)));

  m_bytes_processed += num_read;

  if ((READ_SIZE != num_read) || (m_bytes_processed >= m_size))
    PTZR0->flush();

  return READ_SIZE == num_read ? FILE_STATUS_MOREDATA : FILE_STATUS_DONE;
}

int
avc_es_reader_c::get_progress() {
  return 100 * m_bytes_processed / m_size;
}

void
avc_es_reader_c::identify() {
  id_result_container("AVC/h.264 elementary stream (ES)");
  id_result_track(0, ID_RESULT_TRACK_VIDEO, "MPEG-4 part 10 ES", "packetizer:mpeg4_p10_es_video");
}

