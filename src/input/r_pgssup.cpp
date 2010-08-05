/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   PGS demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <algorithm>

#include "common/endian.h"
#include "common/pgssup.h"
#include "input/r_pgssup.h"
#include "output/p_pgs.h"
#include "merge/output_control.h"

int
pgssup_reader_c::probe_file(mm_io_c *in,
                            uint64_t size) {
  if (5 > size)
    return 0;

  in->setFilePointer(0);
  if (PGSSUP_FILE_MAGIC != in->read_uint16_be())
    return 0;

  in->skip(4 + 4 + 1);
  uint16_t segment_size = in->read_uint16_be();
  if ((in->getFilePointer() + segment_size + 2) >= size)
    return 0;

  in->setFilePointer(segment_size, seek_current);

  return PGSSUP_FILE_MAGIC != in->read_uint16_be() ? 0 : 1;
}

pgssup_reader_c::pgssup_reader_c(track_info_c &p_ti)
 : generic_reader_c(p_ti)
 , m_bytes_processed(0)
 , m_debug(debugging_requested("pgssup_reader"))
{
  try {
    m_in        = mm_io_cptr(new mm_file_io_c(m_ti.m_fname));
    m_file_size = m_in->get_size();
    m_ti.m_id   = 0;       // ID for this track.

  } catch (...) {
    throw error_c(Y("pgssup_reader: Could not open the file."));
  }

  if (verbose)
    mxinfo_fn(m_ti.m_fname, Y("Using the PGSSUP demultiplexer.\n"));
}

pgssup_reader_c::~pgssup_reader_c() {
}

void
pgssup_reader_c::create_packetizer(int64_t) {
  if (!demuxing_requested('s', 0) || (NPTZR() != 0))
    return;

  add_packetizer(new pgs_packetizer_c(this, m_ti));

  mxinfo_tid(m_ti.m_fname, 0, Y("Using the PGS output module.\n"));
}

file_status_e
pgssup_reader_c::read(generic_packetizer_c *,
                      bool) {
  try {
    if (m_debug)
      mxinfo(boost::format("pgssup_reader_c::read(): ---------- start read at %1%\n") % m_in->getFilePointer());

    memory_cptr frame  = memory_c::alloc(0);
    uint64_t timestamp = 0;

    while (1) {
      if (PGSSUP_FILE_MAGIC != m_in->read_uint16_be())
        return flush_packetizers();

      if (0 == frame->get_size())
        timestamp = static_cast<uint64_t>(m_in->read_uint32_be()) * 100000Lu / 9;
      else
        m_in->skip(4);
      m_in->skip(4);

      unsigned char segment_type = m_in->read_uint8();
      uint16_t segment_size      = m_in->read_uint16_be();
      size_t previous_frame_size = frame->get_size();

      if (m_debug)
        mxinfo(boost::format("pgssup_reader_c::read(): type %|1$02x| size %2% at %3%\n") % static_cast<unsigned int>(segment_type) % segment_size % (m_in->getFilePointer() - 10 - 3));

      frame->resize(previous_frame_size + 3 + segment_size);
      unsigned char *data = frame->get_buffer() + previous_frame_size;
      data[0]             = segment_type;
      put_uint16_be(&data[1], segment_size);

      m_bytes_processed += 10 + 3 + segment_size;

      if ((m_in->read(&data[3], segment_size) != segment_size) || (PGSSUP_DISPLAY_SEGMENT == segment_type)) {
        PTZR0->process(new packet_t(frame, timestamp));
        break;
      }
    }
  } catch (...) {
    if (m_debug)
      mxinfo("pgssup_reader_c::read(): exception\n");

    return flush_packetizers();
  }

  return FILE_STATUS_MOREDATA;
}

int
pgssup_reader_c::get_progress() {
  return 100 * m_bytes_processed / m_file_size;
}

void
pgssup_reader_c::identify() {
  id_result_container("PGSSUP");
  id_result_track(0, ID_RESULT_TRACK_SUBTITLES, "PGS");
}
