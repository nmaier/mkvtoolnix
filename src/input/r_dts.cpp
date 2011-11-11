/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   DTS demultiplexer module

   Written by Peter Niemayer <niemayer@isg.de>.
   Modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/dts.h"
#include "common/error.h"
#include "input/r_dts.h"
#include "output/p_dts.h"

#define READ_SIZE 16384

int
dts_reader_c::probe_file(mm_io_c *in,
                         uint64_t size) {
  if (size < READ_SIZE)
    return 0;

  try {
    unsigned char buf[READ_SIZE];
    bool dts14_to_16 = false, swap_bytes = false;

    in->setFilePointer(0, seek_beginning);
    if (in->read(buf, READ_SIZE) != READ_SIZE)
      return 0;
    in->setFilePointer(0, seek_beginning);

    if (detect_dts(buf, READ_SIZE, dts14_to_16, swap_bytes))
      return 1;

  } catch (...) {
  }

  return 0;
}

dts_reader_c::dts_reader_c(const track_info_c &ti,
                           const mm_io_cptr &in)
  : generic_reader_c(ti, in)
  , m_af_buf(memory_c::alloc(2 * READ_SIZE))
  , cur_buf(0)
  , dts14_to_16(false)
  , swap_bytes(false)
{
  buf[0] = reinterpret_cast<unsigned short *>(m_af_buf->get_buffer());
  buf[1] = reinterpret_cast<unsigned short *>(m_af_buf->get_buffer() + READ_SIZE);
}

void
dts_reader_c::read_headers() {
  try {
    if (m_in->read(buf[cur_buf], READ_SIZE) != READ_SIZE)
      throw mtx::input::header_parsing_x();
    m_in->setFilePointer(0, seek_beginning);

  } catch (...) {
    throw mtx::input::open_x();
  }

  detect_dts(buf[cur_buf], READ_SIZE, dts14_to_16, swap_bytes);

  mxverb(3, boost::format("DTS: 14->16 %1% swap %2%\n") % dts14_to_16 % swap_bytes);

  decode_buffer(READ_SIZE);
  int pos = find_dts_header((const unsigned char *)buf[cur_buf], READ_SIZE, &dtsheader);

  if (0 > pos)
    throw mtx::input::header_parsing_x();

  m_ti.m_id = 0;          // ID for this track.

  show_demuxer_info();
}

dts_reader_c::~dts_reader_c() {
}

int
dts_reader_c::decode_buffer(int len) {
  if (swap_bytes) {
    swab((char *)buf[cur_buf], (char *)buf[cur_buf ^ 1], len);
    cur_buf ^= 1;
  }

  if (dts14_to_16) {
    dts_14_to_dts_16(buf[cur_buf], len / 2, buf[cur_buf ^ 1]);
    cur_buf ^= 1;
    len      = len * 7 / 8;
  }

  return len;
}

void
dts_reader_c::create_packetizer(int64_t) {
  if (!demuxing_requested('a', 0) || (NPTZR() != 0))
    return;

  add_packetizer(new dts_packetizer_c(this, m_ti, dtsheader));
  show_packetizer_info(0, PTZR0);

  if (1 < verbose)
    print_dts_header(&dtsheader);
}

file_status_e
dts_reader_c::read(generic_packetizer_c *,
                   bool) {
  int nread = m_in->read(buf[cur_buf], READ_SIZE);

  if (dts14_to_16)
    nread &= ~0xf;

  if (0 >= nread)
    return flush_packetizers();

  int num_to_output = decode_buffer(nread);

  PTZR0->process(new packet_t(new memory_c(buf[cur_buf], num_to_output, false)));

  return ((nread < READ_SIZE) || m_in->eof()) ? flush_packetizers() : FILE_STATUS_MOREDATA;
}

void
dts_reader_c::identify() {
  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_AUDIO, "DTS");
}
