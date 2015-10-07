/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the AAC ADIF demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_R_AAC_ADIF_H
#define MTX_R_AAC_ADIF_H

#include "common/common_pch.h"

#include "common/error.h"
#include "common/mm_io.h"

class aac_adif_reader_c {
public:
  static int probe_file(mm_io_c *io, uint64_t size);
};

#endif // MTX_R_AAC_ADIF_H
