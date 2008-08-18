/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definitions for the ASF demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_ASF_H
#define __R_ASF_H

#include "os.h"

#include "mm_io.h"
#include "pr_generic.h"

class asf_reader_c: public generic_reader_c {
public:
  static int probe_file(mm_io_c *io, int64_t size);

public:
  asf_reader_c(track_info_c &n_ti): generic_reader_c(n_ti) { };
};

#endif // __R_ASF_H
