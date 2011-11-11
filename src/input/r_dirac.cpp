/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Dirac demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/byte_buffer.h"
#include "common/endian.h"
#include "common/error.h"
#include "input/r_dirac.h"
#include "merge/output_control.h"
#include "output/p_dirac.h"


#define PROBESIZE 4
#define READ_SIZE 1024 * 1024

int
dirac_es_reader_c::probe_file(mm_io_c *in,
                              uint64_t size) {
  try {
    if (PROBESIZE > size)
      return 0;

    in->setFilePointer(0, seek_beginning);

    memory_cptr buf = memory_c::alloc(READ_SIZE);
    int num_read    = in->read(buf->get_buffer(), READ_SIZE);

    if (4 > num_read)
      return 0;

    uint32_t marker = get_uint32_be(buf->get_buffer());
    if (DIRAC_SYNC_WORD != marker)
      return 0;

    dirac::es_parser_c parser;
    parser.add_bytes(buf->get_buffer(), num_read);

    return parser.is_sequence_header_available();

  } catch (...) {
    mxinfo("have an xcptn\n");
  }

  return 0;
}

dirac_es_reader_c::dirac_es_reader_c(const track_info_c &ti,
                                     const mm_io_cptr &in)
  : generic_reader_c(ti, in)
  , m_buffer(memory_c::alloc(READ_SIZE))
{
}

void
dirac_es_reader_c::read_headers() {
  try {
    dirac::es_parser_c parser;

    int num_read = m_in->read(m_buffer->get_buffer(), READ_SIZE);
    parser.add_bytes(m_buffer->get_buffer(), num_read);

    if (!parser.is_sequence_header_available())
      throw false;

    parser.get_sequence_header(m_seqhdr);

    m_in->setFilePointer(0, seek_beginning);

  } catch (...) {
    throw mtx::input::open_x();
  }

  show_demuxer_info();
}

void
dirac_es_reader_c::create_packetizer(int64_t) {
  if (!demuxing_requested('v', 0) || (NPTZR() != 0))
    return;

  add_packetizer(new dirac_video_packetizer_c(this, m_ti));

  show_packetizer_info(0, PTZR0);
}

file_status_e
dirac_es_reader_c::read(generic_packetizer_c *,
                        bool) {
  if (m_in->getFilePointer() >= m_size)
    return flush_packetizers();

  int num_read = m_in->read(m_buffer->get_buffer(), READ_SIZE);
  if (0 < num_read)
    PTZR0->process(new packet_t(new memory_c(m_buffer->get_buffer(), num_read)));

  return ((READ_SIZE != num_read) || (m_in->getFilePointer() >= m_size)) ? flush_packetizers() : FILE_STATUS_MOREDATA;
}

void
dirac_es_reader_c::identify() {
  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_VIDEO, "Dirac");
}
