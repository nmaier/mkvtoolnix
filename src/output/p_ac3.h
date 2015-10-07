/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the AC3 output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_P_AC3_H
#define MTX_P_AC3_H

#include "common/common_pch.h"

#include "common/ac3.h"
#include "common/samples_timecode_conv.h"
#include "merge/pr_generic.h"

class ac3_packetizer_c: public generic_packetizer_c {
protected:
  int64_t m_bytes_output, m_packetno, m_last_timecode, m_num_packets_same_tc;
  ac3::frame_c m_first_ac3_header;
  ac3::parser_c m_parser;
  samples_to_timecode_converter_c m_s2tc;
  int64_t m_single_packet_duration;
  std::deque<std::pair<int64_t, uint64_t> > m_available_timecodes;

public:
  ac3_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, int samples_per_sec, int channels, int bsid);
  virtual ~ac3_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void flush_packets();
  virtual void set_headers();

  virtual translatable_string_c get_format_name() const {
    return YT("AC3");
  }

  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message);

protected:
  virtual void add_to_buffer(unsigned char *const buf, int size);
  virtual void adjust_header_values(ac3::frame_c &ac3_header);
  virtual ac3::frame_c get_frame();
  virtual int64_t calculate_timecode(uint64_t stream_position);
  virtual void flush_impl();
};

class ac3_bs_packetizer_c: public ac3_packetizer_c {
protected:
  unsigned char m_bsb;
  bool m_bsb_present;

public:
  ac3_bs_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, unsigned long samples_per_sec, int channels, int bsid);

protected:
  virtual void add_to_buffer(unsigned char *const buf, int size);
};

#endif // MTX_P_AC3_H
