/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the DTS output module

   Written by Peter Niemayer <niemayer@isg.de>.
   Modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_DTS_H
#define __P_DTS_H

#include "common/common_pch.h"

#include "merge/pr_generic.h"
#include "common/dts.h"

class dts_packetizer_c: public generic_packetizer_c {
private:
  int64_t m_samples_written, m_bytes_written;

  unsigned char *m_packet_buffer;
  int m_buffer_size;

  bool m_get_first_header_later;
  dts_header_t m_first_header, m_previous_header;
  bool m_skipping_is_normal;

public:

  dts_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, const dts_header_t &dts_header, bool get_first_header_later = false) throw (error_c);
  virtual ~dts_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void set_headers();
  virtual void set_skipping_is_normal(bool skipping_is_normal) {
    m_skipping_is_normal = skipping_is_normal;
  }

  virtual const char *get_format_name() {
    return "DTS";
  }

  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message);

private:
  virtual void add_to_buffer(unsigned char *buf, int size);
  virtual unsigned char *get_dts_packet(dts_header_t &dts_header);
  virtual bool dts_packet_available();
  virtual void remove_dts_packet(int pos, int framesize);
};

#endif // __P_DTS_H
