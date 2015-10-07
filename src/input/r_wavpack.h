/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the TTA demultiplexer module

   Written by Steve Lhomme <steve.lhomme@free.fr>.
*/

#ifndef MTX_R_WAVPACK_H
#define MTX_R_WAVPACK_H

#include "common/common_pch.h"

#include <stdio.h>

#include "common/error.h"
#include "common/mm_io.h"
#include "merge/pr_generic.h"
#include "common/wavpack.h"

class wavpack_reader_c: public generic_reader_c {
private:
  mm_io_cptr m_in_correc;
  wavpack_header_t header, header_correc;
  wavpack_meta_t meta, meta_correc;

public:
  wavpack_reader_c(const track_info_c &ti, const mm_io_cptr &in);
  virtual ~wavpack_reader_c();

  virtual translatable_string_c get_format_name() const {
    return YT("WAVPACK");
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t id);
  virtual bool is_providing_timecodes() const {
    return false;
  }

  static int probe_file(mm_io_c *in, uint64_t size);
};

#endif // MTX_R_WAVPACK_H
