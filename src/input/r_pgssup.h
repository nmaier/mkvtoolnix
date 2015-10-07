/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the PGS demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_R_PGS_H
#define MTX_R_PGS_H

#include "common/common_pch.h"

#include "common/debugging.h"
#include "common/error.h"
#include "common/mm_io.h"
#include "merge/pr_generic.h"

class pgssup_reader_c: public generic_reader_c {
private:
  debugging_option_c m_debug;

public:
  pgssup_reader_c(const track_info_c &ti, const mm_io_cptr &in);
  virtual ~pgssup_reader_c();

  virtual translatable_string_c get_format_name() const {
    return YT("PGSSUP");
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t id);
  virtual bool is_simple_subtitle_container() {
    return true;
  }

  static int probe_file(mm_io_c *in, uint64_t size);
};

#endif // MTX_R_PGS_H
