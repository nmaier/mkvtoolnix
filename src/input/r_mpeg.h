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

#include "smart_pointers.h"
#include "pr_generic.h"
#include "M2VParser.h"

class error_c;
class generic_reader_c;
class mm_io_c;
class track_info_c;

class mpeg_es_reader_c: public generic_reader_c {
private:
  mm_io_c *mm_io;
  int64_t bytes_processed, size;

  int version, width, height;
  double frame_rate, aspect_ratio;

public:
  mpeg_es_reader_c(track_info_c *nti) throw (error_c);
  virtual ~mpeg_es_reader_c();

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual int get_progress();
  virtual void identify();
  virtual void create_packetizer(int64_t id);

  static bool read_frame(M2VParser &parser, mm_io_c &in,
                         int64_t max_size = -1);

  static int probe_file(mm_io_c *mm_io, int64_t size);
};

struct mpeg_ps_track_t {
  char type;                    // 'v' for video, 'a' for audio, 's' for subs
  int id;

  int v_version, v_width, v_height;
  double v_frame_rate, v_aspect_ratio;
  M2VParser *m2v_parser;

  int a_channels, a_sample_rate, a_bits_per_sample;

  mpeg_ps_track_t():
    type(0), id(0), v_version(0), v_width(0), v_height(0),
    v_frame_rate(0), v_aspect_ratio(0), m2v_parser(NULL),
    a_channels(0), a_sample_rate(0), a_bits_per_sample(0) {
  };
  ~mpeg_ps_track_t() {
    delete m2v_parser;
  }
};

typedef counted_ptr<mpeg_ps_track_t> mpeg_ps_track_ptr;

class mpeg_ps_reader_c: public generic_reader_c {
private:
  mm_io_c *mm_io;
  int64_t bytes_processed, size, duration;

  int version;

  vector<mpeg_ps_track_ptr> tracks;

public:
  mpeg_ps_reader_c(track_info_c *nti) throw (error_c);
  virtual ~mpeg_ps_reader_c();

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual int get_progress();
  virtual void identify();
  virtual void create_packetizer(int64_t id);

  static int probe_file(mm_io_c *mm_io, int64_t size);
};

#endif // __R_MPEG_H
