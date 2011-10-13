/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the PGS demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_PGS_H
#define __R_PGS_H

#include "common/common_pch.h"

#include <stdio.h>

#include "common/error.h"
#include "common/mm_io.h"
#include "merge/pr_generic.h"

class pgssup_reader_c: public generic_reader_c {
private:
  mm_io_cptr m_in;
  uint64_t m_bytes_processed, m_file_size;
  bool m_debug;

public:
  pgssup_reader_c(track_info_c &p_ti);
  virtual ~pgssup_reader_c();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("PGSSUP") : "PGSSUP";
  }

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual int get_progress();
  virtual void create_packetizer(int64_t id);
  virtual bool is_simple_subtitle_container() {
    return true;
  }

  static int probe_file(mm_io_c *in, uint64_t size);
};

#endif // __R_PGS_H
