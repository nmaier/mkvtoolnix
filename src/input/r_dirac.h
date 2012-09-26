/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the Dirac demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_DIRAC_H
#define __R_DIRAC_H

#include "common/common_pch.h"

#include "merge/pr_generic.h"
#include "common/dirac.h"

class dirac_es_reader_c: public generic_reader_c {
private:
  dirac::sequence_header_t m_seqhdr;
  memory_cptr m_raw_seqhdr;

  memory_cptr m_buffer;

public:
  dirac_es_reader_c(const track_info_c &ti, const mm_io_cptr &in);

  virtual const std::string get_format_name(bool translate = true) const {
    return translate ? Y("Dirac") : "Dirac";
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t id);

  static int probe_file(mm_io_c *in, uint64_t size);
};

#endif // __R_DIRAC_H
