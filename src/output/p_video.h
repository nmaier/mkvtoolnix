/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  p_video.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief class definition for the video output module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __P_VIDEO_H
#define __P_VIDEO_H

#include "os.h"

#include "common.h"
#include "pr_generic.h"

#define VFT_IFRAME -1
#define VFT_PFRAMEAUTOMATIC -2
#define VFT_NOBFRAME -1

typedef struct {
  char type;
  int pos;
} frame_t;

class video_packetizer_c: public generic_packetizer_c {
private:
  double fps;
  int width, height, bpp, frames_output;
  int64_t ref_timecode, duration_shift;
  bool avi_compat_mode, bframes, pass_through, is_mpeg4;
  char *codec_id;

public:
  video_packetizer_c(generic_reader_c *nreader, const char *ncodec_id,
                     double nfps, int nwidth, int nheight, bool nbframes,
                     track_info_c *nti)
    throw (error_c);
  virtual ~video_packetizer_c();

  virtual int process(unsigned char *buf, int size, int64_t old_timecode = -1,
                      int64_t duration = -1, int64_t bref = VFT_IFRAME,
                      int64_t fref = VFT_NOBFRAME);
  virtual void set_headers();

  virtual void dump_debug_info();

protected:
  void find_mpeg4_frame_types(unsigned char *buf, int size,
                              vector<frame_t> &frames);
};

#endif // __P_VIDEO_H
