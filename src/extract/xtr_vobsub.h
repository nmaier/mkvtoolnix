/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_XTR_VOBSUB_H
#define MTX_XTR_VOBSUB_H

#include "common/common_pch.h"

#include "extract/xtr_base.h"

class xtr_vobsub_c: public xtr_base_c {
public:
  std::vector<int64_t> m_positions, m_timecodes;
  std::vector<xtr_vobsub_c *> m_slaves;
  memory_cptr m_private_data;
  std::string m_base_name, m_language;
  int m_stream_id;

public:
  xtr_vobsub_c(const std::string &codec_id, int64_t tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *master, KaxTrackEntry &track);
  virtual void handle_frame(xtr_frame_t &f);
  virtual void finish_file();
  virtual void write_idx(mm_io_c &idx, int index);

  virtual const char *get_container_name() {
    return "VobSubs";
  };
};

#endif
