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
 * class definitions for the Quicktime & MP4 reader
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#ifndef __R_QTMP4_H
#define __R_QTMP4_H

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
  bool ok;

  char type;
  uint32_t id;
  char fourcc[4];
  uint32_t pos;

  uint32_t timescale;
  uint32_t global_duration;
  uint32_t avg_duration;
  uint32_t sample_size;

  uint32_t duration;

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

  esds_t esds;
  bool esds_parsed;

  unsigned char *v_stsd;
  uint32_t v_stsd_size;
  uint32_t v_width, v_height, v_bitdepth;
  uint32_t a_channels, a_bitdepth;
  float a_samplerate;
  sound_v1_stsd_atom_t a_stsd;

  unsigned char *priv;
  uint32_t priv_size;

  bool warning_printed;

  int ptzr;
} qtmp4_demuxer_t;

class qtmp4_reader_c: public generic_reader_c {
private:
  mm_io_c *io;
  vector<qtmp4_demuxer_t *> demuxers;
  int64_t file_size, mdat_pos, mdat_size;
  qtmp4_demuxer_t *new_dmx;
  uint32_t compression_algorithm;
  int main_dmx;

public:
  qtmp4_reader_c(track_info_c *nti) throw (error_c);
  virtual ~qtmp4_reader_c();

  virtual int read(generic_packetizer_c *ptzr, bool force = false);
  virtual int display_priority();
  virtual void display_progress(bool final = false);
  virtual void identify();
  virtual void create_packetizers();
  virtual void create_packetizer(int64_t tid);
  virtual void add_available_track_ids();

  static int probe_file(mm_io_c *in, int64_t size);

protected:
  virtual void parse_headers();
  virtual void handle_header_atoms(uint32_t parent, int64_t parent_size,
                                   uint64_t parent_pos, int level);
  virtual void read_atom(uint32_t &atom, uint64_t &size, uint64_t &pos,
                         uint32_t &hsize);
  virtual void free_demuxer(qtmp4_demuxer_t *dmx);
  virtual void parse_header_priv_atoms(qtmp4_demuxer_t *dmx,
                                       unsigned char *mem, int size,
                                       int level);
  virtual bool parse_esds_atom(mm_mem_io_c &memio, qtmp4_demuxer_t *dmx,
                               int level);
  virtual uint32_t read_esds_descr_len(mm_mem_io_c &memio);
  virtual void flush_packetizers();
};

#endif  // __R_QTMP4_H
