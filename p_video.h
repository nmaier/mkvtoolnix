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
    \version \$Id: p_video.h,v 1.1 2003/02/16 17:04:39 mosu Exp $
    \brief class definition for the video output module
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/
 
#ifndef __P_VIDEO_H
#define __P_VIDEO_H

#include "common.h"
#include "queue.h"

class video_packetizer_c: public q_c {
  private:
    char    codec[5];
    double  fps;
    int     width, height;
    int     bpp;
    int     max_frame_size;
    int     packetno, frames_output;
    char   *tempbuf;
    int     avi_compat_mode;
    range_t range;
  public:
    video_packetizer_c(char *, double, int, int, int, int, audio_sync_t *,
                       range_t *nrange, int) throw (error_c);
    virtual ~video_packetizer_c();
    
    virtual int  process(char *buf, int size, int num_frames, int key,
                         int last_frame);
    virtual void set_header();
};

#endif
