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
    \version \$Id: p_video.cpp,v 1.33 2003/05/11 09:05:55 mosu Exp $
    \brief video output module
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "pr_generic.h"
#include "p_video.h"
#include "matroska.h"

using namespace LIBMATROSKA_NAMESPACE;

video_packetizer_c::video_packetizer_c(generic_reader_c *nreader,
                                       double nfps, int nwidth,
                                       int nheight, int nbpp,
                                       int navi_compat_mode, track_info_t *nti)
  throw (error_c) : generic_packetizer_c(nreader, nti) {
  fps = nfps;
  width = nwidth;
  height = nheight;
  bpp = nbpp;
  avi_compat_mode = navi_compat_mode;
  frames_output = 0;
  avi_compat_mode = 1;
  ref_timecode = -1;
  if (get_cue_creation() == CUES_UNSPECIFIED)
    set_cue_creation(CUES_IFRAMES);

  set_track_type(track_video);
}

void video_packetizer_c::set_headers() {
  set_codec_id(MKV_V_MSCOMP);
  set_codec_private(ti->private_data, ti->private_size);

  // Set MinCache and MaxCache to 1 for I- and P-frames. If you only
  // have I-frames then both can be set to 0 (e.g. MJPEG). 2 is needed
  // if there are B-frames as well.
  set_track_min_cache(1);
  set_track_max_cache(1);

  set_video_pixel_width(width);
  set_video_pixel_height(height);
  set_video_frame_rate(fps);

  generic_packetizer_c::set_headers();
}

int video_packetizer_c::process(unsigned char *buf, int size,
                                int64_t old_timecode, int64_t, int64_t bref,
                                int64_t) {
  int64_t timecode;

  if (old_timecode == -1)
    timecode = (int64_t)(1000.0 * frames_output / fps);
  else
    timecode = old_timecode;

  if (bref == -1) {
    // Add a key frame and save its timecode so that we can reference it later.
    add_packet(buf, size, timecode, (int64_t)(1000.0 / fps));
    ref_timecode = timecode;
  } else {
    // This is a P frame - let's reference the last frame.
    add_packet(buf, size, timecode, (int64_t)(1000.0 / fps), 0, ref_timecode);
    ref_timecode = timecode;
  }

  frames_output++;
    
  return EMOREDATA;
}

video_packetizer_c::~video_packetizer_c() {
}
