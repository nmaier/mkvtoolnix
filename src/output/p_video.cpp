/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   video output module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>

#include "common.h"
#include "hacks.h"
#include "matroska.h"
#include "mpeg4_common.h"
#include "output_control.h"
#include "p_video.h"

extern "C" {
#include <avilib.h>
}

using namespace libmatroska;

video_packetizer_c::video_packetizer_c(generic_reader_c *_reader,
                                       const char *_codec_id,
                                       double _fps,
                                       int _width,
                                       int _height,
                                       track_info_c *_ti):
  generic_packetizer_c(_reader, _ti),
  fps(_fps), width(_width), height(_height),
  frames_output(0), ref_timecode(-1), duration_shift(0) {

  if (get_cue_creation() == CUE_STRATEGY_UNSPECIFIED)
    set_cue_creation(CUE_STRATEGY_IFRAMES);

  set_track_type(track_video);

  if (_codec_id != NULL)
    set_codec_id(_codec_id);
  else
    set_codec_id(MKV_V_MSCOMP);

  set_codec_private(ti->private_data, ti->private_size);
  check_fourcc();
}

void
video_packetizer_c::check_fourcc() {
  if ((hcodec_id == MKV_V_MSCOMP) &&
      (ti->private_data != NULL) &&
      (ti->private_size >= sizeof(alBITMAPINFOHEADER)) &&
      (ti->fourcc[0] != 0))
    memcpy(&((alBITMAPINFOHEADER *)ti->private_data)->bi_compression,
           ti->fourcc, 4);
}

void
video_packetizer_c::set_headers() {
  if (fps > 0.0)
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
video_packetizer_c::process(memory_c &mem,
                            int64_t old_timecode,
                            int64_t duration,
                            int64_t bref,
                            int64_t fref) {
  int64_t timecode;

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
    add_packet(mem, timecode, duration);
    ref_timecode = timecode;
  } else {
    // P or B frame. Use our last timecode if the bref is -2, or the provided
    // one otherwise. The forward ref is always taken from the reader.
    add_packet(mem, timecode, duration, false,
               bref == VFT_PFRAMEAUTOMATIC ? ref_timecode : bref, fref);
    if (fref == VFT_NOBFRAME)
      ref_timecode = timecode;
  }

  return FILE_STATUS_MOREDATA;
}

void
video_packetizer_c::dump_debug_info() {
  mxdebug("video_packetizer_c: queue: %d; frames_output: %d; "
          "ref_timecode: %lld\n", packet_queue.size(), frames_output,
          ref_timecode);
}

connection_result_e
video_packetizer_c::can_connect_to(generic_packetizer_c *src) {
  video_packetizer_c *vsrc;

  vsrc = dynamic_cast<video_packetizer_c *>(src);
  if (vsrc == NULL)
    return CAN_CONNECT_NO_FORMAT;
  if ((width != vsrc->width) || (height != vsrc->height) ||
      (fps != vsrc->fps) || (hcodec_id != vsrc->hcodec_id))
    return CAN_CONNECT_NO_PARAMETERS;
  if (((ti->private_data == NULL) && (vsrc->ti->private_data != NULL)) ||
      ((ti->private_data != NULL) && (vsrc->ti->private_data == NULL)) ||
      (ti->private_size != vsrc->ti->private_size))
    return CAN_CONNECT_NO_PARAMETERS;
  if ((ti->private_data != NULL) &&
      memcmp(ti->private_data, vsrc->ti->private_data, ti->private_size))
    return CAN_CONNECT_NO_PARAMETERS;
  return CAN_CONNECT_YES;
}

// ----------------------------------------------------------------

mpeg1_2_video_packetizer_c::
mpeg1_2_video_packetizer_c(generic_reader_c *_reader,
                           int _version,
                           double _fps,
                           int _width,
                           int _height,
                           int _dwidth,
                           int _dheight,
                           bool _framed,
                           track_info_c *_ti):
  video_packetizer_c(_reader, "V_MPEG1", _fps, _width, _height, _ti),
  framed(_framed), aspect_ratio_extracted(false) {

  set_codec_id(mxsprintf("V_MPEG%d", _version));
  if (!ti->aspect_ratio_given && !ti->display_dimensions_given) {
    if ((_dwidth > 0) && (_dheight > 0)) {
      ti->display_dimensions_given = true;
      ti->display_width = _dwidth;
      ti->display_height = _dheight;
    }
  } else
    aspect_ratio_extracted = true;
}

