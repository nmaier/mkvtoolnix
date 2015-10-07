/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the ALAC output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_OUTPUT_P_ALAC_H
#define MTX_OUTPUT_P_ALAC_H

#include "common/common_pch.h"

#include "common/alac.h"
#include "merge/pr_generic.h"

class alac_packetizer_c: public generic_packetizer_c {
private:
  memory_cptr m_magic_cookie;
  unsigned int m_sample_rate, m_channels;

public:
  alac_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, memory_cptr const &magic_cookie, unsigned int sample_rate, unsigned int channels);
  virtual ~alac_packetizer_c();

  virtual int process(packet_cptr packet);

  virtual translatable_string_c get_format_name() const {
    return YT("ALAC");
  }

  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message);
};

#endif // MTX_OUTPUT_P_ALAC_H
