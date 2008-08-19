/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definitions for the MPEG ES/PS demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_MPEG_H
#define __R_MPEG_H

#include "os.h"

#include "dts_common.h"
#include "mpeg4_common.h"
#include "pr_generic.h"
#include "smart_pointers.h"
#include "M2VParser.h"

class error_c;
class generic_reader_c;
class mm_io_c;
class track_info_c;

class mpeg_es_reader_c: public generic_reader_c {
private:
  mm_io_c *io;
  int64_t bytes_processed, size;

  int version, width, height, dwidth, dheight;
  double frame_rate, aspect_ratio;

public:
  mpeg_es_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~mpeg_es_reader_c();

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual int get_progress();
  virtual void identify();
  virtual void create_packetizer(int64_t id);

  static bool read_frame(M2VParser &parser, mm_io_c &in,
                         int64_t max_size = -1, bool flush_parser = false);

  static int probe_file(mm_io_c *io, int64_t size);
};

struct mpeg_ps_track_t {
  int ptzr;

  char type;                    // 'v' for video, 'a' for audio, 's' for subs
  int id, sort_key;
  uint32_t fourcc;

  int64_t timecode_offset;

  int v_version, v_width, v_height, v_dwidth, v_dheight;
  double v_frame_rate, v_aspect_ratio;
  memory_cptr v_avcc;
  unsigned char *raw_seq_hdr;
  int raw_seq_hdr_size;

  int a_channels, a_sample_rate, a_bits_per_sample, a_bsid;
  dts_header_t dts_header;

  unsigned char *buffer;
  int buffer_usage, buffer_size;

  mpeg_ps_track_t():
    ptzr(-1), type(0), id(0), sort_key(0), fourcc(0), timecode_offset(-1),
    v_version(0), v_width(0), v_height(0), v_dwidth(0), v_dheight(0),
    v_frame_rate(0), v_aspect_ratio(0),
    raw_seq_hdr(NULL), raw_seq_hdr_size(0),
    a_channels(0), a_sample_rate(0), a_bits_per_sample(0), a_bsid(0),
    buffer(NULL), buffer_usage(0), buffer_size(0) {
  };

  void use_buffer(int size) {
    safefree(buffer);
    buffer = (unsigned char *)safemalloc(size);
    buffer_size = size;
    buffer_usage = 0;
  }

  void assert_buffer_size(int size) {
    if (!buffer)
      use_buffer(size);
    else if (size > buffer_size) {
      buffer = (unsigned char *)saferealloc(buffer, size);
      buffer_size = size;
    }
  }

  ~mpeg_ps_track_t() {
    safefree(raw_seq_hdr);
    safefree(buffer);
  }
};

typedef counted_ptr<mpeg_ps_track_t> mpeg_ps_track_ptr;

#define NUM_ES_MAP_ENTRIES 0x40

class mpeg_ps_reader_c: public generic_reader_c {
private:
  mm_io_c *io;
  int64_t bytes_processed, size, duration;

  int id2idx[512];
  bool blacklisted_ids[512];
  uint32_t es_map[NUM_ES_MAP_ENTRIES];
  int version;
  bool file_done;

  vector<mpeg_ps_track_ptr> tracks;

public:
  mpeg_ps_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~mpeg_ps_reader_c();

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual int get_progress();
  virtual void identify();
  virtual void create_packetizer(int64_t id);
  virtual void create_packetizers();
  virtual void add_available_track_ids();

  virtual void found_new_stream(int id);

  virtual bool read_timestamp(int c, int64_t &timestamp);
  virtual bool parse_packet(int id, int64_t &timestamp, int &length,
                            int &full_size, int &aid);
  virtual bool find_next_packet(int &id, int64_t max_file_pos = -1);
  virtual bool find_next_packet_for_id(int id, int64_t max_file_pos = -1);

  virtual void parse_program_stream_map();

  static int probe_file(mm_io_c *io, int64_t size);

private:
  virtual void new_stream_v_mpeg_1_2(int id, unsigned char *buf,
                                     int length, mpeg_ps_track_ptr &track);
  virtual void new_stream_v_avc(int id, unsigned char *buf,
                                int length, mpeg_ps_track_ptr &track);
  virtual void new_stream_a_mpeg(int id, unsigned char *buf,
                                 int length, mpeg_ps_track_ptr &track);
  virtual void new_stream_a_ac3(int id, unsigned char *buf,
                                int length, mpeg_ps_track_ptr &track);
  virtual void new_stream_a_dts(int id, unsigned char *buf,
                                int length, mpeg_ps_track_ptr &track);
  virtual bool resync_stream(uint32_t &header);
  virtual file_status_e finish();
  void sort_tracks();
  void calculate_global_timecode_offset();

  void init_reader();
};

class mpeg_ts_reader_c: public generic_reader_c {
protected:
  static int potential_packet_sizes[];

public:
  static bool probe_file(mm_io_c *io, int64_t size);

public:
  mpeg_ts_reader_c(track_info_c &n_ti): generic_reader_c(n_ti) { };
};

#endif // __R_MPEG_H
