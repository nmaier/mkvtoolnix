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

#include "common/os.h"

#include <deque>

#include "common/common.h"
#include "common/mpeg4_p2.h"
#include "output/p_video.h"

class mpeg4_p2_video_packetizer_c: public video_packetizer_c {
protected:
  std::deque<video_frame_t> m_queued_frames;
  std::deque<int64_t> m_available_timecodes, m_available_durations;
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

#endif  // __P_MPEG4_P2_H
