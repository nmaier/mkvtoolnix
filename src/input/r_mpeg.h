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
#include "smart_pointers.h"
#include "pr_generic.h"
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
  int id;
  uint32_t fourcc;

  int64_t timecode_offset;

  int skip_bytes;

  int v_version, v_width, v_height, v_dwidth, v_dheight;
  double v_frame_rate, v_aspect_ratio;
  unsigned char *raw_seq_hdr;
  int raw_seq_hdr_size;

  int a_channels, a_sample_rate, a_bits_per_sample, a_bsid;
  dts_header_t dts_header;

  mpeg_ps_track_t():
    ptzr(-1), type(0), fourcc(0), timecode_offset(-1),
    skip_bytes(0),
    v_version(0), v_width(0), v_height(0), v_dwidth(0), v_dheight(0),
    v_frame_rate(0), v_aspect_ratio(0),
    raw_seq_hdr(NULL), raw_seq_hdr_size(0),
    a_channels(0), a_sample_rate(0), a_bits_per_sample(0), a_bsid(0) {
  };
  ~mpeg_ps_track_t() {
    safefree(raw_seq_hdr);
  }
};

typedef counted_ptr<mpeg_ps_track_t> mpeg_ps_track_ptr;

class mpeg_ps_reader_c: public generic_reader_c {
private:
  mm_io_c *io;
  int64_t bytes_processed, size, duration;

  int id2idx[512];
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
  virtual bool parse_packet(int id, int64_t &timestamp, int &size, int &aid);
  virtual bool find_next_packet(int &id);
  virtual bool find_next_packet_for_id(int id);

  static int probe_file(mm_io_c *io, int64_t size);
};

#endif // __R_MPEG_H
