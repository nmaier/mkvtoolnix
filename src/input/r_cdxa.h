/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the RIFF CDXA demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_CDXA_H
#define __R_CDXA_H

#include "common/common_pch.h"

#include "common/mm_io.h"

class cdxa_reader_c {
public:
  static bool probe_file(mm_io_c *in, uint64_t size);
};

#endif  // __R_CDXA_H
