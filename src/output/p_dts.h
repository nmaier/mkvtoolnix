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

#ifndef MTX_P_DTS_H
#define MTX_P_DTS_H

#include "common/common_pch.h"

#include "common/byte_buffer.h"
#include "common/dts.h"
#include "merge/pr_generic.h"

class dts_packetizer_c: public generic_packetizer_c {
private:
  int64_t m_samples_written, m_bytes_written;

  byte_buffer_c m_packet_buffer;

  dts_header_t m_first_header, m_previous_header;
  bool m_skipping_is_normal;

  std::deque<int64_t> m_available_timecodes;

public:
  dts_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, const dts_header_t &dts_header);
  virtual ~dts_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void set_headers();
  virtual void set_skipping_is_normal(bool skipping_is_normal) {
    m_skipping_is_normal = skipping_is_normal;
  }
  virtual translatable_string_c get_format_name() const {
    return YT("DTS");
  }

  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message);

protected:
  virtual void flush_impl();

private:
  virtual unsigned char *get_dts_packet(dts_header_t &dts_header, bool flushing);
  virtual void process_available_packets(bool flushing);
};

#endif // MTX_P_DTS_H
