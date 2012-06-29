/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the TrueHD output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_TRUEHD_H
#define __P_TRUEHD_H

#include "common/common_pch.h"

#include "merge/pr_generic.h"
#include "common/samples_timecode_conv.h"
#include "common/truehd.h"

class truehd_packetizer_c: public generic_packetizer_c {
protected:
  bool m_first_frame;
  truehd_frame_t m_first_truehd_header;

  int64_t m_current_samples_per_frame, m_samples_output, m_ref_timecode;
  samples_to_timecode_converter_c m_s2tc;
  truehd_parser_c m_parser;
  std::vector<truehd_frame_cptr> m_frames;

public:
  truehd_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, truehd_frame_t::codec_e codec, int sampling_rate, int channels);
  virtual ~truehd_packetizer_c();

  virtual int process(packet_cptr packet);
  virtual void handle_frames();
  virtual void set_headers();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("TrueHD") : "TrueHD";
  }

  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message);

protected:
  virtual void adjust_header_values(truehd_frame_cptr &frame);

  virtual void flush_impl();
  virtual void flush_frames();
  virtual void flush_frames_merged();
  virtual void flush_frames_separate();
};

#endif // __P_TRUEHD_H
