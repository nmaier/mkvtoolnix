/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes
  
   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html
  
   $Id$
   
   DTS demultiplexer module
  
   Written by Peter Niemayer <niemayer@isg.de>.
   Modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "error.h"
#include "r_dts.h"
#include "p_dts.h"

int
dts_reader_c::probe_file(mm_io_c *mm_io,
                         int64_t size) {
  char buf[max_dts_packet_size];
  int pos;
  dts_header_t dtsheader;

  if (size < max_dts_packet_size)
    return 0;
  try {
    mm_io->setFilePointer(0, seek_beginning);
    if (mm_io->read(buf, max_dts_packet_size) != max_dts_packet_size)
      return 0;
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    return 0;
  }

  pos = find_dts_header((unsigned char *)buf, max_dts_packet_size, &dtsheader);
  if (pos < 0)
    return 0;

  return 1;
}

dts_reader_c::dts_reader_c(track_info_c *nti)
  throw (error_c):
  generic_reader_c(nti) {
  int pos;

  try {
    mm_io = new mm_file_io_c(ti->fname);
    size = mm_io->get_size();
    chunk = (unsigned char *)safemalloc(max_dts_packet_size);
    if (mm_io->read(chunk, max_dts_packet_size) != max_dts_packet_size)
      throw error_c("dts_reader: Could not read max_dts_packet_size bytes.");
    mm_io->setFilePointer(0, seek_beginning);
  } catch (exception &ex) {
    throw error_c("dts_reader: Could not open the source file.");
  }

  pos = find_dts_header(chunk, max_dts_packet_size, &dtsheader);

  if (pos < 0)
    throw error_c("dts_reader: No valid DTS packet found in the first "
                  "max_dts_packet_size bytes.\n");
  bytes_processed = 0;
  ti->id = 0;                   // ID for this track.

  if (verbose)
    mxinfo(FMT_FN "Using the DTS demultiplexer.\n", ti->fname.c_str());
}

dts_reader_c::~dts_reader_c() {
  delete mm_io;
  safefree(chunk);
}

void
dts_reader_c::create_packetizer(int64_t) {
  if (NPTZR() != 0)
    return;
  add_packetizer(new dts_packetizer_c(this, dtsheader, ti));
  mxinfo(FMT_TID "Using the DTS output module.\n", ti->fname.c_str(),
         (int64_t)0);
  print_dts_header(&dtsheader);
}

file_status_e
dts_reader_c::read(generic_packetizer_c *,
                   bool) {
  int nread;

  nread = mm_io->read(chunk, max_dts_packet_size);
  if (nread <= 0) {
    PTZR0->flush();
    return FILE_STATUS_DONE;
  }

  memory_c mem(chunk, nread, false);
  PTZR0->process(mem);
  bytes_processed += nread;

  return FILE_STATUS_MOREDATA;
}

int
dts_reader_c::get_progress() {
  return 100 * bytes_processed / size;
}

void
dts_reader_c::identify() {
  mxinfo("File '%s': container: DTS\nTrack ID 0: audio (DTS)\n",
         ti->fname.c_str());
}
