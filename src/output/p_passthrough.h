/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the pass through output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_P_PASSTHROUGH_H
#define MTX_P_PASSTHROUGH_H

#include "common/common_pch.h"

#include "merge/pr_generic.h"

class passthrough_packetizer_c: public generic_packetizer_c {
public:
  passthrough_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti);

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual translatable_string_c get_format_name() const {
    return YT("passthrough");
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message);
};

#endif // MTX_P_PASSTHROUGH_H
