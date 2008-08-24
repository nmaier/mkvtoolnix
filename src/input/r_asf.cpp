/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   ASF demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include "common.h"
#include "r_asf.h"

#define MAGIC_ASF_WMV 0x3026b275

int
asf_reader_c::probe_file(mm_io_c *io,
                         int64_t size) {
  try {
    if (4 > size)
      return 0;

    unsigned char buf[4];

    io->setFilePointer(0, seek_beginning);
    if (io->read(buf, 4) != 4)
      return 0;
    io->setFilePointer(0, seek_beginning);

    if (MAGIC_ASF_WMV == get_uint32_be(buf)) {
      id_result_container_unsupported(io->get_file_name(), "Windows Media (ASF/WMV)");
      // Never reached:
      return 1;
    }

    return 0;

  } catch (...) {
    return 0;
  }
}
