/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_avi.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file r_avi.h
    \version $Id$
    \brief class definitions for the AVI demultiplexer module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __R_AVI_H
#define __R_AVI_H

#include "os.h"

#include <stdio.h>

#include <vector>

extern "C" {
#include <avilib.h>
}

#include "mm_io.h"
#include "pr_generic.h"
#include "common.h"
#include "error.h"
#include "p_video.h"

#define RAVI_UNKNOWN 0
#define RAVI_DIVX3   1
#define RAVI_MPEG4   2

typedef struct avi_demuxer_t {
  generic_packetizer_c *packetizer;
  int channels, bits_per_sample, samples_per_second;
  int aid;
  int eos;
  int64_t bytes_processed;
} avi_demuxer_t;

class avi_reader_c: public generic_reader_c {
private:
  unsigned char *chunk, *old_chunk;
  avi_t *avi;
  video_packetizer_c *vpacketizer;
  vector<avi_demuxer_t *> ademuxers;
  double fps;
  int frames, max_frame_size, act_wchar, old_key, old_nread, dropped_frames;
  int video_done, maxframes, is_divx, rederive_keyframes;

public:
  avi_reader_c(track_info_t *nti) throw (error_c);
  virtual ~avi_reader_c();

  virtual int read();
  virtual packet_t *get_packet();
  virtual int display_priority();
  virtual void display_progress();
  virtual void set_headers();
  virtual void identify();

  static int probe_file(mm_io_c *mm_io, int64_t size);

private:
  virtual void add_audio_demuxer(avi_t *avi, int aid);
  virtual int is_keyframe(unsigned char *data, long size, int suggestion);
};

#endif  // __R_AVI_H
