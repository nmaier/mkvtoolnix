/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the AAC demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_R_AAC_H
#define MTX_R_AAC_H

#include "common/common_pch.h"

#include <stdio.h>

#include "common/aac.h"
#include "common/error.h"
#include "common/mm_io.h"
#include "merge/pr_generic.h"

class aac_reader_c: public generic_reader_c {
private:
  memory_cptr m_chunk;
  bool m_emphasis_present, m_sbr_status_set;
  aac_header_c m_aacheader;

public:
  aac_reader_c(const track_info_c &ti, const mm_io_cptr &in);
  virtual ~aac_reader_c();

  virtual translatable_string_c get_format_name() const {
    return YT("AAC");
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t id);
  virtual bool is_providing_timecodes() const {
    return false;
  }

  static int probe_file(mm_io_c *in, uint64_t size, int64_t probe_range, int num_headers, bool require_zero_offset = false);

protected:
  virtual void guess_adts_version();
  static int find_valid_headers(mm_io_c &in, int64_t probe_range, int num_headers);
};

#endif // MTX_R_AAC_H
