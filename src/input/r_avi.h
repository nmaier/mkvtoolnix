/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * class definitions for the AVI demultiplexer module
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
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

#define RAVI_UNKNOWN 0
#define RAVI_DIVX3   1
#define RAVI_MPEG4   2

typedef struct avi_demuxer_t {
  int ptzr;
  int channels, bits_per_sample, samples_per_second, aid;
  int64_t bytes_processed;
} avi_demuxer_t;

class avi_reader_c: public generic_reader_c {
private:
  unsigned char *chunk, *old_chunk;
  avi_t *avi;
  mm_io_c *io;
  int vptzr;
  vector<avi_demuxer_t> ademuxers;
  double fps;
  int frames, max_frame_size, act_wchar, old_key, old_nread, dropped_frames;
  int video_done, maxframes, is_divx, rederive_keyframes, chunk_size;

public:
  avi_reader_c(track_info_c *nti) throw (error_c);
  virtual ~avi_reader_c();

  virtual int read(generic_packetizer_c *ptzr, bool force = false);
  virtual int display_priority();
  virtual void display_progress(bool final = false);
  virtual void identify();
  virtual void create_packetizers();
  virtual void create_packetizer(int64_t tid);
  virtual void add_available_track_ids();

  static int probe_file(mm_io_c *mm_io, int64_t size);

protected:
  virtual void add_audio_demuxer(int aid);
  virtual int is_keyframe(unsigned char *data, long size, int suggestion);
};

#endif  // __R_AVI_H
