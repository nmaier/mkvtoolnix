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
#include "matroska.h"
#include "mkvmerge.h"
#include "mpeg4bitstream.h"
#include "mpeg4parser.h"
#include "pr_generic.h"
#include "p_video.h"

extern "C" {
#include <avilib.h>
}

using namespace libmatroska;

video_packetizer_c::video_packetizer_c(generic_reader_c *nreader,
                                       const char *ncodec_id,
                                       double nfps, int nwidth,
                                       int nheight, bool nbframes,
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

  set_track_type(track_video);
}

void video_packetizer_c::set_headers() {
  char *fourcc;

  is_mpeg4 = false;
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

  if ((ti->private_data != NULL) &&
      (ti->private_size >= sizeof(alBITMAPINFOHEADER))) {
    fourcc = (char *)&((alBITMAPINFOHEADER *)ti->private_data)->bi_compression;
    if (!strncasecmp(fourcc, "DIVX", 4) ||
        !strncasecmp(fourcc, "XVID", 4) ||
        !strncasecmp(fourcc, "DX5", 3))
      is_mpeg4 = true;
  } else if (!strncmp(hcodec_id, MKV_V_MPEG4_SP, strlen(MKV_V_MPEG4_SP) - 2))
    is_mpeg4 = true;

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
  vector<frame_t> frames;

  debug_enter("video_packetizer_c::process");

  if (is_mpeg4)
    find_mpeg4_frame_types(buf, size, frames);

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

static bool firstframe = true;

void video_packetizer_c::find_mpeg4_frame_types(unsigned char *buf, int size,
                                                vector<frame_t> &frames) {
  bit_cursor_c bits(buf, size);
  uint32_t marker, frame_type;

  mxverb(2, "\nmpeg4_frames: start search in %d bytes\n", size);
  while (!bits.eof()) {
    if (!bits.peek_bits(32, marker))
      return;

    if ((marker & 0xffffff00) != 0x00000100) {
      bits.skip_bits(8);
      continue;
    }

    mxverb(2, "mpeg4_frames:   found start code at %d\n",
           bits.get_bit_position() / 8);
    bits.skip_bits(32);
    if (marker == VOP_START_CODE) {
      if (!bits.get_bits(2, frame_type))
        return;
      mxverb(2, "mpeg4_frames:   found '%c' frame here\n",
             frame_type == 0 ? 'I' : frame_type == 1 ? 'P' :
             frame_type == 2 ? 'B' : frame_type == 3 ? 'S' : '?');
      bits.byte_align();
    }
  }

//   DECODER *dec;
//   Bitstream bs;
//   int vop_type, frame_length, code;

//   FILE *f;
//   if (firstframe) {
//     f = fopen("firstframe", "w");
//     fwrite(buf, size, 1, f);
//     fclose(f);
//     firstframe = false;
//   }
//   mxverb(2, "\nmpeg4_frames: begin search at 0x%08x with %d\n", buf, size);
//   frames.clear();

//   dec = (DECODER *)safemalloc(sizeof(DECODER));
//   memset(dec, 0, sizeof(DECODER));
//   dec->time_inc_resolution = 1;

//   BitstreamInit(&bs, buf, size);
//   vop_type = -1;

//   while (((BitstreamPos(&bs) >> 3) < bs.length) && (vop_type < 0)) {
//     code = BitstreamShowBits(&bs, 24);
//     if (code == 1) {            //it's a startcode
//       mxverb(2, "mpeg4_frames:   pos before BitstreamReadHeaders(): %d\n",
//              BitstreamPos(&bs));
//       vop_type = BitstreamReadHeaders(&bs, dec);
//       mxverb(2, "mpeg4_frames:   pos after BitstreamReadHeaders(): %d\n",
//              BitstreamPos(&bs));
//       continue;
//     }
//     BitstreamSkip(&bs, 8);
//   }

//   if (vop_type < 0) {
//     // no frame found
//     mxverb(2, "mpeg4_frames:   no frame found\n");
//     return;
//   }

//   mxverb(2, "mpeg4_frames:   vop_type %d (%c)\n", vop_type,
//          vop_type == 0 ? 'I' : vop_type == 1 ? 'P' :
//          vop_type == 2 ? 'B' : vop_type == 3 ? 'S' :
//          vop_type == 4 ? 'N' : '?');



//   dec->frames++;

//   switch(vop_type) {
//     case I_VOP :
//       // stuff
//       break;
//     case P_VOP :
//     case S_VOP :
//       if (dec->frames == 0) // invalid
//         DPRINTF(0,"invalid frame, stream does not start with an i-frame");
//       break;
//     case N_VOP :
//       if (dec->frames == 0) // invalid
//         DPRINTF(0,"invalid frame, stream does not start with an i-frame");
//       if (dec->packed_mode && vop_type == N_VOP && dec->frames > 0) //dummy?
//         DPRINTF(0,"dummy n-vop, ignore it (I hope so!)");
//       break;
//     case B_VOP :
//       if (dec->frames == 0) // invalid
//         DPRINTF(0,"invalid frame, stream does not start with an i-frame");
//       if (dec->low_delay) {
//         DPRINTF(0, "warning: bvop found in low_delay==1 stream");
//         dec->low_delay = 1;
//       }

//       if (dec->frames < 2) // broken bframe
//         DPRINTF(0,"invalid frame, b-frame with missing reference(s)");

//   }

  // Let's look for next startcode, to find frame's length.
//   frame_length = bs.length;     // By default

//   BitstreamByteAlign(&bs);
//   while ((BitstreamPos(&bs) >> 3) < bs.length) {	
//     code = BitstreamShowBits(&bs, 24);
//     if (code == 1) {            // It's a startcode.
//       // Found the next frame.
//       BitstreamSkip(&bs, 24);
//       code <<= 8;
//       code |= BitstreamGetBits(&bs, 8);
//       if (code == VISOBJSEQ_STOP_CODE) {
//         // It's an endcode, let's include it in this frame.
//         frame_length = bs.length - (BitstreamPos(&bs) >> 3);
//       } else
//         frame_length = bs.length - (BitstreamPos(&bs) >> 3) - 4;
//       vop_type = BitstreamReadHeaders(&bs, dec);
//       mxverb(2, "mpeg4_parser:   found next frame at %d, frame_length: %d, "
//              "code: 0x%08x, vop_type: %d (%c)\n", BitstreamPos(&bs) >> 3,
//              frame_length, code, vop_type,
//              vop_type == 0 ? 'I' : vop_type == 1 ? 'P' :
//              vop_type == 2 ? 'B' : vop_type == 3 ? 'S' :
//              vop_type == 4 ? 'N' : '?');
//       break;
//     }
//     BitstreamSkip(&bs, 8);
//   }

//   safefree(dec);
}
