/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   HEVC/h2.65 ES demultiplexer module

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
#include "common/codec.h"
#include "common/error.h"
#include "common/memory.h"
#include "input/r_hevc.h"
#include "merge/output_control.h"
#include "output/p_hevc_es.h"


#define PROBESIZE 4
#define READ_SIZE 1024 * 1024
#define MAX_PROBE_BUFFERS 50

using namespace hevc;

int
hevc_es_reader_c::probe_file(mm_io_c *in,
                            uint64_t size) {
  try {
    if (PROBESIZE > size)
      return 0;

    memory_cptr buf = memory_c::alloc(READ_SIZE);
    int num_read, i;
    bool first = true;

    hevc_es_parser_c parser;
    parser.ignore_nalu_size_length_errors();
    parser.set_nalu_size_length(4);

    in->setFilePointer(0, seek_beginning);
    for (i = 0; MAX_PROBE_BUFFERS > i; ++i) {
      num_read = in->read(buf->get_buffer(), READ_SIZE);
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

  } catch (mtx::exception &e) {
    mxinfo(boost::format(Y("Error %1%\n")) % e.error());

  } catch (...) {
    mxinfo(Y("have an xcptn\n"));
  }

  return 0;
}

hevc_es_reader_c::hevc_es_reader_c(const track_info_c &ti,
                                 const mm_io_cptr &in)
  : generic_reader_c(ti, in)
  , m_buffer(memory_c::alloc(READ_SIZE))
{
}

void
hevc_es_reader_c::read_headers() {
  try {
    hevc_es_parser_c parser;
    parser.ignore_nalu_size_length_errors();

    int num_read, i;

    for (i = 0; MAX_PROBE_BUFFERS > i; ++i) {
      num_read = m_in->read(m_buffer->get_buffer(), READ_SIZE);
      if (0 == num_read)
        throw mtx::exception();
      parser.add_bytes(m_buffer->get_buffer(), num_read);
      if (parser.headers_parsed())
        break;
    }

    if (parser.headers_parsed())
      parser.flush();

    m_width  = parser.get_width();
    m_height = parser.get_height();

    m_in->setFilePointer(0, seek_beginning);

  } catch (...) {
    throw mtx::input::open_x();
  }

  show_demuxer_info();
}

void
hevc_es_reader_c::create_packetizer(int64_t) {
  if (!demuxing_requested('v', 0) || (NPTZR() != 0))
    return;

  add_packetizer(new hevc_es_video_packetizer_c(this, m_ti));
  PTZR0->set_video_pixel_dimensions(m_width, m_height);

  show_packetizer_info(0, PTZR0);
}

file_status_e
hevc_es_reader_c::read(generic_packetizer_c *,
                      bool) {
  if (m_in->getFilePointer() >= m_size)
    return FILE_STATUS_DONE;

  int num_read = m_in->read(m_buffer->get_buffer(), READ_SIZE);
  if (0 < num_read)
    PTZR0->process(new packet_t(new memory_c(m_buffer->get_buffer(), num_read)));

  return (0 != num_read) && (m_in->getFilePointer() < m_size) ? FILE_STATUS_MOREDATA : flush_packetizers();
}

void
hevc_es_reader_c::identify() {
  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_VIDEO, codec_c::get_name(CT_V_MPEGH_P2, "HEVC"), "packetizer:mpegh_p2_es_video");
}