int
mpeg1_2_video_packetizer_c::process(memory_c &mem,
                                    int64_t timecode,
                                    int64_t duration,
                                    int64_t bref,
                                    int64_t fref) {
  unsigned char *data_ptr;
  int new_bytes, state;

  if (fps < 0.0)
    extract_fps(mem.data, mem.size);

  if (!aspect_ratio_extracted)
    extract_aspect_ratio(mem.data, mem.size);

  if (framed) {
    assert(0);
  }

  state = parser.GetState();
  if ((state == MPV_PARSER_STATE_EOS) ||
      (state == MPV_PARSER_STATE_ERROR))
    return FILE_STATUS_DONE;

  data_ptr = mem.data;
  new_bytes = mem.size;

  do {
    int bytes_to_add;

    bytes_to_add = (parser.GetFreeBufferSpace() < new_bytes) ?
      parser.GetFreeBufferSpace() : new_bytes;
    if (bytes_to_add > 0) {
      parser.WriteData(data_ptr, bytes_to_add);
      data_ptr += bytes_to_add;
      new_bytes -= bytes_to_add;
    }

    state = parser.GetState();
    while (state == MPV_PARSER_STATE_FRAME) {
      MPEGFrame *frame;

      frame = parser.ReadFrame();
      if (frame == NULL)
        break;

      if (hcodec_private == NULL)
        create_private_data();

      memory_c new_mem(frame->data, frame->size, true);
      video_packetizer_c::process(new_mem, frame->timecode, frame->duration,
                                  frame->firstRef, frame->secondRef);
      frame->data = NULL;
      delete frame;

      state = parser.GetState();
    }
  } while (new_bytes > 0);

  return FILE_STATUS_MOREDATA;
}

void
mpeg1_2_video_packetizer_c::flush() {
  memory_c dummy((unsigned char *)"", 0, false);

  parser.SetEOS();
  process(dummy);
  video_packetizer_c::flush();
}

void
mpeg1_2_video_packetizer_c::extract_fps(const unsigned char *buffer,
                                        int size) {
  int idx;

  idx = mpeg1_2_extract_fps_idx(buffer, size);
  if (idx >= 0) {
    fps = mpeg1_2_get_fps(idx);
    if (fps > 0) {
      set_track_default_duration((int64_t)(1000000000.0 / fps));
      rerender_track_headers();
    } else
      fps = 0.0;
  }
}

void
mpeg1_2_video_packetizer_c::extract_aspect_ratio(const unsigned char *buffer,
                                                 int size) {
  float ar;

  if (ti->aspect_ratio_given || ti->display_dimensions_given)
    return;

  if (mpeg1_2_extract_ar(buffer, size, ar)) {
    ti->display_dimensions_given = true;
    if ((ar <= 0) || (ar == 1))
      set_video_display_width(width);
    else
      set_video_display_width((int)(height * ar));
    set_video_display_height(height);
    rerender_track_headers();
    aspect_ratio_extracted = true;
  }
}

void
mpeg1_2_video_packetizer_c::create_private_data() {
  MPEGChunk *raw_seq_hdr;

  raw_seq_hdr = parser.GetRealSequenceHeader();
  if (raw_seq_hdr != NULL) {
    set_codec_private(raw_seq_hdr->GetPointer(), raw_seq_hdr->GetSize());
    rerender_track_headers();
  }
}

// ----------------------------------------------------------------

mpeg4_p2_video_packetizer_c::
mpeg4_p2_video_packetizer_c(generic_reader_c *_reader,
                            double _fps,
                            int _width,
                            int _height,
                            bool _input_is_native,
                            track_info_c *_ti):
  video_packetizer_c(_reader, MKV_V_MPEG4_ASP, _fps, _width, _height, _ti),
  timecodes_generated(0),
  aspect_ratio_extracted(false), input_is_native(_input_is_native),
  output_is_native(hack_engaged(ENGAGE_NATIVE_MPEG4)) {

  if (input_is_native && !output_is_native)
    mxerror("mkvmerge does not support muxing from native MPEG-4 to "
            "AVI compatibility mode at the moment.\n");

  if (!output_is_native) {
    set_codec_id(MKV_V_MSCOMP);
    check_fourcc();

  } else {
    set_codec_id(MKV_V_MPEG4_ASP);
    if (!input_is_native) {
      safefree(ti->private_data);
      ti->private_data = NULL;
      ti->private_size = 0;
    }
  }
}

