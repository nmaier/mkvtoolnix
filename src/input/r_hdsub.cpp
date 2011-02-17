/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   HDSUB demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/endian.h"
#include "common/hdsub.h"
#include "input/r_hdsub.h"

int
hdsub_reader_c::probe_file(mm_io_c *io,
                         uint64_t size) {
  try {
    if (2 > size)
      return 0;

    unsigned char buf[2];

    io->setFilePointer(0, seek_beginning);
    if (io->read(buf, 2) != 2)
      return 0;
    io->setFilePointer(0, seek_beginning);

    if (HDSUB_FILE_MAGIC == get_uint16_be(buf)) {
      id_result_container_unsupported(io->get_file_name(), "HD-DVD sub");
      // Never reached:
      return 1;
    }

    return 0;

  } catch (...) {
    return 0;
  }
}
