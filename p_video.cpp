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
    \version \$Id: p_video.cpp,v 1.17 2003/04/13 15:23:03 mosu Exp $
    \brief video output module
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "queue.h"
#include "p_video.h"

#include "KaxTracks.h"
#include "KaxTrackVideo.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

video_packetizer_c::video_packetizer_c(double nfps, int nwidth,
                                       int nheight, int nbpp,
                                       int navi_compat_mode, track_info_t *nti)
  throw (error_c) : q_c(nti) {
  packetno = 0;
  fps = nfps;
  width = nwidth;
  height = nheight;
  bpp = nbpp;
  avi_compat_mode = navi_compat_mode;
  frames_output = 0;
  avi_compat_mode = 1;
  last_id = 1;
  set_header();
}

void video_packetizer_c::set_header() {
  using namespace LIBMATROSKA_NAMESPACE;
  int set_codec_private = 0;
  char *codecstr;
  
  if (kax_last_entry == NULL)
    track_entry =
      &GetChild<KaxTrackEntry>(static_cast<KaxTracks &>(*kax_tracks));
  else
    track_entry =
      &GetNextChild<KaxTrackEntry>(static_cast<KaxTracks &>(*kax_tracks),
        static_cast<KaxTrackEntry &>(*kax_last_entry));
  kax_last_entry = track_entry;
  
  if (serialno == -1)
    serialno = track_number++;
  KaxTrackNumber &tnumber =
    GetChild<KaxTrackNumber>(static_cast<KaxTrackEntry &>(*track_entry));
  *(static_cast<EbmlUInteger *>(&tnumber)) = serialno;
  
  *(static_cast<EbmlUInteger *>
    (&GetChild<KaxTrackType>(static_cast<KaxTrackEntry &>(*track_entry)))) =
      track_video;

  KaxCodecID &codec_id =
    GetChild<KaxCodecID>(static_cast<KaxTrackEntry &>(*track_entry));
  if (!avi_compat_mode) {
    codecstr = map_video_codec_fourcc(ti->fourcc, &set_codec_private);
    codec_id.CopyBuffer((binary *)codecstr, countof(codecstr));
  } else
    codec_id.CopyBuffer((binary *)"V_MS/VFW/FOURCC",
                        countof("V_MS/VFW/FOURCC"));
  if (set_codec_private || avi_compat_mode) {
    KaxCodecPrivate &codec_private = 
      GetChild<KaxCodecPrivate>(static_cast<KaxTrackEntry &>(*track_entry));
    codec_private.CopyBuffer((binary *)ti->private_data, ti->private_size);
  }

  KaxTrackVideo &video =
    GetChild<KaxTrackVideo>(static_cast<KaxTrackEntry &>(*track_entry));

  KaxVideoPixelHeight &pheight = GetChild<KaxVideoPixelHeight>(video);
  *(static_cast<EbmlUInteger *>(&pheight)) = height;

  KaxVideoPixelWidth &pwidth = GetChild<KaxVideoPixelWidth>(video);
  *(static_cast<EbmlUInteger *>(&pwidth)) = width;

  KaxVideoFrameRate &frate = GetChild<KaxVideoFrameRate>(video);
  *(static_cast<EbmlFloat *>(&frate)) = fps;
}

int video_packetizer_c::process(unsigned char *buf, int size,
                                int64_t old_timecode, int64_t flags) {
  int64_t timecode;
  int key, num_frames;

  num_frames = flags & VNUMFRAMES;
  if (old_timecode == -1)
    timecode = (int64_t)(1000.0 * frames_output / fps);
  else
    timecode = old_timecode;

  if ((flags & VFT_IFRAME) != 0)
    // Add a key frame and save its ID so that we can reference it later.
    last_id = add_packet(buf, size, timecode);
  else
    // This is a P frame - let's reference the last frame.
    last_id = add_packet(buf, size, timecode, last_id);

  if (num_frames > 1)
    fprintf(stdout, "Warning: video_packetizer: num_frames > 1\n");
  frames_output += num_frames;

  packetno++;
    
  return EMOREDATA;
}

video_packetizer_c::~video_packetizer_c() {
}
