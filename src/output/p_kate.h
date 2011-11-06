/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the Kate packetizer

   Written by ogg.k.ogg.k <ogg.k.ogg.k@googlemail.com>.
   Adapted from code by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_KATE_H
#define __P_KATE_H

#include "common/common_pch.h"

#include "merge/pr_generic.h"
#include "common/kate.h"

class kate_packetizer_c: public generic_packetizer_c {
private:
  std::vector<memory_cptr> m_headers;
  memory_cptr m_global_data;

  kate_identification_header_t m_kate_id;

  int64_t m_previous_timecode;

public:
  kate_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, const void *global_data, int global_size);
  virtual ~kate_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("Kate") : "Kate";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message);
};

#endif  // __P_KATE_H
