/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the TTA output module

   Written by Steve Lhomme <steve.lhomme@free.fr>.
*/

#ifndef __P_WAVPACK_H
#define __P_WAVPACK_H

#include "common/common_pch.h"

#include "merge/pr_generic.h"
#include "common/wavpack.h"

class wavpack_packetizer_c: public generic_packetizer_c {
private:
  int m_channels, m_sample_rate, m_bits_per_sample;
  int64_t m_samples_per_block;
  int64_t m_samples_output;
  bool m_has_correction;

public:
  wavpack_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, wavpack_meta_t &meta);

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual const std::string get_format_name(bool translate = true) const {
    return translate ? Y("WAVPACK4") : "WAVPACK4";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message);
};

#endif // __P_WAVPACK_H
