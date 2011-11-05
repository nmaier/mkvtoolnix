/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the MPEG ES/PS demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_MPEG_ES_H
#define __R_MPEG_ES_H

#include "common/common_pch.h"

#include "common/dts.h"
#include "common/mpeg1_2.h"
#include "merge/pr_generic.h"
#include "common/smart_pointers.h"
#include "mpegparser/M2VParser.h"

class error_c;
class generic_reader_c;
class mm_io_c;
class track_info_c;

class mpeg_es_reader_c: public generic_reader_c {
private:
  mm_io_c *io;
  int64_t bytes_processed, size;

  int version, interlaced, width, height, dwidth, dheight;
  double frame_rate, aspect_ratio;

public:
  mpeg_es_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~mpeg_es_reader_c();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("MPEG video elementary stream") : "MPEG video elementary stream";
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual int get_progress();
  virtual void identify();
  virtual void create_packetizer(int64_t id);

  static bool read_frame(M2VParser &parser, mm_io_c &in, int64_t max_size = -1, bool flush_parser = false);

  static int probe_file(mm_io_c *io, uint64_t size);
};

#endif // __R_MPEG_ES_H
