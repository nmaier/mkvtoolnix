/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definitions for the Quicktime & MP4 reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
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
#include "smart_pointers.h"

struct qt_durmap_t {
  uint32_t number;
  uint32_t duration;

  qt_durmap_t():
    number(0), duration(0) {}
};

struct qt_chunk_t {
  uint32_t samples;
  uint32_t size;
  uint32_t desc;
  uint64_t pos;

  qt_chunk_t():
    samples(0), size(0), desc(0), pos(0) {}
};

struct qt_chunkmap_t {
  uint32_t first_chunk;
  uint32_t samples_per_chunk;
  uint32_t sample_description_id;

  qt_chunkmap_t():
    first_chunk(0), samples_per_chunk(0), sample_description_id(0) {}
};

struct qt_editlist_t {
  uint32_t duration;
  uint32_t pos;
  uint32_t speed;
  uint32_t frames;
  uint32_t start_sample;
  uint32_t start_frame;
  uint32_t pts_offset;

  qt_editlist_t():
    duration(0), pos(0), speed(0), frames(0),
    start_sample(0), start_frame(0), pts_offset(0) {}
};

struct qt_sample_t {
  uint32_t pts;
  uint32_t size;
  uint64_t pos;

  qt_sample_t():
    pts(0), size(0), pos(0) {}
};

struct qt_frame_offset_t {
  uint32_t count;
  uint32_t offset;

  qt_frame_offset_t():
    count(0), offset(0) {}
};

struct qtmp4_demuxer_t {
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

  vector<qt_sample_t> sample_table;
  vector<qt_chunk_t> chunk_table;
  vector<qt_chunkmap_t> chunkmap_table;
  vector<qt_durmap_t> durmap_table;
  vector<uint32_t> keyframe_table;
  vector<qt_editlist_t> editlist_table;
  vector<qt_frame_offset_t> raw_frame_offset_table;
  vector<int32_t> frame_offset_table;

  esds_t esds;
  bool esds_parsed;

  unsigned char *v_stsd;
  uint32_t v_stsd_size;
  uint32_t v_width, v_height, v_bitdepth;
  int64_t v_dts_offset;
  deque<int64_t> references;
  bool avc_use_bframes;
  int64_t max_timecode;
  uint32_t a_channels, a_bitdepth;
  float a_samplerate;
  sound_v1_stsd_atom_t a_stsd;

  unsigned char *priv;
  uint32_t priv_size;

  bool warning_printed;

  int ptzr;

  qtmp4_demuxer_t():
    ok(false), type('?'), id(0), pos(0),
    timescale(1), global_duration(0), avg_duration(0), sample_size(0),
    duration(0),
    esds_parsed(false),
    v_stsd(NULL), v_stsd_size(0),
    v_width(0), v_height(0), v_bitdepth(0), v_dts_offset(0),
    avc_use_bframes(false),
    max_timecode(0),
    a_channels(0), a_bitdepth(0), a_samplerate(0.0),
    priv(NULL), priv_size(0),
    warning_printed(false),
    ptzr(-1) {

    memset(fourcc, 0, 4);
    memset(&esds, 0, sizeof(esds_t));
    memset(&a_stsd, 0, sizeof(sound_v1_stsd_atom_t));
  }

  ~qtmp4_demuxer_t() {
    safefree(v_stsd);
    safefree(priv);
    safefree(esds.decoder_config);
    safefree(esds.sl_config);
  }

  double calculate_fps();
};
typedef counted_ptr<qtmp4_demuxer_t> qtmp4_demuxer_ptr;

struct qt_atom_t {
  uint32_t fourcc;
  uint64_t size;
  uint64_t pos;
  uint32_t hsize;

  qt_atom_t():
    fourcc(0), size(0), pos(0), hsize(0) {}

  qt_atom_t to_parent() {
    qt_atom_t parent;

    parent.fourcc = fourcc;
    parent.size = size - hsize;
    parent.pos = pos + hsize;
    return parent;
  }
};

class qtmp4_reader_c: public generic_reader_c {
private:
  mm_io_c *io;
  vector<qtmp4_demuxer_ptr> demuxers;
  int64_t file_size, mdat_pos, mdat_size;
  uint32_t compression_algorithm;
  int main_dmx;

public:
  qtmp4_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~qtmp4_reader_c();

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual int get_progress();
  virtual void identify();
  virtual void create_packetizers();
  virtual void create_packetizer(int64_t tid);
  virtual void add_available_track_ids();

  static int probe_file(mm_io_c *in, int64_t size);

protected:
  virtual void parse_headers();
  virtual qt_atom_t read_atom();
  virtual void parse_header_priv_atoms(qtmp4_demuxer_ptr &dmx,
                                       unsigned char *mem, int size,
                                       int level);
  virtual bool parse_esds_atom(mm_mem_io_c &memio, qtmp4_demuxer_ptr &dmx,
                               int level);
  virtual uint32_t read_esds_descr_len(mm_mem_io_c &memio);
  virtual void flush_packetizers();

  virtual void handle_video_with_bframes(qtmp4_demuxer_ptr &dmx,
                                         int64_t timecode, int64_t duration,
                                         bool is_keyframe,
                                         memory_cptr mem);

  virtual void handle_cmov_atom(qt_atom_t parent, int level);
  virtual void handle_cmvd_atom(qt_atom_t parent, int level);
  virtual void handle_ctts_atom(qtmp4_demuxer_ptr &new_dmx, qt_atom_t parent,
                                int level);
  virtual void handle_dcom_atom(qt_atom_t parent, int level);
  virtual void handle_hdlr_atom(qtmp4_demuxer_ptr &new_dmx, qt_atom_t parent,
                                int level);
  virtual void handle_mdhd_atom(qtmp4_demuxer_ptr &new_dmx, qt_atom_t parent,
                                int level);
  virtual void handle_mdia_atom(qtmp4_demuxer_ptr &new_dmx, qt_atom_t parent,
                                int level);
  virtual void handle_minf_atom(qtmp4_demuxer_ptr &new_dmx, qt_atom_t parent,
                                int level);
  virtual void handle_moov_atom(qt_atom_t parent, int level);
  virtual void handle_mvhd_atom(qt_atom_t parent, int level);
  virtual void handle_stbl_atom(qtmp4_demuxer_ptr &new_dmx, qt_atom_t parent,
                                int level);
  virtual void handle_stco_atom(qtmp4_demuxer_ptr &new_dmx, qt_atom_t parent,
                                int level);
  virtual void handle_stsc_atom(qtmp4_demuxer_ptr &new_dmx, qt_atom_t parent,
                                int level);
  virtual void handle_stsd_atom(qtmp4_demuxer_ptr &new_dmx, qt_atom_t parent,
                                int level);
  virtual void handle_stss_atom(qtmp4_demuxer_ptr &new_dmx, qt_atom_t parent,
                                int level);
  virtual void handle_stsz_atom(qtmp4_demuxer_ptr &new_dmx, qt_atom_t parent,
                                int level);
  virtual void handle_sttd_atom(qtmp4_demuxer_ptr &new_dmx, qt_atom_t parent,
                                int level);
  virtual void handle_stts_atom(qtmp4_demuxer_ptr &new_dmx, qt_atom_t parent,
                                int level);
  virtual void handle_tkhd_atom(qtmp4_demuxer_ptr &new_dmx, qt_atom_t parent,
                                int level);
  virtual void handle_trak_atom(qtmp4_demuxer_ptr &new_dmx, qt_atom_t parent,
                                int level);
};

#endif  // __R_QTMP4_H
