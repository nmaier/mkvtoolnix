/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   VobBtn stream reader

   Written by Steve Lhomme <steve.lhomme@free.fr>.
   Modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "mm_io.h"
#include "output_control.h"
#include "p_vobbtn.h"
#include "r_vobbtn.h"

using namespace std;

int
vobbtn_reader_c::probe_file(mm_io_c *io,
                            int64_t size) {
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
    btn_file = new mm_file_io_c(ti.fname);
    size     = btn_file->get_size();
  } catch (...) {
    throw error_c(Y("vobbtn_reader: Could not open the file."));
  }

  // read the width & height
  btn_file->setFilePointer(8, seek_beginning);
  width  = btn_file->read_uint16_be();
  height = btn_file->read_uint16_be();
  // get ready to read
  btn_file->setFilePointer(16, seek_beginning);

  if (verbose)
    mxinfo_fn(ti.fname, Y("Using the VobBtn button reader.\n"));
}

vobbtn_reader_c::~vobbtn_reader_c() {
  delete btn_file;
}

void
vobbtn_reader_c::create_packetizer(int64_t tid) {
  ti.id = tid;
  if (demuxing_requested('s', tid))
    add_packetizer(new vobbtn_packetizer_c(this, width, height, ti));
}

file_status_e
vobbtn_reader_c::read(generic_packetizer_c *ptzr,
                      bool force) {
  uint8_t tmp[4];

  // _todo_ add some tests on the header and size
  int nread = btn_file->read(tmp, 4);
  if (0 >= nread) {
    PTZR0->flush();
    return FILE_STATUS_DONE;
  }

  uint16_t frame_size = btn_file->read_uint16_be();
  nread               = btn_file->read(chunk, frame_size);
  if (0 >= nread) {
    PTZR0->flush();
    return FILE_STATUS_DONE;
  }

  PTZR0->process(new packet_t(new memory_c(chunk, nread, false)));
  return FILE_STATUS_MOREDATA;
}

int
vobbtn_reader_c::get_progress() {
  return 100 * btn_file->getFilePointer() / size;
}

void
vobbtn_reader_c::identify() {
  id_result_container("VobBtn");
  id_result_track(0, ID_RESULT_TRACK_BUTTONS, (boost::format("MPEG PCI, width %1% / height %2%") % width % height).str());
}
