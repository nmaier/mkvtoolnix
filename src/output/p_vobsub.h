/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes
  
   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html
  
   $Id$
  
   class definition for the VobSub output module
  
   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_VOBSUB_H
#define __P_VOBSUB_H

#include "os.h"

#include "common.h"
#include "compression.h"
#include "pr_generic.h"

class vobsub_packetizer_c: public generic_packetizer_c {
private:
  unsigned char *idx_data;
  int idx_data_size;

public:
  vobsub_packetizer_c(generic_reader_c *nreader,
                      const void *nidx_data, int nidx_data_size,
                      track_info_c *nti)
    throw (error_c);
  virtual ~vobsub_packetizer_c();

  virtual int process(memory_c &mem,
                      int64_t old_timecode = -1, int64_t duration = -1,
                      int64_t bref = -1, int64_t fref = -1);
  virtual void set_headers();

  virtual void dump_debug_info();

  virtual const char *get_format_name() {
    return "VobSub";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src);
};

#endif // __P_VOBSUB_H
