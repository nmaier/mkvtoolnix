/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes
  
   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html
  
   $Id$
   
   WAVPACK demultiplexer module
  
   Written by Steve Lhomme <steve.lhomme@free.fr>.
   Modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "error.h"
#include "output_control.h"
#include "p_wavpack.h"
#include "r_wavpack.h"

int
wavpack_reader_c::probe_file(mm_io_c *mm_io,
                             int64_t size) {
  wavpack_header_t header;

  try {
    mm_io->setFilePointer(0, seek_beginning);
    if (mm_io->read(&header, sizeof(wavpack_header_t)) !=
        sizeof(wavpack_header_t))
      return 0;
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    return 0;
  }
  if (!strncmp(header.ck_id, "wvpk", 4)) {
    header.version = get_uint16(&header.version);
    if ((header.version >> 8) == 4)
      return 1;
  }
  return 0;
}

wavpack_reader_c::wavpack_reader_c(track_info_c *nti)
  throw (error_c):
  generic_reader_c(nti) {

  try {
    mm_io = new mm_file_io_c(ti->fname);
    size = mm_io->get_size();

    int32_t packet_size = wv_parse_frame(mm_io, header, meta);
    if (packet_size < 0)
      mxerror(FMT_FN "The file header was not read correctly.\n",
              ti->fname.c_str());

    mm_io->setFilePointer(mm_io->getFilePointer() - sizeof(wavpack_header_t),
                          seek_beginning);

  } catch (exception &ex) {
    throw error_c("wavpack_reader: Could not open the file.");
  }
  if (verbose)
    mxinfo(FMT_FN "Using the WAVPACK demultiplexer.\n", ti->fname.c_str());
}

wavpack_reader_c::~wavpack_reader_c() {
  delete mm_io;
}

void
wavpack_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;
  add_packetizer(new wavpack_packetizer_c(this, meta, ti));

  mxinfo(FMT_TID "Using the WAVPACK output module.\n", ti->fname.c_str(),
         (int64_t)0);
}

file_status_e
wavpack_reader_c::read(generic_packetizer_c *ptzr,
                       bool force) {
  wavpack_header_t dummy_header;
  wavpack_meta_t dummy_meta;
  int32_t data_size = wv_parse_frame(mm_io, dummy_header, dummy_meta);

  if (data_size < 0)
    return FILE_STATUS_DONE;

  uint8_t *chunk = (uint8_t *)safemalloc(data_size + sizeof(wavpack_header_t) -
                                         WV_KEEP_HEADER_POSITION);

  // keep the header minus the ID & size (all found in Matroska)
  put_uint16(chunk, dummy_header.version);
  chunk[2] = dummy_header.track_no;
  chunk[3] = dummy_header.index_no;
  put_uint32(&chunk[4], dummy_header.total_samples);
  put_uint32(&chunk[8], dummy_header.block_index);
  put_uint32(&chunk[12], dummy_header.block_samples);
  put_uint32(&chunk[16], dummy_header.flags);
  put_uint32(&chunk[20], dummy_header.crc);

  if (mm_io->read(&chunk[sizeof(wavpack_header_t) - WV_KEEP_HEADER_POSITION],
                  data_size) < 0)
    return FILE_STATUS_DONE;

  memory_c mem(chunk, data_size + sizeof(wavpack_header_t) -
               WV_KEEP_HEADER_POSITION, true);
  PTZR0->process(mem);

  return FILE_STATUS_MOREDATA;
}

int
wavpack_reader_c::get_progress() {
  return 100 * mm_io->getFilePointer() / size;
}

void
wavpack_reader_c::identify() {
  mxinfo("File '%s': container: WAVPACK\nTrack ID 0: audio (WAVPACK)\n",
         ti->fname.c_str());
}
