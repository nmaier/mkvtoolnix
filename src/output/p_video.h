/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * class definition for the video output module
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#ifndef __P_VIDEO_H
#define __P_VIDEO_H

#include "os.h"

#include "common.h"
#include "mpeg4_common.h"
#include "pr_generic.h"

#define VFT_IFRAME -1
#define VFT_PFRAMEAUTOMATIC -2
#define VFT_NOBFRAME -1

class video_packetizer_c: public generic_packetizer_c {
private:
  double fps;
  int width, height, bpp, frames_output;
  int64_t ref_timecode, duration_shift;
  bool avi_compat_mode, bframes, pass_through, is_mpeg4, is_mpeg1_2;
  bool aspect_ratio_extracted;
  vector<video_frame_t> queued_frames;
  video_frame_t bref_frame, fref_frame;

public:
  video_packetizer_c(generic_reader_c *nreader, const char *ncodec_id,
                     double nfps, int nwidth, int nheight, bool nbframes,
                     track_info_c *nti)
    throw (error_c);
  virtual ~video_packetizer_c();

  virtual int process(memory_c &mem, int64_t old_timecode = -1,
                      int64_t duration = -1, int64_t bref = VFT_IFRAME,
                      int64_t fref = VFT_NOBFRAME);
  virtual void set_headers();
  virtual void flush();

  virtual void dump_debug_info();

  virtual const char *get_format_name() {
    return "video";
  }
  virtual connection_result_e can_connect_to(generic_packetizer_c *src);

protected:
  virtual void flush_frames(char next_frame = '?', bool flush_all = false);
  virtual void extract_mpeg4_aspect_ratio(const unsigned char *buffer,
                                          int size);
  virtual void extract_mpeg1_2_fps(const unsigned char *buffer, int size);
};

#endif // __P_VIDEO_H
