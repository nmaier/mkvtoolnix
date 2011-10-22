/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   VobBtn stream reader

   Written by Steve Lhomme <steve.lhomme@free.fr>.
   Modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/endian.h"
#include "common/mm_io.h"
#include "common/strings/formatting.h"
#include "common/strings/parsing.h"
#include "input/r_vobbtn.h"
#include "merge/output_control.h"
#include "output/p_vobbtn.h"


int
vobbtn_reader_c::probe_file(mm_io_c *io,
                            uint64_t size) {
  unsigned char chunk[23];

  try {
    io->setFilePointer(0, seek_beginning);
    if (io->read(chunk, 23) != 23)
      return 0;
    if (strncasecmp((char*)chunk, "butonDVD", 8))
      return 0;
    if ((0x00 != chunk[0x10]) || (0x00 != chunk[0x11]) || (0x01 != chunk[0x12]) || (0xBF != chunk[0x13]))
      return 0;
    if ((0x03 != chunk[0x14]) || (0xD4 != chunk[0x15]) || (0x00 != chunk[0x16]))
      return 0;
    io->setFilePointer(0, seek_beginning);
  } catch (...) {
    return 0;
  }
  return 1;
}

vobbtn_reader_c::vobbtn_reader_c(track_info_c &_ti)
  throw (error_c):
  generic_reader_c(_ti) {

  try {
    btn_file = new mm_file_io_c(m_ti.m_fname);
    size     = btn_file->get_size();
  } catch (...) {
    throw error_c(boost::format(Y("%1%: Could not open the file.")) % get_format_name());
  }

  // read the width & height
  btn_file->setFilePointer(8, seek_beginning);
  width  = btn_file->read_uint16_be();
  height = btn_file->read_uint16_be();
  // get ready to read
  btn_file->setFilePointer(16, seek_beginning);

  show_demuxer_info();
}

vobbtn_reader_c::~vobbtn_reader_c() {
  delete btn_file;
}

void
vobbtn_reader_c::create_packetizer(int64_t tid) {
  m_ti.m_id = tid;
  if (!demuxing_requested('s', tid))
    return;

  add_packetizer(new vobbtn_packetizer_c(this, m_ti, width, height));
  show_packetizer_info(0, PTZR0);
}

file_status_e
vobbtn_reader_c::read(generic_packetizer_c *ptzr,
                      bool force) {
  uint8_t tmp[4];

  // _todo_ add some tests on the header and size
  int nread = btn_file->read(tmp, 4);
  if (0 >= nread)
    return flush_packetizers();

  uint16_t frame_size = btn_file->read_uint16_be();
  nread               = btn_file->read(chunk, frame_size);
  if (0 >= nread)
    return flush_packetizers();

  PTZR0->process(new packet_t(new memory_c(chunk, nread, false)));
  return FILE_STATUS_MOREDATA;
}

int
vobbtn_reader_c::get_progress() {
  return 100 * btn_file->getFilePointer() / size;
}

void
vobbtn_reader_c::identify() {
  id_result_container();
  id_result_track(0, ID_RESULT_TRACK_BUTTONS, (boost::format("MPEG PCI, width %1% / height %2%") % width % height).str());
}
