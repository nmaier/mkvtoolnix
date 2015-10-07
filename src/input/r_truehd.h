/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the TrueHD demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_R_TRUEHD_H
#define MTX_R_TRUEHD_H

#include "common/common_pch.h"

#include <stdio.h>

#include "common/error.h"
#include "common/mm_io.h"
#include "merge/pr_generic.h"
#include "common/truehd.h"

class truehd_reader_c: public generic_reader_c {
private:
  memory_cptr m_chunk;
  truehd_frame_cptr m_header;

public:
  truehd_reader_c(const track_info_c &ti, const mm_io_cptr &in);
  virtual ~truehd_reader_c();

  virtual translatable_string_c get_format_name() const {
    return YT("TrueHD");
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t id);
  virtual bool is_providing_timecodes() const {
    return false;
  }

  static int probe_file(mm_io_c *in, uint64_t size);

protected:
  static bool find_valid_headers(mm_io_c &in, int64_t probe_range, int num_headers);
};

#endif // MTX_R_TRUEHD_H
