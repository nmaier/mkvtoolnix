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
    \version \$Id: p_video.h,v 1.16 2003/04/11 11:27:32 mosu Exp $
    \brief class definition for the video output module
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/
 
#ifndef __P_VIDEO_H
#define __P_VIDEO_H

#include "common.h"
#include "queue.h"

#define VFT_IFRAME 0x10000000L
#define VFT_PFRAME 0x20000000L
#define VFT_BFRAME 0x40000000L
#define VNUMFRAMES 0x0000FFFFL

class video_packetizer_c: public q_c {
private:
  double         fps;
  int            width, height, bpp, packetno;
  int            frames_output, avi_compat_mode;
  u_int64_t      last_id;

public:
  video_packetizer_c(double nfps, int nwidth, int nheight, int nbpp,
                     int navi_compat_mode, track_info_t *nti) throw (error_c);
  virtual ~video_packetizer_c();
    
  virtual int  process(unsigned char *buf, int size, int64_t old_timecode = -1,
                       int64_t flags = VFT_IFRAME);
  virtual void set_header();
};

#endif // __P_VIDEO_H
