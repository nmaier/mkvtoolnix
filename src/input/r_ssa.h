/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the SSA/ASS subtitle parser

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_SSA_H
#define __R_SSA_H

#include "common/common_pch.h"

#include "common/mm_io.h"
#include "merge/pr_generic.h"
#include "input/subtitles.h"

class ssa_reader_c: public generic_reader_c {
private:
  ssa_parser_cptr m_subs;

public:
  ssa_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~ssa_reader_c();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("SSA/ASS") : "SSA/ASS";
  }

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t tid);
  virtual int get_progress();
  virtual bool is_simple_subtitle_container() {
    return true;
  }

  static int probe_file(mm_text_io_c *io, uint64_t size);
};

#endif  // __R_SSA_H
