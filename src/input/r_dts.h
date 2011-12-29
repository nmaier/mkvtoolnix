/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the DTS demultiplexer module

   Written by Peter Niemayer <niemayer@isg.de>.
   Modified by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_DTS_H
#define __R_DTS_H

#include "common/common_pch.h"

#include <stdio.h>

#include "common/dts.h"
#include "common/error.h"
#include "common/mm_io.h"
#include "merge/pr_generic.h"

class dts_reader_c: public generic_reader_c {
private:
  memory_cptr m_af_buf;
  unsigned short *m_buf[2];
  unsigned int m_cur_buf;
  dts_header_t m_dtsheader;
  bool m_dts14_to_16, m_swap_bytes, m_debug;

public:
  dts_reader_c(const track_info_c &ti, const mm_io_cptr &in);
  virtual ~dts_reader_c();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("DTS") : "DTS";
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t id);

  static int probe_file(mm_io_c *in, uint64_t size, bool strict_mode = false);

protected:
  virtual int decode_buffer(size_t length);
};

#endif // __R_DTS_H
