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
    \brief video output module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "mkvmerge.h"
#include "pr_generic.h"
#include "p_video.h"
#include "matroska.h"

extern "C" {
#include <avilib.h>
}

using namespace libmatroska;

video_packetizer_c::video_packetizer_c(generic_reader_c *nreader,
                                       const char *ncodec_id,
                                       double nfps, int nwidth,
                                       int nheight, bool nbframes,
                                       track_info_t *nti)
  throw (error_c) : generic_packetizer_c(nreader, nti) {
  fps = nfps;
  width = nwidth;
  height = nheight;
  frames_output = 0;
  bframes = nbframes;
  ref_timecode = -1;
  if (get_cue_creation() == CUES_UNSPECIFIED)
    set_cue_creation(CUES_IFRAMES);
  video_track_present = true;
  codec_id = safestrdup(ncodec_id);
  duration_shift = 0;

  set_track_type(track_video);
}

void video_packetizer_c::set_headers() {
  if (codec_id != NULL)
    set_codec_id(codec_id);
  else
    set_codec_id(MKV_V_MSCOMP);
  if (!strcmp(hcodec_id, MKV_V_MSCOMP) &&
      (ti->private_data != NULL) &&
      (ti->private_size >= sizeof(alBITMAPINFOHEADER)) &&
      (ti->fourcc[0] != 0))
    memcpy(&((alBITMAPINFOHEADER *)ti->private_data)->bi_compression,
           ti->fourcc, 4);
  set_codec_private(ti->private_data, ti->private_size);

  // Set MinCache and MaxCache to 1 for I- and P-frames. If you only
  // have I-frames then both can be set to 0 (e.g. MJPEG). 2 is needed
  // if there are B-frames as well.
  if (bframes) {
    set_track_min_cache(2);
    set_track_max_cache(2);
  } else {
    set_track_min_cache(1);
    set_track_max_cache(1);
  }
  if (fps != 0.0)
    set_track_default_duration_ns((int64_t)(1000000000.0 / fps));

  set_video_pixel_width(width);
  set_video_pixel_height(height);

  generic_packetizer_c::set_headers();

  track_entry->EnableLacing(false);
}

// Semantics:
// bref == -1: I frame, no references
// bref == -2: P or B frame, use timecode of last I / P frame as the bref
// bref > 0: P or B frame, use this bref as the backward reference
//           (absolute reference, not relative!)
// fref == 0: P frame, no forward reference
// fref > 0: B frame with given forward reference (absolute reference,
//           not relative!)
int video_packetizer_c::process(unsigned char *buf, int size,
                                int64_t old_timecode, int64_t duration,
                                int64_t bref, int64_t fref) {
  int64_t timecode;

  debug_enter("video_packetizer_c::process");

  if (old_timecode == -1)
    timecode = (int64_t)(1000.0 * frames_output / fps) + duration_shift;
  else
    timecode = old_timecode;

  if ((duration == -1) || (duration == (int64_t)(1000.0 / fps)))
    duration = (int64_t)(1000.0 / fps);
  else
    duration_shift += duration - (int64_t)(1000.0 / fps);
  frames_output++;

  if (bref == VFT_IFRAME) {
    // Add a key frame and save its timecode so that we can reference it later.
    add_packet(buf, size, timecode, duration, false, -1, -1, bframes ? 2 : 0);
    ref_timecode = timecode;
  } else {
    // P or B frame. Use our last timecode if the bref is -2, or the provided
    // one otherwise. The forward ref is always taken from the reader.
    add_packet(buf, size, timecode, duration, false,
               bref == VFT_PFRAMEAUTOMATIC ? ref_timecode : bref, fref, 
               fref != VFT_NOBFRAME ? 0 : bframes ? 2 : 0);
    if (fref == VFT_NOBFRAME)
      ref_timecode = timecode;
  }

  debug_leave("video_packetizer_c::process");

  return EMOREDATA;
}

video_packetizer_c::~video_packetizer_c() {
  safefree(codec_id);
}

void video_packetizer_c::dump_debug_info() {
  mxdebug("video_packetizer_c: queue: %d; frames_output: %d; "
          "ref_timecode: %lld\n", packet_queue.size(), frames_output,
          ref_timecode);
}
