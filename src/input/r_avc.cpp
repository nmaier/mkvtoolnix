/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

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
#include "r_avc.h"
#include "p_avc.h"

using namespace std;

#define PROBESIZE 4
#define READ_SIZE 1024 * 1024

using namespace mpeg4::p10;

int
avc_es_reader_c::probe_file(mm_io_c *io,
                            int64_t size) {
  unsigned char *buf;
  int num_read;

  if (size < PROBESIZE)
    return 0;
  try {
    buf = (unsigned char *)safemalloc(READ_SIZE);
    io->setFilePointer(0, seek_beginning);
    num_read = io->read(buf, READ_SIZE);
    if (num_read < 4) {
      safefree(buf);
      return 0;
    }
    io->setFilePointer(0, seek_beginning);

    avc_es_parser_c parser;
    parser.enable_timecode_generation(40000000);
    parser.add_bytes(buf, num_read);

    return parser.headers_parsed();

  } catch (...) {
    return 0;
  }

  return 1;
}

avc_es_reader_c::avc_es_reader_c(track_info_c &n_ti)
  throw (error_c):
  generic_reader_c(n_ti),
  m_bytes_processed(0),
  m_buffer(new memory_c((unsigned char *)safemalloc(READ_SIZE), READ_SIZE)) {

  try {
    m_io = counted_ptr<mm_io_c>(new mm_file_io_c(ti.fname));
    m_size = m_io->get_size();

    int num_read = m_io->read(m_buffer->get(), READ_SIZE);

    avc_es_parser_c parser;
    parser.enable_timecode_generation(40000000);
    parser.add_bytes(m_buffer->get(), num_read);
    m_avcc = parser.get_avcc();
    m_width = parser.get_width();
    m_height = parser.get_height();

    m_io->setFilePointer(0, seek_beginning);
  } catch (...) {
    throw error_c("avc_es_reader: Could not open the source file.");
  }

  if (verbose)
    mxinfo(FMT_FN "Using the AVC/h.264 ES demultiplexer.\n", ti.fname.c_str());
}

void
avc_es_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;

  add_packetizer(new mpeg4_p10_es_video_packetizer_c(this, m_avcc, m_width,
                                                     m_height, ti));

  mxinfo(FMT_TID "Using the MPEG-4 part 10 ES video output module.\n",
         ti.fname.c_str(), (int64_t)0);
}

file_status_e
avc_es_reader_c::read(generic_packetizer_c *,
                      bool) {
  int num_read;

//   mxinfo("read proc " LLD " size " LLD "\n", m_bytes_processed, m_size);

  if (m_bytes_processed >= m_size)
    return FILE_STATUS_DONE;

  num_read = m_io->read(m_buffer->get(), READ_SIZE);
  if (0 >= num_read)
    return FILE_STATUS_DONE;

  PTZR0->process(new packet_t(new memory_c(m_buffer->get(), num_read)));

  m_bytes_processed += num_read;

  if (READ_SIZE != num_read)
    PTZR0->flush();

  return READ_SIZE == num_read ? FILE_STATUS_MOREDATA : FILE_STATUS_DONE;
}

int
avc_es_reader_c::get_progress() {
  return 100 * m_bytes_processed / m_size;
}

void
avc_es_reader_c::identify() {
  mxinfo("File '%s': container: AVC/h.264 elementary stream (ES)\n"
         "Track ID 0: video (MPEG-4 part 10 ES)\n", ti.fname.c_str());
}

