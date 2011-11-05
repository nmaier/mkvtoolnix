/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the RealMedia demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_REAL_H
#define __R_REAL_H

#include "common/common_pch.h"

#include <stdio.h>

#include "common/error.h"
#include "librmff/librmff.h"
#include "output/p_video.h"
#include "merge/pr_generic.h"

typedef struct {
  memory_cptr data;
  uint64_t flags;
} rv_segment_t;

typedef counted_ptr<rv_segment_t> rv_segment_cptr;

struct real_demuxer_t {
  int ptzr;
  rmff_track_t *track;

  int bsid;
  unsigned int channels, samples_per_second, bits_per_sample;
  unsigned int width, height;
  char fourcc[5];
  bool is_aac;
  bool rv_dimensions;
  bool force_keyframe_flag;
  bool cook_audio_fix;
  float fps;

  real_video_props_t *rvp;
  real_audio_v4_props_t *ra4p;
  real_audio_v5_props_t *ra5p;

  unsigned char *private_data, *extra_data;
  unsigned int private_size, extra_data_size;

  bool first_frame;
  int num_packets;
  uint64_t last_timecode;
  int64_t ref_timecode;         // can be negative

  std::vector<rv_segment_cptr> segments;

  real_demuxer_t(rmff_track_t *n_track):
    ptzr(-1),
    track(n_track),
    bsid(0),
    channels(0),
    samples_per_second(0),
    bits_per_sample(0),
    width(0),
    height(0),
    is_aac(false),
    rv_dimensions(false),
    force_keyframe_flag(false),
    cook_audio_fix(false),
    fps(0.0),
    rvp(NULL),
    ra4p(NULL),
    ra5p(NULL),
    private_data(NULL),
    extra_data(NULL),
    private_size(0),
    extra_data_size(0),
    first_frame(true),
    num_packets(0),
    last_timecode(0),
    ref_timecode(0) {

    memset(fourcc, 0, 5);
  };
};

typedef counted_ptr<real_demuxer_t> real_demuxer_cptr;

class real_reader_c: public generic_reader_c {
private:
  rmff_file_t *file;
  std::vector<counted_ptr<real_demuxer_t> > demuxers;
  int64_t file_size;
  bool done;

public:
  real_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~real_reader_c();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("RealMedia") : "RealMedia";
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual int get_progress();
  virtual void identify();
  virtual void create_packetizers();
  virtual void create_packetizer(int64_t tid);
  virtual void add_available_track_ids();

  static int probe_file(mm_io_c *io, uint64_t size);

protected:
  virtual void parse_headers();
  virtual real_demuxer_cptr find_demuxer(unsigned int id);
  virtual void assemble_video_packet(real_demuxer_cptr dmx, rmff_frame_t *frame);
  virtual file_status_e finish();
  virtual bool get_rv_dimensions(unsigned char *buf, int size, uint32_t &width, uint32_t &height);
  virtual void set_dimensions(real_demuxer_cptr dmx, unsigned char *buffer, int size);
  virtual void get_information_from_data();
  virtual void deliver_aac_frames(real_demuxer_cptr dmx, memory_c &mem);
  virtual void queue_audio_frames(real_demuxer_cptr dmx, memory_c &mem, uint64_t timecode, uint32_t flags);
  virtual void queue_one_audio_frame(real_demuxer_cptr dmx, memory_c &mem, uint64_t timecode, uint32_t flags);
  virtual void deliver_audio_frames(real_demuxer_cptr dmx, uint64_t duration);

  virtual void create_audio_packetizer(real_demuxer_cptr dmx);
  virtual void create_aac_audio_packetizer(real_demuxer_cptr dmx);
  virtual void create_dnet_audio_packetizer(real_demuxer_cptr dmx);
  virtual void create_video_packetizer(real_demuxer_cptr dmx);
};

#endif  // __R_REAL_H
