/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definitions for the RealMedia demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_REAL_H
#define __R_REAL_H

#include "os.h"

#include <stdio.h>

#include <vector>

#include "common.h"
#include "error.h"
#include "librmff.h"
#include "p_video.h"
#include "pr_generic.h"

typedef struct {
  unsigned char *data;
  uint64_t size, flags;
} rv_segment_t;

typedef struct {
  int ptzr;
  rmff_track_t *track;

  int bsid, channels, samples_per_second, bits_per_sample;
  int width, height;
  char fourcc[5];
  bool is_aac;
  bool rv_dimensions;
  float fps;

  real_video_props_t *rvp;
  real_audio_v4_props_t *ra4p;
  real_audio_v5_props_t *ra5p;

  unsigned char *private_data, *extra_data;
  int private_size, extra_data_size;

  int num_packets;
  int64_t last_timecode, ref_timecode;

  vector<rv_segment_t> *segments;
} real_demuxer_t;

class real_reader_c: public generic_reader_c {
private:
  rmff_file_t *file;
  vector<real_demuxer_t *> demuxers;
  int64_t file_size;
  bool done;

public:
  real_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~real_reader_c();

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual int get_progress();
  virtual void identify();
  virtual void create_packetizers();
  virtual void create_packetizer(int64_t tid);
  virtual void add_available_track_ids();

  static int probe_file(mm_io_c *mm_io, int64_t size);

protected:
  virtual void parse_headers();
  virtual real_demuxer_t *find_demuxer(int id);
  virtual void assemble_video_packet(real_demuxer_t *dmx, rmff_frame_t *frame);
  virtual file_status_e finish();
  virtual bool get_rv_dimensions(unsigned char *buf, int size, uint32_t &width,
                                 uint32_t &height);
  virtual void set_dimensions(real_demuxer_t *dmx, unsigned char *buffer,
                              int size);
  virtual void get_information_from_data();
  virtual void flush_packetizers();
  virtual void deliver_aac_frames(real_demuxer_t *dmx, memory_c &mem);
  virtual void queue_audio_frames(real_demuxer_t *dmx, memory_c &mem,
                                  uint64_t timecode, uint32_t flags);
  virtual void queue_one_audio_frame(real_demuxer_t *dmx, memory_c &mem,
                                     uint64_t timecode, uint32_t flags);
  virtual void deliver_audio_frames(real_demuxer_t *dmx, uint64_t duration);
};

#endif  // __R_REAL_H
