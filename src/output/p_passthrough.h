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

#include "common/common_pch.h"

#include "merge/pr_generic.h"

class passthrough_packetizer_c: public generic_packetizer_c {
public:
  passthrough_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti)
    throw (error_c);

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("passthrough") : "passthrough";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message);
};

#endif // __P_PASSTHROUGH_H
