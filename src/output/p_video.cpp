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
                                       track_info_c &_ti):
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

  set_codec_private(ti.private_data, ti.private_size);
  check_fourcc();
}

void
video_packetizer_c::check_fourcc() {
  if ((hcodec_id == MKV_V_MSCOMP) &&
      (ti.private_data != NULL) &&
      (ti.private_size >= sizeof(alBITMAPINFOHEADER)) &&
      !ti.fourcc.empty()) {
    memcpy(&((alBITMAPINFOHEADER *)ti.private_data)->bi_compression,
           ti.fourcc.c_str(), 4);
    set_codec_private(ti.private_data, ti.private_size);
  }
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
video_packetizer_c::process(packet_cptr packet) {
  if ((0.0 == fps) && (-1 == packet->timecode))
    mxerror(FMT_TID "video_packetizer: The FPS is 0.0 but the reader did "
            "not provide a timecode for a packet. %s\n",
            ti.fname.c_str(), (int64_t)ti.id, BUGMSG);

  if (packet->timecode == -1)
    packet->timecode = (int64_t)(1000000000.0 * frames_output / fps) +
      duration_shift;

  if (packet->duration == -1) {
    if (0.0 == fps)
      packet->duration = 0;
    else
      packet->duration = (int64_t)(1000000000.0 / fps);
  } else if (0.0 != packet->duration)
    duration_shift += packet->duration - (int64_t)(1000000000.0 / fps);
  frames_output++;

  if (VFT_IFRAME == packet->bref)
    // Add a key frame and save its timecode so that we can reference it later.
    ref_timecode = packet->timecode;

  else {
    // P or B frame. Use our last timecode if the bref is -2, or the provided
    // one otherwise. The forward ref is always taken from the reader.
    if (VFT_PFRAMEAUTOMATIC == packet->bref)
      packet->bref = ref_timecode;
    if (VFT_NOBFRAME == packet->fref)
      ref_timecode = packet->timecode;
  }
  add_packet(packet);

  return FILE_STATUS_MOREDATA;
}

void
video_packetizer_c::dump_debug_info() {
  mxdebug("video_packetizer_c: queue: %u; frames_output: %d; "
          "ref_timecode: " LLD "\n", (unsigned int)packet_queue.size(),
          frames_output, ref_timecode);
}

connection_result_e
video_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                   string &error_message) {
  video_packetizer_c *vsrc;

  vsrc = dynamic_cast<video_packetizer_c *>(src);
  if (vsrc == NULL)
    return CAN_CONNECT_NO_FORMAT;
  connect_check_v_width(width, vsrc->width);
  connect_check_v_height(height, vsrc->height);
  connect_check_codec_id(hcodec_id, vsrc->hcodec_id);

  if (((ti.private_data == NULL) && (vsrc->ti.private_data != NULL)) ||
      ((ti.private_data != NULL) && (vsrc->ti.private_data == NULL)) ||
      (ti.private_size != vsrc->ti.private_size)) {
    error_message = mxsprintf("The codec's private data does not match "
                              "(lengths: %d and %d).", ti.private_size,
                              vsrc->ti.private_size);
    return CAN_CONNECT_MAYBE_CODECPRIVATE;
  }
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
                           track_info_c &_ti):
  video_packetizer_c(_reader, "V_MPEG1", _fps, _width, _height, _ti),
  framed(_framed), aspect_ratio_extracted(false) {

  set_codec_id(mxsprintf("V_MPEG%d", _version));
  if (!ti.aspect_ratio_given && !ti.display_dimensions_given) {
    if ((_dwidth > 0) && (_dheight > 0)) {
      aspect_ratio_extracted = true;
      ti.display_dimensions_given = true;
      ti.display_width = _dwidth;
      ti.display_height = _dheight;
    }
  } else
    aspect_ratio_extracted = true;

  timecode_factory_application_mode = TFA_SHORT_QUEUEING;
  if (hack_engaged(ENGAGE_USE_CODEC_STATE))
      parser.SeparateSequenceHeaders();
}

