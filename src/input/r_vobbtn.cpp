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

#define PFX "vobbtn_reader: "

int
vobbtn_reader_c::probe_file(mm_io_c *mm_io,
                            int64_t size) {
  unsigned char chunk[23];

  try {
    mm_io->setFilePointer(0, seek_beginning);
    if (mm_io->read(chunk, 23) != 23)
      return 0;
    if (strncasecmp((char*)chunk, "butonDVD", 8))
      return 0;
    if ((chunk[0x10] != 0x00) || (chunk[0x11] != 0x00) ||
        (chunk[0x12] != 0x01) || (chunk[0x13] != 0xBF))
      return 0;
    if ((chunk[0x14] != 0x03) || (chunk[0x15] != 0xD4) ||
        (chunk[0x16] != 0x00))
      return 0;
    mm_io->setFilePointer(0, seek_beginning);
  } catch (...) {
    return 0;
  }
  return 1;
}

vobbtn_reader_c::vobbtn_reader_c(track_info_c *nti)
  throw (error_c):
  generic_reader_c(nti) {

  try {
    btn_file = new mm_file_io_c(ti->fname);
    size = btn_file->get_size();
  } catch (...) {
    string emsg = PFX "Could not open the button file '";
    emsg += ti->fname;
    emsg += "'.";
    throw error_c(emsg.c_str());
  }

  // read the width & height
  btn_file->setFilePointer(8, seek_beginning);
  width = btn_file->read_uint16_be();
  height = btn_file->read_uint16_be();
  // get ready to read
  btn_file->setFilePointer(16, seek_beginning);

  if (verbose)
    mxinfo(FMT_FN "Using the VobBtn button reader.\n", ti->fname.c_str());
}

vobbtn_reader_c::~vobbtn_reader_c() {
  delete btn_file;
}

void
vobbtn_reader_c::create_packetizer(int64_t tid) {
  ti->id = tid;
  add_packetizer(new vobbtn_packetizer_c(this, width, height, ti));
}

file_status_e
vobbtn_reader_c::read(generic_packetizer_c *ptzr,
                      bool force) {
  uint8_t _tmp[4];
  int nread;
  uint16_t frame_size;

  // _todo_ add some tests on the header and size
  nread = btn_file->read(_tmp, 4);
  if (nread <= 0) {
    PTZR0->flush();
    return FILE_STATUS_DONE;
  }
  frame_size = btn_file->read_uint16_be();

  nread = btn_file->read(chunk, frame_size);
  if (nread <= 0) {
    PTZR0->flush();
    return FILE_STATUS_DONE;
  }

  memory_c mem(chunk, nread, false);
  PTZR0->process(mem);
  return FILE_STATUS_MOREDATA;
}

int
vobbtn_reader_c::get_progress() {
  return 100 * btn_file->getFilePointer() / size;
}

void
vobbtn_reader_c::identify() {
  mxinfo("File '%s': container: VobBtn\n", ti->fname.c_str());
  mxinfo("Track ID 0: MPEG PCI buttons (width %d / height %d)\n", width,
         height);
}
