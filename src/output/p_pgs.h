/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the PGS output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_PGS_H
#define __P_PGS_H

#include "common/common_pch.h"

#include "common/compression.h"
#include "merge/pr_generic.h"

class pgs_packetizer_c: public generic_packetizer_c {
protected:
  bool m_aggregate_packets;
  packet_cptr m_aggregated;

public:
  pgs_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti);
  virtual ~pgs_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void set_headers();
  virtual void set_aggregate_packets(bool aggregate_packets) {
    m_aggregate_packets = aggregate_packets;
  }

  virtual const char *get_format_name() {
    return "PGS";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message);
};

#endif // __P_PGS_H
