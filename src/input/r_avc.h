/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the AVC/h.264 ES demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_AVC_H
#define __R_AVC_H

#include "common/common_pch.h"

#include "common/mpeg4_p10.h"
#include "merge/pr_generic.h"

class avc_es_reader_c: public generic_reader_c {
private:
  counted_ptr<mm_io_c> m_io;
  int64_t m_bytes_processed, m_size;

  memory_cptr m_avcc;
  int m_width, m_height;

  memory_cptr m_buffer;

public:
  avc_es_reader_c(track_info_c &n_ti);

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("AVC/h.264") : "AVC/h.264";
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual int get_progress();
  virtual void identify();
  virtual void create_packetizer(int64_t id);

  static int probe_file(mm_io_c *io, uint64_t size);
};

#endif // __R_AVC_H
