/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the VobBtn output module

   Written by Steve Lhomme <steve.lhomme@free.fr>.
   Modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_P_VOBBTN_H
#define MTX_P_VOBBTN_H

#include "common/common_pch.h"

#include "common/compression.h"
#include "merge/pr_generic.h"

class vobbtn_packetizer_c: public generic_packetizer_c {
protected:
  int64_t m_previous_timecode;
  int m_width, m_height;

public:
  vobbtn_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, int width, int height);
  virtual ~vobbtn_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual const std::string get_format_name(bool translate = true) const {
    return translate ? Y("VobBtn") : "VobBtn";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message);
};

#endif // MTX_P_VOBBTN_H