int
mpeg4_p2_video_packetizer_c::process(memory_c &mem,
                                     int64_t old_timecode,
                                     int64_t duration,
                                     int64_t bref,
                                     int64_t fref) {
  if (!aspect_ratio_extracted)
    extract_aspect_ratio(mem.data, mem.size);

  if (input_is_native == output_is_native)
    return
      video_packetizer_c::process(mem, old_timecode, duration, bref, fref);

  if (input_is_native)
    return process_native(mem, old_timecode, duration, bref, fref);

  return process_non_native(mem, old_timecode, duration, bref, fref);
}

int
mpeg4_p2_video_packetizer_c::process_non_native(memory_c &mem,
                                                int64_t old_timecode,
                                                int64_t old_duration,
                                                int64_t bref,
                                                int64_t fref) {
  vector<video_frame_t> frames;
  vector<video_frame_t>::iterator frame;

  if (NULL == ti->private_data) {
    uint32_t pos, size;

    if (mpeg4_p2_find_config_data(mem.data, mem.size, pos, size)) {
      ti->private_data = (unsigned char *)safememdup(&mem.data[pos], size);
      ti->private_size = size;
      set_codec_private(ti->private_data, ti->private_size);
      rerender_track_headers();
    }
  }

  mpeg4_p2_find_frame_types(mem.data, mem.size, frames);

  // Add a timecode and a duration if they've been given.
  if (-1 != old_timecode)
    available_timecodes.push_back(old_timecode);
  else if (0.0 == fps)
    mxerror("Cannot convert non-native MPEG4 video frames into native ones "
            "if the source container provides neither timecodes nor a "
            "number of frames per second.\n");
  if (-1 != old_duration)
    available_durations.push_back(old_duration);

  foreach(frame, frames) {
    // Maybe we can flush queued frames now. But only if we don't have
    // a B frame.
    if (FRAME_TYPE_B != frame->type)
      flush_frames_maybe(frame->type);

    // Add a timecode and a duration for each frame if none have been
    // given and we have a fixed number of FPS.
    if (-1 == old_timecode) {
      available_timecodes.push_back((int64_t)(timecodes_generated *
                                              1000000000.0 / fps));
      ++timecodes_generated;
    }
    if (-1 == old_duration)
      available_durations.push_back((int64_t)(1000000000.0 / fps));

    // Copy the data. If there's only one frame in this packet then
    // we might save a memcpy.
    if ((1 == frames.size()) && (frame->size == mem.size))
      frame->data = mem.grab();
    else
      frame->data = (unsigned char *)safememdup(&mem.data[frame->pos],
                                                frame->size);
    queued_frames.push_back(*frame);
  }

  return FILE_STATUS_MOREDATA;
}

int
mpeg4_p2_video_packetizer_c::process_native(memory_c &mem,
                                            int64_t old_timecode,
                                            int64_t duration,
                                            int64_t bref,
                                            int64_t fref) {
  // Not implemented yet.
  return FILE_STATUS_MOREDATA;
}

void
mpeg4_p2_video_packetizer_c::flush_frames_maybe(frame_type_e next_frame) {
  int i, num_bframes;

  if (0 == queued_frames.size())
    return;

  num_bframes = 0;
  for (i = 0; i < queued_frames.size(); ++i)
    if (FRAME_TYPE_B == queued_frames[i].type)
      ++num_bframes;

  if ((FRAME_TYPE_I == next_frame) ||
      (num_bframes > 0) || (FRAME_TYPE_P == queued_frames[0].type))
    flush_frames();
}


