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

#ifndef MTX_P_KATE_H
#define MTX_P_KATE_H

#include "common/common_pch.h"

#include "merge/pr_generic.h"
#include "common/kate.h"

class kate_packetizer_c: public generic_packetizer_c {
private:
  std::vector<memory_cptr> m_headers;

  kate_identification_header_t m_kate_id;

  int64_t m_previous_timecode;

public:
  kate_packetizer_c(generic_reader_c *reader, track_info_c &ti);
  virtual ~kate_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual translatable_string_c get_format_name() const {
    return YT("Kate");
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message);
};

#endif  // MTX_P_KATE_H
