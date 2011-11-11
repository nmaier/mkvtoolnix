/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the HDSUB demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_HDSUB_H
#define __R_HDSUB_H

#include "common/common_pch.h"

class hdsub_reader_c {
public:
  static int probe_file(mm_io_c *in, uint64_t size);
};

#endif // __R_HDSUB_H
