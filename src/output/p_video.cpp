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
#include "hacks.h"
#include "matroska.h"
#include "mkvmerge.h"
#include "pr_generic.h"
#include "p_video.h"

extern "C" {
#include <avilib.h>
}

#define VOP_START_CODE 0x000001b6

using namespace libmatroska;

video_packetizer_c::video_packetizer_c(generic_reader_c *nreader,
                                       const char *ncodec_id,
                                       double nfps,
                                       int nwidth,
                                       int nheight,
                                       bool nbframes,
                                       track_info_c *nti)
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
  bref_frame.type = '?';
  fref_frame.type = '?';

  set_track_type(track_video);
}

void
video_packetizer_c::set_headers() {
  char *fourcc;

  is_mpeg4 = false;
  if ((ti->private_data != NULL) &&
      (ti->private_size >= sizeof(alBITMAPINFOHEADER))) {
    fourcc = (char *)&((alBITMAPINFOHEADER *)ti->private_data)->bi_compression;
    if (!strncasecmp(fourcc, "DIVX", 4) ||
        !strncasecmp(fourcc, "XVID", 4) ||
        !strncasecmp(fourcc, "DX5", 3))
      is_mpeg4 = true;
  } else if ((hcodec_id != NULL) &&
             !strncmp(hcodec_id, MKV_V_MPEG4_SP, strlen(MKV_V_MPEG4_SP) - 2))
    is_mpeg4 = true;

  if (codec_id != NULL)
    set_codec_id(codec_id);
  else if (is_mpeg4 && hack_engaged(ENGAGE_NATIVE_BFRAMES)) {
    if (bframes)
      set_codec_id(MKV_V_MPEG4_ASP);
    else
      set_codec_id(MKV_V_MPEG4_SP);
  } else
    set_codec_id(MKV_V_MSCOMP);
  if ((!is_mpeg4 || !hack_engaged(ENGAGE_NATIVE_BFRAMES)) &&
      (hcodec_id != NULL) && !strcmp(hcodec_id, MKV_V_MSCOMP) &&
      (ti->private_data != NULL) &&
      (ti->private_size >= sizeof(alBITMAPINFOHEADER)) &&
      (ti->fourcc[0] != 0))
    memcpy(&((alBITMAPINFOHEADER *)ti->private_data)->bi_compression,
           ti->fourcc, 4);
  if (!is_mpeg4 || !hack_engaged(ENGAGE_NATIVE_BFRAMES))
    set_codec_private(ti->private_data, ti->private_size);

  // Set MinCache to 1 for I- and P-frames. If you only
  // have I-frames then it can be set to 0 (e.g. MJPEG). 2 is needed
  // if there are B-frames as well.
  if (bframes)
    set_track_min_cache(2);
  else
    set_track_min_cache(1);
  if (fps != 0.0)
    set_track_default_duration((int64_t)(1000000000.0 / fps));

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
int
video_packetizer_c::process(unsigned char *buf,
                            int size,
                            int64_t old_timecode,
                            int64_t duration,
                            int64_t bref,
                            int64_t fref) {
  int64_t timecode;
  vector<video_frame_t> frames;
  uint32_t i;

  debug_enter("video_packetizer_c::process");

  if (hack_engaged(ENGAGE_NATIVE_BFRAMES) && is_mpeg4 && (pass != 1) &&
      (fps != 0.0)) {
    find_mpeg4_frame_types(buf, size, frames);
    for (i = 0; i < frames.size(); i++) {
      if ((frames[i].type == 'I') ||
          ((frames[i].type != 'B') && (fref_frame.type != '?')))
        flush_frames(frames[i].type);

      if (old_timecode == -1)
        timecode = (int64_t)(1000000000.0 * frames_output / fps) +
          duration_shift;
      else
        timecode = old_timecode;

      if ((duration == -1) || (duration == (int64_t)(1000.0 / fps)))
        duration = (int64_t)(1000000000.0 / fps);
      else
        duration_shift += duration - (int64_t)(1000000000.0 / fps);

      frames_output++;
      frames[i].timecode = timecode;
      frames[i].duration = duration;
      frames[i].data = (unsigned char *)safememdup(&buf[frames[i].pos],
                                                   frames[i].size);

      if (frames[i].type == 'I') {
        frames[i].bref = -1;
        frames[i].fref = -1;
        if (bref_frame.type == '?') {
          bref_frame = frames[i];
          add_packet(frames[i].data, frames[i].size, frames[i].timecode,
                     frames[i].duration, false, -1, -1, -1, cp_no);
        } else
          fref_frame = frames[i];

      } else if (frames[i].type != 'B') {
        frames_output--;
        if (bref_frame.type == '?')
          mxerror("video_packetizer: Found a P frame but no I frame. This "
                  "should not have happened. Either this is a bug in mkvmerge "
                  "or the video stream is damaged.\n");
        frames[i].bref = bref_frame.timecode;
        frames[i].fref = -1;
        fref_frame = frames[i];

      } else {
        if (!bframes) {
          set_codec_id(MKV_V_MPEG4_ASP);
          set_track_min_cache(2);
          rerender_track_headers();
        }
        bframes = true;
        queued_frames.push_back(frames[i]);

      }
    }

    debug_leave("video_packetizer_c::process");

    if (!duplicate_data)
      safefree(buf);

    return EMOREDATA;
  }

  if (old_timecode == -1)
    timecode = (int64_t)(1000000000.0 * frames_output / fps) + duration_shift;
  else
    timecode = old_timecode;

  if (duration == -1)
    duration = (int64_t)(1000000000.0 / fps);
  else
    duration_shift += duration - (int64_t)(1000000000.0 / fps);
  frames_output++;

  if (bref == VFT_IFRAME) {
    // Add a key frame and save its timecode so that we can reference it later.
    add_packet(buf, size, timecode, duration);
    ref_timecode = timecode;
  } else {
    // P or B frame. Use our last timecode if the bref is -2, or the provided
    // one otherwise. The forward ref is always taken from the reader.
    add_packet(buf, size, timecode, duration, false,
               bref == VFT_PFRAMEAUTOMATIC ? ref_timecode : bref, fref);
    if (fref == VFT_NOBFRAME)
      ref_timecode = timecode;
  }

  debug_leave("video_packetizer_c::process");

  return EMOREDATA;
}

video_packetizer_c::~video_packetizer_c() {
  safefree(codec_id);
}

void
video_packetizer_c::dump_debug_info() {
  mxdebug("video_packetizer_c: queue: %d; frames_output: %d; "
          "ref_timecode: %lld\n", packet_queue.size(), frames_output,
          ref_timecode);
}

void
video_packetizer_c::flush() {
  flush_frames(true);
}

void
video_packetizer_c::find_mpeg4_frame_types(unsigned char *buf,
                                           int size,
                                           vector<video_frame_t> &frames) {
  bit_cursor_c bits(buf, size);
  uint32_t marker, frame_type;
  bool first_frame;
  video_frame_t frame;
  vector<video_frame_t>::iterator fit;

  frame.pos = 0;
  frames.clear();
  first_frame = true;
  mxverb(2, "\nmpeg4_frames: start search in %d bytes\n", size);
  while (!bits.eof()) {
    if (!bits.peek_bits(32, marker))
      break;

    if ((marker & 0xffffff00) != 0x00000100) {
      bits.skip_bits(8);
      continue;
    }

    mxverb(2, "mpeg4_frames:   found start code at %d\n",
           bits.get_bit_position() / 8);
    bits.skip_bits(32);
    if (marker == VOP_START_CODE) {
      if (!bits.get_bits(2, frame_type))
        break;
      if (!first_frame) {
        frame.size = (bits.get_bit_position() / 8) - 4 - frame.pos;
        frames.push_back(frame);
        frame.pos = (bits.get_bit_position() / 8) - 4;
      } else
        first_frame = false;
      frame.type = frame_type == 0 ? 'I' : frame_type == 1 ? 'P' :
        frame_type == 2 ? 'B' : 'S';
      bits.byte_align();
    }
  }

  if (!first_frame) {
    frame.size = size - frame.pos;
    frames.push_back(frame);
  }
  mxverb(2, "mpeg4_frames:   found %d frames\n", frames.size());
  for (fit = frames.begin(); fit < frames.end(); fit++)
    mxverb(2, "mpeg4_frames:   '%c' frame with size %d at %d\n",
           fit->type, fit->size, fit->pos);

  fit = frames.begin();
  while (fit < frames.end()) {
    if (fit->size < 10)      // dummy frame
      frames.erase(fit);
    else
      fit++;
  }
}

void
video_packetizer_c::flush_frames(char next_frame,
                                 bool flush_all) {
  uint32_t i;

  if (bref_frame.type == '?') {
    if (fref_frame.type != '?')
      die("video_packetizer: bref_frame.type == '?' but fref_frame.type != "
          "'?'. This should not have happened.\n");
    if (queued_frames.size() > 0) {
      mxwarn("video_packetizer: No I frame found but B frames queued. This "
             "indicates a broken video stream.\n");
      for (i = 0; i < queued_frames.size(); i++)
        safefree(queued_frames[i].data);
      queued_frames.clear();
    }
    return;
  }

  if (fref_frame.type == '?') {
    if (queued_frames.size() != 0) {
      mxwarn("video_packetizer: B frames queued but only one reference frame "
             "found. This indicates a broken video stream, or the frames are "
             "placed in display order which is not supported.\n");
      for (i = 0; i < queued_frames.size(); i++)
        safefree(queued_frames[i].data);
      queued_frames.clear();
    }
    if (flush_all) {
      add_packet(bref_frame.data, bref_frame.size, bref_frame.duration,
                 false, bref_frame.bref, bref_frame.fref, -1, cp_no);
      bref_frame.type = '?';
    }
    return;
  }

  if (fref_frame.type != '?') {
    if ((fref_frame.type == 'P') || (fref_frame.type == 'S'))
      frames_output++;
    fref_frame.timecode = (int64_t)(fref_frame.timecode +
                                    queued_frames.size() * 1000000000 / fps);
    add_packet(fref_frame.data, fref_frame.size, fref_frame.timecode,
               fref_frame.duration, false, fref_frame.bref, fref_frame.fref,
               -1, cp_no);

    for (i = 0; i < queued_frames.size(); i++)
      add_packet(queued_frames[i].data, queued_frames[i].size,
                 queued_frames[i].timecode, queued_frames[i].duration,
                 false, bref_frame.timecode, fref_frame.timecode, -1,
                 cp_no);
    queued_frames.clear();

    bref_frame = fref_frame;
    fref_frame.type = '?';
  }

  if (flush_all || ((next_frame == 'I') && (bref_frame.type == 'P')))
    bref_frame.type = '?';
}
