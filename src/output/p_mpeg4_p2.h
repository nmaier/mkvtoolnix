/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the MPEG4 part 2 video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_MPEG4_P2_H
#define __P_MPEG4_P2_H

#include "common/common_pch.h"

#include <deque>

#include "common/mpeg4_p2.h"
#include "output/p_video.h"

class mpeg4_p2_video_packetizer_c: public video_packetizer_c {
protected:
  struct timecode_duration_t {
    int64_t m_timecode, m_duration;
    timecode_duration_t(int64_t timecode, int64_t duration)
      : m_timecode(timecode)
      , m_duration(duration)
    {
    }
  };

  struct statistics_t {
    int m_num_dropped_timecodes, m_num_generated_timecodes;
    int m_num_i_frames, m_num_p_frames, m_num_b_frames, m_num_n_vops;

    statistics_t()
      : m_num_dropped_timecodes(0)
      , m_num_generated_timecodes(0)
      , m_num_i_frames(0)
      , m_num_p_frames(0)
      , m_num_b_frames(0)
      , m_num_n_vops(0)
    {
    }
  };

  std::deque<video_frame_t> m_ref_frames, m_b_frames;
  std::deque<timecode_duration_t> m_available_timecodes;
  int64_t m_timecodes_generated, m_previous_timecode;
  bool m_aspect_ratio_extracted, m_input_is_native, m_output_is_native;
  bool m_size_extracted;
  mpeg4::p2::config_data_t m_config_data;
  statistics_t m_statistics;

public:
  mpeg4_p2_video_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, double fps, int width, int height, bool input_is_native);
  virtual ~mpeg4_p2_video_packetizer_c();

  virtual int process(packet_cptr packet);

  virtual const std::string get_format_name(bool translate = true) const {
    return translate ? Y("MPEG-4") : "MPEG-4";
  }

protected:
  virtual int process_native(packet_cptr packet);
  virtual int process_non_native(packet_cptr packet);
  virtual void flush_impl();
  virtual void flush_frames(bool end_of_file);
  virtual void extract_aspect_ratio(const unsigned char *buffer, int size);
  virtual void extract_size(const unsigned char *buffer, int size);
  virtual void extract_config_data(packet_cptr &packet);
  virtual void fix_codec_string();
  virtual void generate_timecode_and_duration();
  virtual void get_next_timecode_and_duration(int64_t &timecode, int64_t &duration);
};

#endif  // __P_MPEG4_P2_H
