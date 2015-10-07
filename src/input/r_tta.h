/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the TTA demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_R_TTA_H
#define MTX_R_TTA_H

#include "common/common_pch.h"

#include <stdio.h>

#include "common/error.h"
#include "common/mm_io.h"
#include "merge/pr_generic.h"
#include "common/tta.h"

class tta_reader_c: public generic_reader_c {
private:
  memory_cptr m_chunk;
  std::vector<uint32_t> seek_points;
  unsigned int pos;
  tta_file_header_t header;

public:
  tta_reader_c(const track_info_c &ti, const mm_io_cptr &in);
  virtual ~tta_reader_c();

  virtual translatable_string_c get_format_name() const {
    return YT("TTA");
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t id);

  static int probe_file(mm_io_c *in, uint64_t size);
};

#endif // MTX_R_TTA_H
