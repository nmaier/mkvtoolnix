/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the MP3 reader module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_R_MP3_H
#define MTX_R_MP3_H

#include "common/common_pch.h"

#include <stdio.h>

#include "common/mm_io.h"
#include "common/error.h"

#include "output/p_mp3.h"

class mp3_reader_c: public generic_reader_c {
private:
  memory_cptr m_chunk;
  mp3_header_t m_mp3header;

public:
  mp3_reader_c(const track_info_c &ti, const mm_io_cptr &in);
  virtual ~mp3_reader_c();

  virtual translatable_string_c get_format_name() const {
    return YT("MP2/MP3");
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t tid);

  static int probe_file(mm_io_c *in, uint64_t size, int64_t probe_range, int num_headers = 5, bool require_zero_offset = false);

protected:
  static int find_valid_headers(mm_io_c &in, int64_t probe_range, int num_headers);
};

#endif  // MTX_R_MP3_H
