/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the Dirac video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_DIRAC_H
#define __P_DIRAC_H

#include "common/common_pch.h"

#include "merge/pr_generic.h"
#include "common/dirac.h"

class dirac_video_packetizer_c: public generic_packetizer_c {
protected:
  dirac::es_parser_c m_parser;

  dirac::sequence_header_t m_seqhdr;
  bool m_headers_found;

  int64_t m_previous_timecode;

public:
  dirac_video_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti);

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual void flush();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("Dirac") : "Dirac";
  };

  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message);

protected:
  virtual void flush_frames();
  virtual void headers_found();
};

#endif // __P_DIRAC_H
