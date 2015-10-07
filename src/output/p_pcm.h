/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the PCM output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_P_PCM_H
#define MTX_P_PCM_H

#include "common/common_pch.h"

#include "common/byte_buffer.h"
#include "common/samples_timecode_conv.h"
#include "merge/pr_generic.h"

class pcm_packetizer_c: public generic_packetizer_c {
public:
  enum pcm_format_e {
    little_endian_integer = 0,
    big_endian_integer    = 1,
    ieee_float            = 2
  };

private:
  int m_samples_per_sec, m_channels, m_bits_per_sample, m_samples_per_packet;
  size_t m_packet_size, m_min_packet_size, m_samples_output;
  pcm_format_e m_format;
  byte_buffer_c m_buffer;
  samples_to_timecode_converter_c m_s2tc;

public:
  pcm_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, int p_samples_per_sec, int channels, int bits_per_sample, pcm_format_e format = little_endian_integer);
  virtual ~pcm_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual translatable_string_c get_format_name() const {
    return YT("PCM");
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message);

protected:
  virtual int process_packaged(packet_cptr packet);
  virtual void flush_impl();
};

#endif // MTX_P_PCM_H
