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

#include "common/common.h"

#include "merge/pr_generic.h"

class cdxa_reader_c: public generic_reader_c {
public:
  static bool probe_file(mm_io_c *io, int64_t size);

public:
  cdxa_reader_c(track_info_c &n_ti): generic_reader_c(n_ti) { };
};

#endif  // __R_CDXA_H
