/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the AC3 output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_AC3_H
#define __P_AC3_H

#include "os.h"

#include "byte_buffer.h"
#include "common.h"
#include "pr_generic.h"
#include "ac3_common.h"

class ac3_packetizer_c: public generic_packetizer_c {
protected:
  int64_t m_bytes_output, m_packetno, m_bytes_skipped;
  int m_samples_per_sec;
  byte_buffer_c m_byte_buffer;
  bool m_first_packet;
  ac3_header_t m_first_ac3_header;

public:
  ac3_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, int samples_per_sec, int channels, int bsid) throw (error_c);
  virtual ~ac3_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual const char *get_format_name() {
    return "AC3";
  }

  virtual connection_result_e can_connect_to(generic_packetizer_c *src, string &error_message);

protected:
  virtual unsigned char *get_ac3_packet(unsigned long *header, ac3_header_t *ac3header);
  virtual void add_to_buffer(unsigned char *buf, int size);
  virtual void adjust_header_values(ac3_header_t &ac3_header);
};

class ac3_bs_packetizer_c: public ac3_packetizer_c {
protected:
  unsigned char m_bsb;
  bool m_bsb_present;

public:
  ac3_bs_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, unsigned long samples_per_sec, int channels, int bsid) throw (error_c);

protected:
  virtual void add_to_buffer(unsigned char *buf, int size);
};

#endif // __P_AC3_H
