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

pgssup_reader_c::pgssup_reader_c(const track_info_c &ti,
                                 const mm_io_cptr &in)
  : generic_reader_c(ti, in)
  , m_debug{"pgssup_reader"}
{
}

void
pgssup_reader_c::read_headers() {
  m_ti.m_id = 0;       // ID for this track.

  show_demuxer_info();
}

pgssup_reader_c::~pgssup_reader_c() {
}

void
pgssup_reader_c::create_packetizer(int64_t) {
  if (!demuxing_requested('s', 0) || (NPTZR() != 0))
    return;

  pgs_packetizer_c *ptzr = new pgs_packetizer_c(this, m_ti);
  ptzr->set_aggregate_packets(true);
  add_packetizer(ptzr);

  show_packetizer_info(0, ptzr);
}

file_status_e
pgssup_reader_c::read(generic_packetizer_c *,
                      bool) {
  try {
    if (m_debug)
      mxinfo(boost::format("pgssup_reader_c::read(): ---------- start read at %1%\n") % m_in->getFilePointer());

    if (PGSSUP_FILE_MAGIC != m_in->read_uint16_be())
      return flush_packetizers();

    uint64_t timestamp = static_cast<uint64_t>(m_in->read_uint32_be()) * 100000Lu / 9;
    m_in->skip(4);

    memory_cptr frame = memory_c::alloc(3);
    if (3 != m_in->read(frame->get_buffer(), 3))
      return flush_packetizers();

    unsigned int segment_size = get_uint16_be(frame->get_buffer() + 1);
    frame->resize(3 + segment_size);

    if (segment_size != m_in->read(frame->get_buffer() + 3, segment_size))
      return flush_packetizers();

    if (m_debug)
      mxinfo(boost::format("pgssup_reader_c::read(): type %|1$02x| size %2% at %3%\n") % static_cast<unsigned int>(frame->get_buffer()[0]) % segment_size % (m_in->getFilePointer() - 10 - 3));

    PTZR0->process(new packet_t(frame, timestamp));

  } catch (...) {
    if (m_debug)
      mxinfo("pgssup_reader_c::read(): exception\n");

    return flush_packetizers();
  }

  return FILE_STATUS_MOREDATA;
}

void
pgssup_reader_c::identify() {
  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_SUBTITLES, "PGS");
}
