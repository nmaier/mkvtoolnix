/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  r_mp4.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief class definitions for the Quicktime & MP4 reader
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __R_MP4_H
#define __R_MP4_H

#include "os.h"

#include <stdio.h>

#include <vector>

#include "common.h"
#include "mm_io.h"
#include "p_video.h"
#include "qtmp4_atoms.h"

typedef struct {
  uint32_t number;
  uint32_t duration;
} qt_durmap_t;

typedef struct {
  uint32_t samples;
  uint32_t size;
  uint32_t desc;
  uint64_t pos;
} qt_chunk_t;

typedef struct {
  uint32_t first_chunk;
  uint32_t samples_per_chunk;
  uint32_t sample_description_id;
} qt_chunkmap_t;

typedef struct {
  uint32_t duration;
  uint32_t pos;
  uint32_t speed;
  uint32_t frames;
  uint32_t start_sample;
  uint32_t start_frame;
  uint32_t pts_offset;
} qt_editlist_t;

typedef struct {
  uint32_t pts;
  uint32_t size;
  uint64_t pos;
} qt_sample_t;

typedef struct {
  char type;
  uint32_t id;
  char fourcc[4];
  uint32_t pos;

  uint32_t timescale;
  uint32_t duration;
  uint32_t avg_duration;
  uint32_t sample_size;

  qt_sample_t *sample_table;
  uint32_t sample_table_len;
  qt_chunk_t *chunk_table;
  uint32_t chunk_table_len;
  qt_chunkmap_t *chunkmap_table;
  uint32_t chunkmap_table_len;
  qt_durmap_t *durmap_table;
  uint32_t durmap_table_len;
  uint32_t *keyframe_table;
  uint32_t keyframe_table_len;
  qt_editlist_t *editlist_table;
  uint32_t editlist_table_len;

  uint32_t v_width, v_height, v_bitdepth;
  qt_image_description_t *v_desc;
  uint32_t v_desc_size;
  uint32_t a_channels, a_bitdepth;
  float a_samplerate;

  generic_packetizer_c *packetizer;
} qtmp4_demuxer_t;

class qtmp4_reader_c: public generic_reader_c {
private:
  mm_io_c *io;
  vector<qtmp4_demuxer_t *> demuxers;
  int64_t file_size;
  bool done;
  qtmp4_demuxer_t *new_dmx;
  uint32_t compression_algorithm;

public:
  qtmp4_reader_c(track_info_t *nti) throw (error_c);
  virtual ~qtmp4_reader_c();

  virtual int read();
  virtual packet_t *get_packet();
  virtual int display_priority();
  virtual void display_progress();
  virtual void set_headers();
  virtual void identify();

  static int probe_file(mm_io_c *in, int64_t size);

protected:
  virtual void parse_headers();
  virtual void create_packetizers();
  virtual void handle_header_atoms(uint32_t parent, int64_t parent_size,
                                   uint64_t parent_pos, int level);
  virtual void read_atom(uint32_t &atom, uint64_t &size, uint64_t &pos,
                         uint32_t &hsize);
  virtual void free_demuxer(qtmp4_demuxer_t *dmx);
//   virtual real_demuxer_t *find_demuxer(int id);
//   virtual int finish();
};

#endif  // __R_MP4_H
