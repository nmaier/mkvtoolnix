/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes
  
   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html
  
   $Id$
  
   class definitions for the TTA demultiplexer module
  
   Written by Steve Lhomme <steve.lhomme@free.fr>.
*/

#ifndef __R_WAVPACK_H
#define __R_WAVPACK_H

#include "os.h"

#include <stdio.h>

#include "common.h"
#include "error.h"
#include "mm_io.h"
#include "pr_generic.h"
#include "wavpack_common.h"

class wavpack_reader_c: public generic_reader_c {
private:
  mm_io_c *mm_io;
  int64_t size;
  wavpack_header_t header;
  wavpack_meta_t meta;

public:
  wavpack_reader_c(track_info_c *nti) throw (error_c);
  virtual ~wavpack_reader_c();

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual int get_progress();
  virtual void identify();
  virtual void create_packetizer(int64_t id);

  static int probe_file(mm_io_c *mm_io, int64_t size);
};

#endif // __R_WAVPACK_H
