/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the TTA demultiplexer module

   Written by Steve Lhomme <steve.lhomme@free.fr>.
*/

#ifndef __R_WAVPACK_H
#define __R_WAVPACK_H

#include "common/os.h"

#include <stdio.h>

#include "common/common.h"
#include "common/error.h"
#include "common/mm_io.h"
#include "merge/pr_generic.h"
#include "common/wavpack_common.h"

class wavpack_reader_c: public generic_reader_c {
private:
  mm_io_c *io,*io_correc;
  int64_t size;
  wavpack_header_t header, header_correc;
  wavpack_meta_t meta, meta_correc;

public:
  wavpack_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~wavpack_reader_c();

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual int get_progress();
  virtual void identify();
  virtual void create_packetizer(int64_t id);

  static int probe_file(mm_io_c *io, int64_t size);
};

#endif // __R_WAVPACK_H
