/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the VobSub output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_VOBSUB_H
#define __P_VOBSUB_H

#include "common/common_pch.h"

#include "common/compression.h"
#include "merge/pr_generic.h"

class vobsub_packetizer_c: public generic_packetizer_c {
private:
  memory_cptr idx_data;

public:
  vobsub_packetizer_c(generic_reader_c *_reader, const void *_idx_data, int _idx_data_size, track_info_c &_ti);
  virtual ~vobsub_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("VobSub") : "VobSub";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src,
                                             std::string &error_message);
};

#endif // __P_VOBSUB_H