void
mpeg4_p2_video_packetizer_c::flush_frames() {
  int i, num_bframes, b_offset;
  int64_t b_bref, b_fref;

  if ((available_timecodes.size() < queued_frames.size()) ||
      (available_durations.size() < queued_frames.size())) {
    int64_t timecode;

    if (available_timecodes.empty())
      timecode = 0;
    else
      timecode = available_timecodes[0];

    mxerror("Invalid/unsupported sequence of MPEG4 video frames regarding "
            "B frames. If your video plays normally around timecode "
            FMT_TIMECODE " then this is a bug in mkvmerge and you should "
            "contact the author Moritz Bunkus <moritz@bunkus.org>.\n",
            ARG_TIMECODE_NS(timecode));
  }

  if ((2 <= queued_frames.size()) && (FRAME_TYPE_B != queued_frames[1].type))
    b_offset = 1;
  else
    b_offset = 0;

  num_bframes = 0;
  b_bref = last_i_p_frame;
  for (i = 0; i < queued_frames.size(); ++i) {
    if (FRAME_TYPE_I == queued_frames[i].type) {
      queued_frames[i].timecode = available_timecodes[0];
      queued_frames[i].duration = available_durations[0];
      queued_frames[i].bref = -1;
      queued_frames[i].fref = -1;
      if (-1 == last_i_p_frame) {
        last_i_p_frame = queued_frames[i].timecode;
        b_bref = queued_frames[i].timecode;
      }
      b_fref = queued_frames[i].timecode;

    } else if (FRAME_TYPE_P == queued_frames[i].type) {
      queued_frames[i].timecode =
        available_timecodes[queued_frames.size() - 1];
      queued_frames[i].duration =
        available_durations[queued_frames.size() - 1];
      queued_frames[i].bref = last_i_p_frame;
      last_i_p_frame = queued_frames[i].timecode;
      b_fref = last_i_p_frame;
      queued_frames[i].fref = -1;

    } else {
      queued_frames[i].timecode = available_timecodes[num_bframes + b_offset];
      queued_frames[i].duration = available_durations[num_bframes + b_offset];
      queued_frames[i].bref = b_bref;
      queued_frames[i].fref = b_fref;
      ++num_bframes;
    }      
  }

  for (i = 0; i < queued_frames.size(); ++i) {
    memory_c mem(queued_frames[i].data, queued_frames[i].size, true);
    add_packet(mem, queued_frames[i].timecode, queued_frames[i].duration,
               false, queued_frames[i].bref, queued_frames[i].fref);
  }

  available_timecodes.erase(available_timecodes.begin(),
                            available_timecodes.begin() +
                            queued_frames.size());
  available_durations.erase(available_durations.begin(),
                            available_durations.begin() +
                            queued_frames.size());
  queued_frames.clear();
}

void
mpeg4_p2_video_packetizer_c::flush() {
  flush_frames();
}

void
mpeg4_p2_video_packetizer_c::extract_aspect_ratio(const unsigned char *buffer,
                                                  int size) {
  uint32_t num, den;

  if (ti->aspect_ratio_given || ti->display_dimensions_given) {
    aspect_ratio_extracted = true;
    return;
  }

  if (mpeg4_p2_extract_par(buffer, size, num, den)) {
    aspect_ratio_extracted = true;
    ti->aspect_ratio_given = true;
    ti->aspect_ratio = (float)hvideo_pixel_width /
      (float)hvideo_pixel_height * (float)num / (float)den;
    generic_packetizer_c::set_headers();
    rerender_track_headers();
    mxinfo("Track %lld of '%s': Extracted the aspect ratio information "
           "from the MPEG4 layer 2 video data and set the display dimensions "
           "to %u/%u.\n", (int64_t)ti->id, ti->fname.c_str(),
           (uint32_t)ti->display_width, (uint32_t)ti->display_height);
  } else if (50 <= frames_output)
    aspect_ratio_extracted = true;
}

// ----------------------------------------------------------------

mpeg4_p10_video_packetizer_c::
mpeg4_p10_video_packetizer_c(generic_reader_c *_reader,
                             double _fps,
                             int _width,
                             int _height,
                             track_info_c *_ti):
  video_packetizer_c(_reader, MKV_V_MPEG4_AVC, _fps, _width, _height, _ti) {

  if ((ti->private_data != NULL) && (ti->private_size > 0))
    extract_aspect_ratio();
}

void
mpeg4_p10_video_packetizer_c::extract_aspect_ratio() {
  uint32_t num, den;

  if (ti->aspect_ratio_given || ti->display_dimensions_given)
    return;

  if (mpeg4_p10_extract_par(ti->private_data, ti->private_size, num, den) &&
      (0 != num) && (0 != den)) {
    double par = (double)num / (double)den;

    if (par >= 1) {
      ti->display_width = irnd(width * par);
      ti->display_height = height;

    } else {
      ti->display_width = width;
      ti->display_height = irnd(height / par);

    }

    ti->display_dimensions_given = true;
    mxinfo("Track %lld of '%s': Extracted the aspect ratio information "
           "from the MPEG-4 layer 10 (AVC) video data and set the display "
           "dimensions to %u/%u.\n", (int64_t)ti->id, ti->fname.c_str(),
           (uint32_t)ti->display_width, (uint32_t)ti->display_height);
  }
}
