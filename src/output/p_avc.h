/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the MPEG 4 part 10 ES video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_AVC_H
#define __P_AVC_H

#include "common/common_pch.h"

#include "common/mpeg4_p10.h"
#include "merge/pr_generic.h"

using namespace mpeg4::p10;

class mpeg4_p10_es_video_packetizer_c: public generic_packetizer_c {
protected:
  avc_es_parser_c m_parser;
  memory_cptr m_avcc;
  int m_width, m_height;
  bool m_allow_timecode_generation, m_first_frame;

public:
  mpeg4_p10_es_video_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, memory_cptr avcc, int width, int height);

  virtual int process(packet_cptr packet);
  virtual void add_extra_data(memory_cptr data);
  virtual void set_headers();

  virtual void flush();
  virtual void flush_frames();

  virtual void enable_timecode_generation(bool enable, int64_t default_duration = -1);
  virtual void extract_aspect_ratio();
  virtual void set_track_default_duration(int64_t default_duration);

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("AVC/h.264") : "AVC/h.264";
  };

  virtual void connect(generic_packetizer_c *src, int64_t p_append_timecode_offset = -1);
  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message);
};

#endif // __P_AVC_H
