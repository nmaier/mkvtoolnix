/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   ASF demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/endian.h"
#include "input/r_asf.h"
#include "merge/pr_generic.h"

#define MAGIC_ASF_WMV 0x3026b275

int
asf_reader_c::probe_file(mm_io_c *in,
                         uint64_t size) {
  try {
    if (4 > size)
      return 0;

    unsigned char buf[4];

    in->setFilePointer(0, seek_beginning);
    if (in->read(buf, 4) != 4)
      return 0;
    in->setFilePointer(0, seek_beginning);

    if (MAGIC_ASF_WMV == get_uint32_be(buf)) {
      id_result_container_unsupported(in->get_file_name(), "Windows Media (ASF/WMV)");
      // Never reached:
      return 1;
    }

    return 0;

  } catch (...) {
    return 0;
  }
}
