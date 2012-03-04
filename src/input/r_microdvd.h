/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the MicroDVD subtitle reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef INPUT_R_MICRODVD_H
#define INPUT_R_MICRODVD_H

#include "common/common_pch.h"

#include "merge/pr_generic.h"

class microdvd_reader_c {
public:
  static int probe_file(mm_text_io_c *in, uint64_t size);
};

#endif  // INPUT_R_MICRODVD_H
