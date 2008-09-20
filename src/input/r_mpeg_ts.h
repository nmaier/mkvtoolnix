/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definitions for the MPEG TS demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_MPEG_TS_H
#define __R_MPEG_TS_H

#include "os.h"

#include "pr_generic.h"

class mpeg_ts_reader_c: public generic_reader_c {
protected:
  static int potential_packet_sizes[];

public:
  static bool probe_file(mm_io_c *io, int64_t size);

public:
  mpeg_ts_reader_c(track_info_c &n_ti): generic_reader_c(n_ti) { };
};

#endif  // __T_MPEG_TS_H
