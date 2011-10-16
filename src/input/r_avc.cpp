/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   AVC/h2.64 ES demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include <algorithm>
#include <deque>
#include <list>

#include "common/byte_buffer.h"
#include "common/error.h"
#include "common/memory.h"
#include "input/r_avc.h"
#include "merge/output_control.h"
#include "output/p_avc.h"


#define PROBESIZE 4
#define READ_SIZE 1024 * 1024
#define MAX_PROBE_BUFFERS 50

using namespace mpeg4::p10;

int
avc_es_reader_c::probe_file(mm_io_c *io,
                            uint64_t size) {
  try {
    if (PROBESIZE > size)
      return 0;

    memory_cptr buf = memory_c::alloc(READ_SIZE);
    int num_read, i;
    bool first = true;

    avc_es_parser_c parser;
    parser.ignore_nalu_size_length_errors();
    parser.set_nalu_size_length(4);
    parser.enable_timecode_generation(40000000);

    io->setFilePointer(0, seek_beginning);
    for (i = 0; MAX_PROBE_BUFFERS > i; ++i) {
      num_read = io->read(buf->get_buffer(), READ_SIZE);
      if (4 > num_read)
        return 0;

      // MPEG TS starts with 0x47.
      if (first && (0x47 == buf->get_buffer()[0]))
        return 0;
      first = false;

      parser.add_bytes(buf->get_buffer(), num_read);

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
  throw (error_c)
  : generic_reader_c(n_ti)
  , m_bytes_processed(0)
  , m_buffer(memory_c::alloc(READ_SIZE))
{
  try {
    m_io   = counted_ptr<mm_io_c>(new mm_file_io_c(m_ti.m_fname));
    m_size = m_io->get_size();

    avc_es_parser_c parser;
    parser.ignore_nalu_size_length_errors();
    parser.enable_timecode_generation(40000000);

    if (map_has_key(m_ti.m_nalu_size_lengths, 0))
      parser.set_nalu_size_length(m_ti.m_nalu_size_lengths[0]);
    else if (map_has_key(m_ti.m_nalu_size_lengths, -1))
      parser.set_nalu_size_length(m_ti.m_nalu_size_lengths[-1]);

    int num_read, i;

    for (i = 0; MAX_PROBE_BUFFERS > i; ++i) {
      num_read = m_io->read(m_buffer->get_buffer(), READ_SIZE);
      if (0 == num_read)
        throw error_c(Y("avc_es_reader: Should not have happened."));
      parser.add_bytes(m_buffer->get_buffer(), num_read);
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

  show_demuxer_info();
}

void
avc_es_reader_c::create_packetizer(int64_t) {
  if (!demuxing_requested('v', 0) || (NPTZR() != 0))
    return;

  add_packetizer(new mpeg4_p10_es_video_packetizer_c(this, m_ti, m_avcc, m_width, m_height));

  show_packetizer_info(0, PTZR0);
}

file_status_e
avc_es_reader_c::read(generic_packetizer_c *,
                      bool) {
  int num_read;

  if (m_bytes_processed >= m_size)
    return FILE_STATUS_DONE;

  num_read = m_io->read(m_buffer->get_buffer(), READ_SIZE);
  if (0 < num_read) {
    PTZR0->process(new packet_t(new memory_c(m_buffer->get_buffer(), num_read)));
    m_bytes_processed += num_read;

    if (m_bytes_processed < m_size)
      return FILE_STATUS_MOREDATA;
  }

  return flush_packetizers();
}

int
avc_es_reader_c::get_progress() {
  return 100 * m_bytes_processed / m_size;
}

void
avc_es_reader_c::identify() {
  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_VIDEO, "MPEG-4 part 10 ES", "packetizer:mpeg4_p10_es_video");
}

