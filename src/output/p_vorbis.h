/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the Vorbis packetizer

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "config.h"

#ifndef __P_VORBIS_H
#define __P_VORBIS_H

#include "os.h"

#include <ogg/ogg.h>
#include <vorbis/codec.h>

#include "common.h"
#include "pr_generic.h"
#include "pr_generic.h"

class vorbis_packetizer_c: public generic_packetizer_c {
private:
  int64_t m_previous_bs, m_samples, m_previous_samples_sum, m_previous_timecode, m_timecode_offset;
  vector<memory_cptr> m_headers;
  vorbis_info m_vi;
  vorbis_comment m_vc;

public:
  vorbis_packetizer_c(generic_reader_c *p_reader,  track_info_c &p_ti,
                      unsigned char *d_header,     int l_header,
                      unsigned char *d_comments,   int l_comments,
                      unsigned char *d_codecsetup, int l_codecsetup) throw (error_c);
  virtual ~vorbis_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual const char *get_format_name() {
    return "Vorbis";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src, string &error_message);
};

#endif  // __P_VORBIS_H
