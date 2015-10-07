/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the Subripper subtitle reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_R_SRT_H
#define MTX_R_SRT_H

#include "common/common_pch.h"

#include "common/mm_io.h"
#include "merge/pr_generic.h"
#include "input/subtitles.h"

class srt_reader_c: public generic_reader_c {
private:
  mm_text_io_cptr m_text_in;
  srt_parser_cptr m_subs;

public:
  srt_reader_c(const track_info_c &ti, const mm_io_cptr &in);
  virtual ~srt_reader_c();

  virtual translatable_string_c get_format_name() const {
    return YT("SRT subtitles");
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t tid);
  virtual int get_progress();
  virtual bool is_simple_subtitle_container() {
    return true;
  }

  static int probe_file(mm_text_io_c *in, uint64_t size);
};

#endif  // MTX_R_SRT_H
