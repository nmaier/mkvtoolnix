/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the MP3 reader module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_MP3_H
#define __R_MP3_H

#include "common/common_pch.h"

#include <stdio.h>

#include "common/mm_io.h"
#include "common/error.h"

#include "output/p_mp3.h"

class mp3_reader_c: public generic_reader_c {
private:
  unsigned char chunk[16384];
  mm_io_c *io;
  int64_t bytes_processed, size;
  mp3_header_t mp3header;

public:
  mp3_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~mp3_reader_c();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("MP2/MP3") : "MP2/MP3";
  }

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t tid);

  virtual int get_progress();

  static int probe_file(mm_io_c *io, uint64_t size, int64_t probe_range, int num_headers = 5);

protected:
  static int find_valid_headers(mm_io_c *io, int64_t probe_range, int num_headers);
};

#endif  // __R_MP3_H
