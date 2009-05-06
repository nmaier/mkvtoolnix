/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the pass through output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_PASSTHROUGH_H
#define __P_PASSTHROUGH_H

#include "common/os.h"

#include "common/common.h"
#include "merge/pr_generic.h"

class passthrough_packetizer_c: public generic_packetizer_c {
public:
  passthrough_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti)
    throw (error_c);

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual const char *get_format_name() {
    return "passthrough";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src, string &error_message);
};

#endif // __P_PASSTHROUGH_H