int
mpeg1_2_video_packetizer_c::process_framed(packet_cptr packet) {
  if (0 == packet->data->get_size())
    return FILE_STATUS_MOREDATA;

  if (!hack_engaged(ENGAGE_USE_CODEC_STATE) ||
      (4 > packet->data->get_size()))
    return video_packetizer_c::process(packet);

  unsigned char *buf = packet->data->get();
  int pos = 4, marker, size = packet->data->get_size();

  marker = get_uint32_be(buf);
  while ((pos < size) && (MPEGVIDEO_SEQUENCE_START_CODE != marker)) {
    marker <<= 8;
    marker |= buf[pos];
    ++pos;
  }

  if ((MPEGVIDEO_SEQUENCE_START_CODE != marker) ||
      ((pos + 4) >= size))
    return video_packetizer_c::process(packet);

  int start = pos - 4;
  marker = get_uint32_be(&buf[pos]);
  pos += 4;

  while ((pos < size) &&
         ((MPEGVIDEO_EXT_START_CODE == marker) ||
          (0x00000100 != (marker & 0xffffff00)))) {
    marker <<= 8;
    marker |= buf[pos];
    ++pos;
  }

  if (pos >= size)
    return video_packetizer_c::process(packet);

  pos -= 4;
  int sh_size = pos - start;

  if ((NULL == seq_hdr.get()) ||
      (sh_size != seq_hdr->get_size()) ||
      memcmp(&buf[start], seq_hdr->get(), sh_size)) {
    seq_hdr = clone_memory(&buf[start], sh_size);
    packet->codec_state = clone_memory(&buf[start], sh_size);
  }

  memmove(&buf[start], &buf[pos], size - pos);
  packet->data->set_size(size - sh_size);

  return video_packetizer_c::process(packet);
}

int
mpeg1_2_video_packetizer_c::process(packet_cptr packet) {
  unsigned char *data_ptr;
  int new_bytes, state;

  if (fps < 0.0)
    extract_fps(packet->data->get(), packet->data->get_size());

  if (!aspect_ratio_extracted)
    extract_aspect_ratio(packet->data->get(), packet->data->get_size());

  if (framed)
    return process_framed(packet);

  state = parser.GetState();
  if ((state == MPV_PARSER_STATE_EOS) ||
      (state == MPV_PARSER_STATE_ERROR))
    return FILE_STATUS_DONE;

  memory_cptr old_memory = packet->data;
  data_ptr = old_memory->get();
  new_bytes = old_memory->get_size();

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

      packet_t *new_packet =
        new packet_t(new memory_c(frame->data, frame->size, true),
                     frame->timecode, frame->duration,
                     frame->firstRef, frame->secondRef);
      new_packet->time_factor =
        MPEG2_PICTURE_TYPE_FRAME == frame->pictureStructure ? 1 : 2;

      if ((NULL != frame->seqHdrData) &&
          ((NULL == seq_hdr.get()) ||
           (frame->seqHdrDataSize != seq_hdr->get_size()) ||
           memcmp(frame->seqHdrData, seq_hdr->get(), frame->seqHdrDataSize))) {
        seq_hdr = memory_cptr(new memory_c(frame->seqHdrData,
                                           frame->seqHdrDataSize, true));
        frame->seqHdrData = NULL;
        new_packet->codec_state = clone_memory(seq_hdr);
      }

      video_packetizer_c::process(packet_cptr(new_packet));
      frame->data = NULL;
      delete frame;

      state = parser.GetState();
    }
  } while (new_bytes > 0);

  return FILE_STATUS_MOREDATA;
}

void
mpeg1_2_video_packetizer_c::flush() {
  parser.SetEOS();
  generic_packetizer_c::process(new packet_t(new memory_c((unsigned char *)"",
                                                          0, false)));
  video_packetizer_c::flush();
}

