/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __P_VIDEO_H
#define __P_VIDEO_H

#include "os.h"

#include "common.h"
#include "mpeg4_common.h"
#include "pr_generic.h"
#include "M2VParser.h"

#define VFT_IFRAME          -1
#define VFT_PFRAMEAUTOMATIC -2
#define VFT_NOBFRAME        -1

class video_packetizer_c: public generic_packetizer_c {
protected:
  double m_fps;
  int m_width, m_height, m_frames_output;
  int64_t m_ref_timecode, m_duration_shift;
  bool m_pass_through;

public:
  video_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, const char *codec_id, double fps, int width, int height);

  virtual int process(packet_cptr packet);
  virtual void set_headers();

  virtual const char *get_format_name() {
    return "video";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src, string &error_message);

protected:
  virtual void check_fourcc();
};

class mpeg1_2_video_packetizer_c: public video_packetizer_c {
protected:
  M2VParser m_parser;
  memory_cptr m_seq_hdr;
  bool m_framed, m_aspect_ratio_extracted;

public:
  mpeg1_2_video_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, int version, double fps, int width, int height, int dwidth, int dheight, bool framed);

  virtual int process(packet_cptr packet);
  virtual void flush();

protected:
  virtual void extract_fps(const unsigned char *buffer, int size);
  virtual void extract_aspect_ratio(const unsigned char *buffer, int size);
  virtual void create_private_data();
  virtual int process_framed(packet_cptr packet);
};

class mpeg4_p2_video_packetizer_c: public video_packetizer_c {
protected:
  deque<video_frame_t> m_queued_frames;
  deque<int64_t> m_available_timecodes, m_available_durations;
  int64_t m_timecodes_generated, m_last_i_p_frame, m_previous_timecode;
  bool m_aspect_ratio_extracted, m_input_is_native, m_output_is_native;
  bool m_size_extracted;

public:
  mpeg4_p2_video_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, double fps, int width, int height, bool input_is_native);

  virtual int process(packet_cptr packet);
  virtual void flush();

protected:
  virtual int process_native(packet_cptr packet);
  virtual int process_non_native(packet_cptr packet);
  virtual void flush_frames_maybe(frame_type_e next_frame);
  virtual void flush_frames(bool end_of_file);
  virtual void extract_aspect_ratio(const unsigned char *buffer, int size);
  virtual void extract_size(const unsigned char *buffer, int size);
  virtual void fix_codec_string();
  virtual void handle_missing_timecodes(bool end_of_file);
};

class mpeg4_p10_video_packetizer_c: public video_packetizer_c {
protected:
  int m_nalu_size_len_src, m_nalu_size_len_dst;
  int64_t m_max_nalu_size;

public:
  mpeg4_p10_video_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, double fps, int width, int height);
  virtual int process(packet_cptr packet);

  virtual connection_result_e can_connect_to(generic_packetizer_c *src, string &error_message);

protected:
  virtual void extract_aspect_ratio();
  virtual void setup_nalu_size_len_change();
  virtual void change_nalu_size_len(packet_cptr packet);
};

class theora_video_packetizer_c: public video_packetizer_c {
public:
  theora_video_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti, double fps, int width, int height);
  virtual void set_headers();
  virtual int process(packet_cptr packet);

protected:
  virtual void extract_aspect_ratio();
};

#endif // __P_VIDEO_H
