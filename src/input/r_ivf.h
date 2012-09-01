/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the IVF demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_IVF_H
#define __R_IVF_H

#include "common/common_pch.h"

#include "common/mm_io.h"
#include "merge/pr_generic.h"

class ivf_reader_c: public generic_reader_c {
private:
  uint16_t m_width, m_height;
  uint64_t m_frame_rate_num, m_frame_rate_den;

public:
  ivf_reader_c(const track_info_c &ti, const mm_io_cptr &in);
  virtual ~ivf_reader_c();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("IVF (VP8)") : "IVF (VP8)";
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t id);

  static int probe_file(mm_io_c *in, uint64_t size);
};

#endif // __R_IVF_H
