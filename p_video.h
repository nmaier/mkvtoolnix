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
    \version \$Id: p_video.h,v 1.27 2003/05/25 15:35:39 mosu Exp $
    \brief class definition for the video output module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __P_VIDEO_H
#define __P_VIDEO_H

#include "common.h"
#include "pr_generic.h"

#define VFT_IFRAME 0x10000000L
#define VFT_PFRAME 0x20000000L
#define VFT_BFRAME 0x40000000L
#define VNUMFRAMES 0x0000FFFFL

class video_packetizer_c: public generic_packetizer_c {
private:
  double fps;
  int width, height, bpp, frames_output, avi_compat_mode;
  int64_t ref_timecode;

public:
  video_packetizer_c(generic_reader_c *nreader, double nfps, int nwidth,
                     int nheight, int nbpp, int navi_compat_mode,
                     track_info_t *nti) throw (error_c);
  virtual ~video_packetizer_c();

  virtual int process(unsigned char *buf, int size, int64_t old_timecode = -1,
                      int64_t flags = VFT_IFRAME, int64_t bref = -1,
                      int64_t fref = -1);
  virtual void set_headers();

  virtual void dump_debug_info();
};

#endif // __P_VIDEO_H
