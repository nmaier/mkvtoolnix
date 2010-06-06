/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Macromedia Flash Video (FLV) demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "input/r_flv.h"

int
flv_reader_c::probe_file(mm_io_c *io,
                         uint64_t size) {
  try {
    if (3 > size)
      return 0;

    unsigned char buf[3];

    io->setFilePointer(0, seek_beginning);
    if (io->read(buf, 3) != 3)
      return 0;
    io->setFilePointer(0, seek_beginning);

    if (!memcmp(buf, "FLV", 3)) {
      id_result_container_unsupported(io->get_file_name(), "Macromedia Flash Video (FLV)");
      // Never reached:
      return 1;
    }

    return 0;

  } catch (...) {
    return 0;
  }
}
