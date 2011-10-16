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
dts_reader_c::probe_file(mm_io_c *io,
                         uint64_t size) {
  if (size < READ_SIZE)
    return 0;

  try {
    unsigned char buf[READ_SIZE];
    bool dts14_to_16 = false, swap_bytes = false;

    io->setFilePointer(0, seek_beginning);
    if (io->read(buf, READ_SIZE) != READ_SIZE)
      return 0;
    io->setFilePointer(0, seek_beginning);

    if (detect_dts(buf, READ_SIZE, dts14_to_16, swap_bytes))
      return 1;

  } catch (...) {
  }

  return 0;
}

dts_reader_c::dts_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti),
  cur_buf(0),
  dts14_to_16(false),
  swap_bytes(false) {

  try {
    io     = new mm_file_io_c(m_ti.m_fname);
    size   = io->get_size();
    buf[0] = (unsigned short *)safemalloc(READ_SIZE);
    buf[1] = (unsigned short *)safemalloc(READ_SIZE);

    if (io->read(buf[cur_buf], READ_SIZE) != READ_SIZE)
      throw error_c(boost::format(Y("dts_reader: Could not read %1% bytes.")) % READ_SIZE);
    io->setFilePointer(0, seek_beginning);

  } catch (...) {
    throw error_c(Y("dts_reader: Could not open the source file."));
  }

  detect_dts(buf[cur_buf], READ_SIZE, dts14_to_16, swap_bytes);

  mxverb(3, boost::format("DTS: 14->16 %1% swap %2%\n") % dts14_to_16 % swap_bytes);

  decode_buffer(READ_SIZE);
  int pos = find_dts_header((const unsigned char *)buf[cur_buf], READ_SIZE, &dtsheader);

  if (0 > pos)
    throw error_c(boost::format(Y("dts_reader: No valid DTS packet found in the first %1% bytes.\n")) % READ_SIZE);

  bytes_processed = 0;
  m_ti.m_id       = 0;          // ID for this track.

  if (verbose)
    mxinfo_fn(m_ti.m_fname, Y("Using the DTS demultiplexer.\n"));
}

dts_reader_c::~dts_reader_c() {
  delete io;
  safefree(buf[0]);
  safefree(buf[1]);
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
  mxinfo_tid(m_ti.m_fname, 0, Y("Using the DTS output module.\n"));

  if (1 < verbose)
    print_dts_header(&dtsheader);
}

file_status_e
dts_reader_c::read(generic_packetizer_c *,
                   bool) {
  int nread  = io->read(buf[cur_buf], READ_SIZE);

  if (dts14_to_16)
    nread &= ~0xf;

  if (0 >= nread)
    return flush_packetizers();

  int num_to_output = decode_buffer(nread);

  PTZR0->process(new packet_t(new memory_c(buf[cur_buf], num_to_output, false)));
  bytes_processed += nread;

  return ((nread < READ_SIZE) || io->eof()) ? flush_packetizers() : FILE_STATUS_MOREDATA;
}

int
dts_reader_c::get_progress() {
  return 100 * bytes_processed / size;
}

void
dts_reader_c::identify() {
  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_AUDIO, "DTS");
}
