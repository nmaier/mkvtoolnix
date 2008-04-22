/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id: r_aac.cpp 3780 2008-03-09 16:02:08Z mosu $

   ASF demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include "common.h"
#include "r_asf.h"

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

    if (get_uint32_be(buf) == 0x3026b275) {
      mxerror(mxsprintf("The file '%s' is an ASF/WMV container which is not supported by mkvmerge.\n", io->get_file_name().c_str()).c_str());
      // Never reached:
      return 1;
    }

    return 0;

  } catch (...) {
    return 0;
  }
}
