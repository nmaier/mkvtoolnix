/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the AAC demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_AAC_H
#define __R_AAC_H

#include "common/common_pch.h"

#include <stdio.h>

#include "common/aac.h"
#include "common/error.h"
#include "common/mm_io.h"
#include "merge/pr_generic.h"

class aac_reader_c: public generic_reader_c {
private:
  memory_cptr m_chunk;
  mm_io_cptr m_io;
  int64_t m_bytes_processed, m_size;
  bool m_emphasis_present, m_sbr_status_set;
  aac_header_t m_aacheader;

public:
  aac_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~aac_reader_c();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("AAC") : "AAC";
  }

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual int get_progress();
  virtual void create_packetizer(int64_t id);

  static int probe_file(mm_io_c *io, uint64_t size, int64_t probe_range, int num_headers);

protected:
  virtual void guess_adts_version();
  static int find_valid_headers(mm_io_c *io, int64_t probe_range, int num_headers);
};

#endif // __R_AAC_H
