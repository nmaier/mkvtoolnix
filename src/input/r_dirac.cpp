/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Dirac demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/os.h"

#include "common/byte_buffer.h"
#include "common/common.h"
#include "common/endian.h"
#include "common/error.h"
#include "input/r_dirac.h"
#include "merge/output_control.h"
#include "output/p_dirac.h"


#define PROBESIZE 4
#define READ_SIZE 1024 * 1024

int
dirac_es_reader_c::probe_file(mm_io_c *io,
                              int64_t size) {
  try {
    if (PROBESIZE > size)
      return 0;

    io->setFilePointer(0, seek_beginning);

    memory_cptr buf = memory_c::alloc(READ_SIZE);
    int num_read    = io->read(buf->get(), READ_SIZE);

    if (4 > num_read)
      return 0;

    uint32_t marker = get_uint32_be(buf->get());
    if (DIRAC_SYNC_WORD != marker)
      return 0;

    dirac::es_parser_c parser;
    parser.add_bytes(buf->get(), num_read);

    return parser.is_sequence_header_available();

  } catch (...) {
    mxinfo("have an xcptn\n");
  }

  return 0;
}

dirac_es_reader_c::dirac_es_reader_c(track_info_c &n_ti)
  throw (error_c)
  : generic_reader_c(n_ti)
  , m_bytes_processed(0)
  , m_buffer(memory_c::alloc(READ_SIZE))
{

  try {
    m_io   = counted_ptr<mm_io_c>(new mm_file_io_c(ti.fname));
    m_size = m_io->get_size();

    dirac::es_parser_c parser;

    int num_read = m_io->read(m_buffer->get(), READ_SIZE);
    parser.add_bytes(m_buffer->get(), num_read);

    if (!parser.is_sequence_header_available())
      throw false;

    parser.get_sequence_header(m_seqhdr);

    m_io->setFilePointer(0, seek_beginning);

  } catch (...) {
    throw error_c(Y("dirac_es_reader: Could not open the source file."));
  }

  if (verbose)
    mxinfo_fn(ti.fname, Y("Using the Dirac demultiplexer.\n"));
}

void
dirac_es_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;

  add_packetizer(new dirac_video_packetizer_c(this, ti));

  mxinfo_tid(ti.fname, 0, Y("Using the Dirac video output module.\n"));
}

file_status_e
dirac_es_reader_c::read(generic_packetizer_c *,
                        bool) {
  if (m_bytes_processed >= m_size)
    return FILE_STATUS_DONE;

  int num_read = m_io->read(m_buffer->get(), READ_SIZE);
  if (0 >= num_read)
    return FILE_STATUS_DONE;

  PTZR0->process(new packet_t(new memory_c(m_buffer->get(), num_read)));

  m_bytes_processed += num_read;

  if ((READ_SIZE != num_read) || (m_bytes_processed >= m_size))
    PTZR0->flush();

  return READ_SIZE == num_read ? FILE_STATUS_MOREDATA : FILE_STATUS_DONE;
}

int
dirac_es_reader_c::get_progress() {
  return 100 * m_bytes_processed / m_size;
}

void
dirac_es_reader_c::identify() {
  id_result_container("Dirac elementary stream");
  id_result_track(0, ID_RESULT_TRACK_VIDEO, "Dirac");
}
