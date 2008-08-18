/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definitions for the Macromedia Flash Video (FLV) demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_FLV_H
#define __R_FLV_H

#include "os.h"

#include "mm_io.h"
#include "pr_generic.h"

class flv_reader_c: public generic_reader_c {
public:
  static int probe_file(mm_io_c *io, int64_t size);

public:
  flv_reader_c(track_info_c &n_ti): generic_reader_c(n_ti) { };
};

#endif // __R_FLV_H
