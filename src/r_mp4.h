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

typedef struct {
  uint32_t number, duration;
} qt_sample_duration_t;

typedef struct {
  uint32_t first_chunk, samples_per_chunk, description_id;
} qt_sample_to_chunk_t;

typedef struct {
  char type;
  uint32_t id;
  char fourcc[4];

  uint32_t timescale, duration, sample_size;

  uint32_t *sync_table;
  uint32_t sync_table_len;
  qt_sample_duration_t *duration_table;
  uint32_t duration_table_len;
  qt_sample_to_chunk_t *sample_to_chunk_table;
  uint32_t sample_to_chunk_table_len;
  uint32_t *sample_size_table;
  uint32_t sample_size_table_len;
  uint64_t *chunk_offset_table;
  uint32_t chunk_offset_table_len;

  uint32_t v_width, v_height, v_bitdepth;
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
