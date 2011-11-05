/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the VC1 ES demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_VC1_H
#define __R_VC1_H

#include "common/common_pch.h"

#include "merge/pr_generic.h"
#include "common/vc1.h"

class vc1_es_reader_c: public generic_reader_c {
private:
  counted_ptr<mm_io_c> m_io;
  int64_t m_bytes_processed, m_size;

  memory_cptr m_raw_seqhdr;
  int m_width, m_height;

  memory_cptr m_buffer;

  vc1::sequence_header_t m_seqhdr;

public:
  vc1_es_reader_c(track_info_c &n_ti) throw (error_c);

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("VC1") : "VC1";
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual int get_progress();
  virtual void identify();
  virtual void create_packetizer(int64_t id);

  static int probe_file(mm_io_c *io, uint64_t size);
};

#endif // __R_VC1_H
