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

#include <stdio.h>

#include "common/error.h"
#include "common/mm_io.h"
#include "merge/pr_generic.h"

class hdsub_reader_c: public generic_reader_c {
public:
  hdsub_reader_c(track_info_c &n_ti): generic_reader_c(n_ti) { };

  static int probe_file(mm_io_c *in, uint64_t size);
};

#endif // __R_HDSUB_H
