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
    \version \$Id: p_video.h,v 1.13 2003/03/04 10:16:28 mosu Exp $
    \brief class definition for the video output module
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/
 
#ifndef __P_VIDEO_H
#define __P_VIDEO_H

#include "common.h"
#include "queue.h"

class video_packetizer_c: public q_c {
private:
  char           codec[5];
  double         fps;
  int            width, height, bpp, max_frame_size, packetno;
  int            frames_output;
  unsigned char *tempbuf;
  int            avi_compat_mode;
  u_int64_t      last_id;

public:
  video_packetizer_c(unsigned char *pr_data, int pd_size, char *ncodec,
                     double nfps, int nwidth, int nheight, int nbpp,
                     int nmax_frame_size, audio_sync_t *nasync,
                     int navi_compat_mode) throw (error_c);
  virtual ~video_packetizer_c();
    
  virtual int  process(unsigned char *buf, int size, int num_frames, int key,
                       int last_frame, int64_t old_timecode = -1);
  virtual void set_header();
};

#endif // __P_VIDEO_H
