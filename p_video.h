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
    \version \$Id: p_video.h,v 1.5 2003/02/27 09:52:37 mosu Exp $
    \brief class definition for the video output module
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/
 
#ifndef __P_VIDEO_H
#define __P_VIDEO_H

#include "common.h"
#include "queue.h"

class video_packetizer_c: public q_c {
private:
  char              codec[5];
  double            fps;
  int               width, height, bpp, max_frame_size, packetno;
  int               frames_output;
  char             *tempbuf;
  int               avi_compat_mode;
  range_t           range;
  u_int64_t         last_id;
  packet_t         *last_keyframe;
  cluster_helper_c *last_helper;

public:
  video_packetizer_c(void *, int, char *, double, int, int, int, int,
                     audio_sync_t *, range_t *nrange, int) throw (error_c);
  virtual ~video_packetizer_c();
    
  virtual int  process(char *buf, int size, int num_frames, int key,
                       int last_frame);
  virtual void set_header();
  virtual void added_packet_to_cluster(packet_t *packet,
                                       cluster_helper_c *helper);
};

#endif // __P_VIDEO_H
