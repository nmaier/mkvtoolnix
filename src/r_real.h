/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_real.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file r_avi.h
    \version $Id$
    \brief class definitions for the RealMedia demultiplexer module
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __R_REAL_H
#define __R_REAL_H

#include "os.h"

#include <stdio.h>

#include <vector>

#include "mm_io.h"
#include "pr_generic.h"
#include "common.h"
#include "error.h"
#include "p_video.h"

typedef struct {
  unsigned char *data;
  uint64_t size, offset;
} rv_segment_t;

typedef struct {
  generic_packetizer_c *packetizer;
  int id;

  uint32_t start_time, preroll;

  int channels, bits_per_sample, samples_per_second;

  int width, height;
  float fps;

  char fourcc[5], type;

  unsigned char *private_data;
  int private_size;

  unsigned char *c_data;
  int c_len, c_numpackets;
  int64_t c_timecode, c_reftimecode;
  bool c_keyframe;

  vector<rv_segment_t> *segments;
  bool f_merged;
} real_demuxer_t;

class real_reader_c: public generic_reader_c {
private:
  mm_io_c *io;
  vector<real_demuxer_t *> demuxers;
  int act_wchar;
  int64_t file_size, last_timecode;
  bool done;

public:
  real_reader_c(track_info_t *nti) throw (error_c);
  virtual ~real_reader_c();

  virtual int read();
  virtual packet_t *get_packet();
  virtual int display_priority();
  virtual void display_progress();
  virtual void set_headers();
  virtual void identify();

  static int probe_file(mm_io_c *mm_io, int64_t size);

protected:
  virtual void parse_headers();
  virtual void create_packetizers();
  virtual real_demuxer_t *find_demuxer(int id);
  virtual void assemble_packet(real_demuxer_t *dmx, unsigned char *p, int size,
                               int64_t timecode, bool keyframe);
  virtual void deliver_segments(real_demuxer_t *dmx, int64_t timecode);
  virtual int finish();
};

#endif  // __R_REAL_H