void
mpeg1_2_video_packetizer_c::extract_fps(const unsigned char *buffer,
                                        int size) {
  int idx;

  idx = mpeg1_2::extract_fps_idx(buffer, size);
  if (idx >= 0) {
    fps = mpeg1_2::get_fps(idx);
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

  if (ti.aspect_ratio_given || ti.display_dimensions_given)
    return;

  if (mpeg1_2::extract_ar(buffer, size, ar)) {
    ti.display_dimensions_given = true;
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
                            track_info_c &_ti):
  video_packetizer_c(_reader, MKV_V_MPEG4_ASP, _fps, _width, _height, _ti),
  timecodes_generated(0), last_i_p_frame(-1), previous_timecode(0),
  aspect_ratio_extracted(false), input_is_native(_input_is_native),
  output_is_native(hack_engaged(ENGAGE_NATIVE_MPEG4)),
  size_extracted(false) {

  if (input_is_native)
    output_is_native = true;

  if (!output_is_native) {
    set_codec_id(MKV_V_MSCOMP);
    check_fourcc();

  } else {
    set_codec_id(MKV_V_MPEG4_ASP);
    if (!input_is_native) {
      safefree(ti.private_data);
      ti.private_data = NULL;
      ti.private_size = 0;
    }
  }

  timecode_factory_application_mode = TFA_SHORT_QUEUEING;
}

int
mpeg4_p2_video_packetizer_c::process(packet_cptr packet) {
  if (!size_extracted)
    extract_size(packet->data->get(), packet->data->get_size());
  if (!aspect_ratio_extracted)
    extract_aspect_ratio(packet->data->get(), packet->data->get_size());

  if (input_is_native == output_is_native)
    return video_packetizer_c::process(packet);

  if (input_is_native)
    return process_native(packet);

  return process_non_native(packet);
}

int
mpeg4_p2_video_packetizer_c::process_non_native(packet_cptr packet) {
  vector<video_frame_t> frames;
  vector<video_frame_t>::iterator frame;

  if (NULL == ti.private_data) {
    memory_c *config_data;

    config_data = mpeg4::p2::parse_config_data(packet->data->get(),
                                               packet->data->get_size());
    if (NULL != config_data) {
      ti.private_data = (unsigned char *)safememdup(config_data->get(),
                                                    config_data->get_size());
      ti.private_size = config_data->get_size();
      delete config_data;
      fix_codec_string();
      set_codec_private(ti.private_data, ti.private_size);
      rerender_track_headers();

    } else
      mxerror("Could not find the codec configuration data in the first "
              "MPEG-4 part 2 video frame. This track cannot be stored in "
              "native mode.\n");
  }

  mpeg4::p2::find_frame_types(packet->data->get(), packet->data->get_size(),
                              frames);

  // Add a timecode and a duration if they've been given.
  if (-1 != packet->timecode)
    available_timecodes.push_back(packet->timecode);
  else if (0.0 == fps)
    mxerror("Cannot convert non-native MPEG4 video frames into native ones "
            "if the source container provides neither timecodes nor a "
            "number of frames per second.\n");
  if (-1 != packet->duration)
    available_durations.push_back(packet->duration);

  mxforeach(frame, frames) {
    // Maybe we can flush queued frames now. But only if we don't have
    // a B frame.
    if (FRAME_TYPE_B != frame->type)
      flush_frames_maybe(frame->type);

    // Add a timecode and a duration for each frame if none have been
    // given and we have a fixed number of FPS.
    if (-1 == packet->timecode) {
      available_timecodes.push_back((int64_t)(timecodes_generated *
                                              1000000000.0 / fps));
      ++timecodes_generated;
    }
    if (-1 == packet->duration)
      available_durations.push_back((int64_t)(1000000000.0 / fps));

    frame->data = (unsigned char *)
      safememdup(packet->data->get() + frame->pos, frame->size);
    queued_frames.push_back(*frame);
  }

  previous_timecode = available_timecodes.back();

  return FILE_STATUS_MOREDATA;
}

void
mpeg4_p2_video_packetizer_c::fix_codec_string() {
  static const unsigned char start_code[4] = {0x00, 0x00, 0x01, 0xb2};
  unsigned char *end_pos;
  int i, size;

  if ((NULL == ti.private_data) || (0 == ti.private_size))
    return;

  size = ti.private_size;
  for (i = 0; 9 < size;) {
    if (memcmp(&ti.private_data[i], start_code, 4) != 0) {
      ++i;
      --size;
      continue;
    }

    i += 8;
    size -= 8;
    if (strncasecmp((const char *)&ti.private_data[i - 4], "divx", 4) != 0)
      continue;

    end_pos = (unsigned char *)memchr(&ti.private_data[i], 0, size);
    if (NULL == end_pos)
      end_pos = &ti.private_data[i + size];

    --end_pos;
    if ('p' == *end_pos)
      *end_pos = 'n';

    return;
  }
}

int
mpeg4_p2_video_packetizer_c::process_native(packet_cptr packet) {
  // Not implemented yet.
  return FILE_STATUS_MOREDATA;
}

void
mpeg4_p2_video_packetizer_c::flush_frames_maybe(frame_type_e next_frame) {
  int i, num_bframes;

  if ((0 == queued_frames.size()) || (FRAME_TYPE_B == next_frame))
    return;

  if ((FRAME_TYPE_I == next_frame) ||
      (FRAME_TYPE_P == queued_frames[0].type)) {
    flush_frames(false);
    return;
  }

  num_bframes = 0;
  for (i = 0; i < queued_frames.size(); ++i)
    if (FRAME_TYPE_B == queued_frames[i].type)
      ++num_bframes;

  if ((0 < num_bframes) || (2 <= queued_frames.size()))
    flush_frames(false);
}

/** \brief Handle frame sequences in which too few timecodes are available

   This function gets called if mkvmerge wants to flush its frame queue
   but it doesn't have enough timecodes and/or durations available for
   each queued frame. This can happen in two cases:

   1. A picture sequence is found that mkvmerge does not support. An
      example: Two frames have been read. The first contained a
      P and a B frame (that's OK so far), but the second one contained
      another P or I frame without an intermediate dummy frame.

   2. The end of the file has been reached but the frame queue contains
      more frames than the timecode queue. For example: The last frame
      contained two frames, a P and a B frame. Right afterwards the
      end of the file is reached. In this case a dummy frame is missing.

   Both cases can be solved if the source file provides a FPS for this
   track. Otherwise such frames have to be dropped.
*/
void
mpeg4_p2_video_packetizer_c::handle_missing_timecodes(bool end_of_file) {
  if (0.0 < fps) {
    while (available_timecodes.size() < queued_frames.size()) {
      previous_timecode = (int64_t)(previous_timecode +  1000000000.0 / fps);
      available_timecodes.push_back(previous_timecode);
      mxverb(3, "mpeg4_p2::flush_frames(): Needed new timecode " LLD "\n",
             previous_timecode);
    }
    while (available_durations.size() < queued_frames.size()) {
      available_durations.push_back((int64_t)(1000000000.0 / fps));
      mxverb(3, "mpeg4_p2::flush_frames(): Needed new duration " LLD "\n",
             available_durations.back());
    }

  } else {
    int64_t timecode;
    int num_dropped, i;

    if (available_timecodes.size() < available_durations.size()) {
      num_dropped = queued_frames.size() - available_timecodes.size();
      available_durations.erase(available_durations.begin() +
                                available_timecodes.size(),
                                available_durations.end());
    } else {
      num_dropped = queued_frames.size() - available_durations.size();
      available_timecodes.erase(available_timecodes.begin() +
                                available_durations.size(),
                                available_timecodes.end());
    }

    for (i = available_timecodes.size(); queued_frames.size() > i; ++i)
      safefree(queued_frames[i].data);
    queued_frames.erase(queued_frames.begin() + available_timecodes.size(),
                        queued_frames.end());

    if (available_timecodes.empty())
      timecode = 0;
    else
      timecode = available_timecodes.front();

    mxwarn(FMT_TID "During MPEG-4 part 2 B frame handling: The frame queue "
           "contains more frames than timecodes are available that "
           "can be assigned to them because %s. Therefore %d frame%s to be "
           "dropped. The video might be broken around timecode "
           FMT_TIMECODE ".\n", ti.fname.c_str(), (int64_t)ti.id,
           end_of_file ? "the end of the source file was encountered" :
           "an unsupported sequence of frames was encountered",
           num_dropped, 1 == num_dropped ? " has" : "s have",
           ARG_TIMECODE_NS(timecode));
  }
}

void
mpeg4_p2_video_packetizer_c::flush_frames(bool end_of_file) {
  int i;

  if (queued_frames.empty())
    return;

  if ((available_timecodes.size() < queued_frames.size()) ||
      (available_durations.size() < queued_frames.size()))
    handle_missing_timecodes(end_of_file);

  // Very first frame?
  if (-1 == last_i_p_frame) {
    video_frame_t &frame = queued_frames.front();
    last_i_p_frame = available_timecodes[0];
    add_packet(new packet_t(new memory_c(frame.data, frame.size, true),
                            last_i_p_frame, available_durations[0], -1, -1));
    available_timecodes.erase(available_timecodes.begin(),
                              available_timecodes.begin() + 1);
    available_durations.erase(available_durations.begin(),
                              available_durations.begin() + 1);
    queued_frames.pop_front();
    flush_frames(end_of_file);
    return;
  }

  queued_frames[0].timecode = available_timecodes[queued_frames.size() - 1];
  queued_frames[0].duration = available_durations[queued_frames.size() - 1];
  queued_frames[0].fref = -1;
  if (FRAME_TYPE_I == queued_frames[0].type)
    queued_frames[0].bref = -1;
  else
    queued_frames[0].bref = last_i_p_frame;

  for (i = 1; queued_frames.size() > i; ++i) {
    queued_frames[i].timecode = available_timecodes[i - 1];
    queued_frames[i].duration = available_durations[i - 1];
    queued_frames[i].fref = queued_frames[0].timecode;
    queued_frames[i].bref = last_i_p_frame;
  }

  last_i_p_frame = queued_frames[0].timecode;

  for (i = 0; i < queued_frames.size(); ++i)
    add_packet(new packet_t(new memory_c(queued_frames[i].data,
                                         queued_frames[i].size, true),
                            queued_frames[i].timecode,
                            queued_frames[i].duration,
                            queued_frames[i].bref,
                            queued_frames[i].fref));

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
  flush_frames(true);
  generic_packetizer_c::flush();
}

void
mpeg4_p2_video_packetizer_c::extract_aspect_ratio(const unsigned char *buffer,
                                                  int size) {
  uint32_t num, den;

  if (ti.aspect_ratio_given || ti.display_dimensions_given) {
    aspect_ratio_extracted = true;
    return;
  }

  if (mpeg4::p2::extract_par(buffer, size, num, den)) {
    aspect_ratio_extracted = true;
    ti.aspect_ratio_given = true;
    ti.aspect_ratio = (float)hvideo_pixel_width /
      (float)hvideo_pixel_height * (float)num / (float)den;
    generic_packetizer_c::set_headers();
    rerender_track_headers();
    mxinfo("Track " LLD " of '%s': Extracted the aspect ratio information "
           "from the MPEG4 layer 2 video data and set the display dimensions "
           "to %u/%u.\n", (int64_t)ti.id, ti.fname.c_str(),
           (uint32_t)ti.display_width, (uint32_t)ti.display_height);
  } else if (50 <= frames_output)
    aspect_ratio_extracted = true;
}

void
mpeg4_p2_video_packetizer_c::extract_size(const unsigned char *buffer,
                                          int size) {
  uint32_t width, height;

  if (mpeg4::p2::extract_size(buffer, size, width, height)) {
    size_extracted = true;
    if (!reader->appending &&
        ((width != hvideo_pixel_width) || (height != hvideo_pixel_height))) {
      set_video_pixel_width(width);
      set_video_pixel_height(height);
      if (!output_is_native &&
          (ti.private_size >= sizeof(alBITMAPINFOHEADER))) {
        alBITMAPINFOHEADER *bih = (alBITMAPINFOHEADER *)ti.private_data;
        put_uint32_le(&bih->bi_width, width);
        put_uint32_le(&bih->bi_height, height);
        set_codec_private(ti.private_data, ti.private_size);
      }
      generic_packetizer_c::set_headers();
      rerender_track_headers();
      mxinfo("Track " LLD " of '%s': The extracted values for video width and "
             "height from the MPEG4 layer 2 video data bitstream differ from "
             "what the values in the source container. The ones from the "
             "video data bitstream (%ux%u) will be used.\n",
             (int64_t)ti.id, ti.fname.c_str(), width, height);
    }

  } else if (50 <= frames_output)
    aspect_ratio_extracted = true;
}

// ----------------------------------------------------------------

mpeg4_p10_video_packetizer_c::
mpeg4_p10_video_packetizer_c(generic_reader_c *_reader,
                             double _fps,
                             int _width,
                             int _height,
                             track_info_c &_ti):
  video_packetizer_c(_reader, MKV_V_MPEG4_AVC, _fps, _width, _height, _ti),
  m_nalu_size_len_src(0), m_nalu_size_len_dst(0), m_max_nalu_size(0) {

  relaxed_timecode_checking = true;

  if ((ti.private_data != NULL) && (ti.private_size > 0)) {
    extract_aspect_ratio();
    setup_nalu_size_len_change();
  }
}

void
mpeg4_p10_video_packetizer_c::extract_aspect_ratio() {
  uint32_t num, den;
  unsigned char *priv;

  priv = ti.private_data;
  if (mpeg4::p10::extract_par(ti.private_data, ti.private_size, num, den) &&
      (0 != num) && (0 != den)) {
    if (!ti.aspect_ratio_given && !ti.display_dimensions_given) {
      double par = (double)num / (double)den;

      if (par >= 1) {
        ti.display_width = irnd(width * par);
        ti.display_height = height;

      } else {
        ti.display_width = width;
        ti.display_height = irnd(height / par);

      }

      ti.display_dimensions_given = true;
      mxinfo("Track " LLD " of '%s': Extracted the aspect ratio information "
             "from the MPEG-4 layer 10 (AVC) video data and set the display "
             "dimensions to %u/%u.\n", (int64_t)ti.id, ti.fname.c_str(),
             (uint32_t)ti.display_width, (uint32_t)ti.display_height);
    }

    set_codec_private(ti.private_data, ti.private_size);
  }

  if (priv != ti.private_data)
    safefree(priv);
}

int
mpeg4_p10_video_packetizer_c::process(packet_cptr packet) {
  if (VFT_PFRAMEAUTOMATIC == packet->bref) {
    packet->fref = -1;
    packet->bref = ref_timecode;
  }

  ref_timecode = packet->timecode;

  if (m_nalu_size_len_dst)
    change_nalu_size_len(packet);

  add_packet(packet);

  return FILE_STATUS_MOREDATA;
}

connection_result_e
mpeg4_p10_video_packetizer_c::can_connect_to(generic_packetizer_c *src,
                                             string &error_message) {
  connection_result_e result;
  mpeg4_p10_video_packetizer_c *vsrc;

  vsrc = dynamic_cast<mpeg4_p10_video_packetizer_c *>(src);
  if (NULL == vsrc)
    return CAN_CONNECT_NO_FORMAT;

  result = video_packetizer_c::can_connect_to(src, error_message);
  if (CAN_CONNECT_YES != result)
    return result;

  if ((NULL != ti.private_data) &&
      memcmp(ti.private_data, vsrc->ti.private_data, ti.private_size)) {
    error_message = mxsprintf("The codec's private data does not match."
                              "Both have the same length (%d) but different "
                              "content.", ti.private_size);
    return CAN_CONNECT_MAYBE_CODECPRIVATE;
  }

  return CAN_CONNECT_YES;
}

void
mpeg4_p10_video_packetizer_c::setup_nalu_size_len_change() {
  if (!ti.private_data || (5 > ti.private_size))
    return;

  m_nalu_size_len_src = (ti.private_data[4] & 0x03) + 1;

  if (!ti.nalu_size_length || (ti.nalu_size_length == m_nalu_size_len_src))
    return;

  m_nalu_size_len_dst = ti.nalu_size_length;
  ti.private_data[4]  = (ti.private_data[4] & 0xfc) | (m_nalu_size_len_dst - 1);
  m_max_nalu_size     = 1ll << (8 * m_nalu_size_len_dst);

  set_codec_private(ti.private_data, ti.private_size);

  mxverb(2, "mpeg4_p10: Adjusting NALU size length from %d to %d\n",
         m_nalu_size_len_src, m_nalu_size_len_dst);
}

void
mpeg4_p10_video_packetizer_c::change_nalu_size_len(packet_cptr packet) {
  unsigned char *src = packet->data->get(), *dst;
  int size           = packet->data->get_size();
  int src_pos, dst_pos, nalu_size, i, shift;

  if (!src || !size)
    return;

  vector<int> nalu_sizes;

  src_pos = 0;

  // Find all NALU sizes in this packet.
  while (src_pos < size) {
    if ((size - src_pos) < m_nalu_size_len_src)
      break;

    nalu_size = 0;
    for (i = 0; i < m_nalu_size_len_src; ++i)
      nalu_size = (nalu_size << 8) + src[src_pos + i];

    if ((size - src_pos - m_nalu_size_len_src) < nalu_size)
      nalu_size = size - src_pos - m_nalu_size_len_src;

    if (nalu_size > m_max_nalu_size)
      mxerror(FMT_TID "The chosen NALU size length of %d is too small. Try using '4'.\n",
              ti.fname.c_str(), (int64_t)ti.id, m_nalu_size_len_dst);

    src_pos += m_nalu_size_len_src + nalu_size;

    nalu_sizes.push_back(nalu_size);
  }

  // Allocate memory if the new NALU size length is greater
  // than the previous one. Otherwise reuse the existing memory.
  if (m_nalu_size_len_dst > m_nalu_size_len_src) {
    int new_size = size + nalu_sizes.size() * (m_nalu_size_len_dst - m_nalu_size_len_src);
    packet->data = memory_cptr(new memory_c((unsigned char *)safemalloc(new_size), new_size, true));
  }

  // Copy the NALUs and write the new sized length field.
  dst     = packet->data->get();
  src_pos = 0;
  dst_pos = 0;

  for (i = 0; nalu_sizes.size() > i; ++i) {
    nalu_size = nalu_sizes[i];

    for (shift = 0; shift < m_nalu_size_len_dst; ++shift)
      dst[dst_pos + shift] = (nalu_size >> (8 * (m_nalu_size_len_dst - 1 - shift))) & 0xff;

    memmove(&dst[dst_pos + m_nalu_size_len_dst], &src[src_pos + m_nalu_size_len_src], nalu_size);

    src_pos += m_nalu_size_len_src + nalu_size;
    dst_pos += m_nalu_size_len_dst + nalu_size;
  }

  packet->data->set_size(dst_pos);
}

