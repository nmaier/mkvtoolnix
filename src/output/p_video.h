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

#define VFT_IFRAME -1
#define VFT_PFRAMEAUTOMATIC -2
#define VFT_NOBFRAME -1

class video_packetizer_c: public generic_packetizer_c {
protected:
  double fps;
  int width, height, frames_output;
  int64_t ref_timecode, duration_shift;
  bool pass_through;

public:
  video_packetizer_c(generic_reader_c *_reader, const char *_codec_id,
                     double _fps, int _width, int _height,
                     track_info_c *_ti);

  virtual int process(memory_c &mem, int64_t old_timecode = -1,
                      int64_t duration = -1, int64_t bref = VFT_IFRAME,
                      int64_t fref = VFT_NOBFRAME);
  virtual void set_headers();

  virtual void dump_debug_info();

  virtual const char *get_format_name() {
    return "video";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src);

protected:
  virtual void check_fourcc();
};

class mpeg1_2_video_packetizer_c: public video_packetizer_c {
protected:
  M2VParser parser;
  bool framed, aspect_ratio_extracted;

public:
  mpeg1_2_video_packetizer_c(generic_reader_c *_reader, int _version,
                             double _fps, int _width, int _height,
                             int _dwidth, int _dheight, bool _framed,
                             track_info_c *_ti);

  virtual int process(memory_c &mem, int64_t old_timecode = -1,
                      int64_t duration = -1, int64_t bref = VFT_IFRAME,
                      int64_t fref = VFT_NOBFRAME);
  virtual void flush();

protected:
  virtual void extract_fps(const unsigned char *buffer, int size);
  virtual void extract_aspect_ratio(const unsigned char *buffer, int size);
  virtual void create_private_data();
};

class mpeg4_p2_video_packetizer_c: public video_packetizer_c {
protected:
  deque<video_frame_t> available_frames, queued_frames;
  deque<int64_t> available_timecodes, available_durations;
  int64_t timecodes_generated;
  video_frame_t bref_frame, fref_frame;
  bool aspect_ratio_extracted, input_is_native, output_is_native;
  int64_t csum;

public:
  mpeg4_p2_video_packetizer_c(generic_reader_c *_reader,
                              double _fps, int _width, int _height,
                              bool _input_is_native, track_info_c *_ti);
  virtual ~mpeg4_p2_video_packetizer_c();

  virtual int process(memory_c &mem, int64_t old_timecode = -1,
                      int64_t duration = -1, int64_t bref = VFT_IFRAME,
                      int64_t fref = VFT_NOBFRAME);
  virtual void flush();

protected:
  virtual int process_native(memory_c &mem, int64_t old_timecode,
                             int64_t old_duration, int64_t bref,
                             int64_t fref);
  virtual int process_non_native(memory_c &mem, int64_t old_timecode,
                                 int64_t old_duration, int64_t bref,
                                 int64_t fref);
  virtual void flush_frames(char next_frame = '?', bool flush_all = false);
  virtual void extract_aspect_ratio(const unsigned char *buffer, int size);
};

class mpeg4_p10_video_packetizer_c: public video_packetizer_c {
public:
  mpeg4_p10_video_packetizer_c(generic_reader_c *_reader,
                               double _fps, int _width, int _height,
                               track_info_c *_ti);

protected:
  virtual void extract_aspect_ratio();
};

#endif // __P_VIDEO_H
