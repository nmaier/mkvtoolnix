/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __XTR_IVF_H
#define __XTR_IVF_H

#include "common/common_pch.h"

#include "common/ivf.h"
#include "extract/xtr_base.h"

class xtr_ivf_c: public xtr_base_c {
public:
  uint64_t m_frame_rate_num, m_frame_rate_den;
  uint32_t m_frame_count;
  ivf::file_header_t m_file_header;

public:
  xtr_ivf_c(const std::string &codec_id, int64_t tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *master, KaxTrackEntry &track);
  virtual void handle_frame(memory_cptr &frame, KaxBlockAdditions *additions, int64_t timecode, int64_t duration, int64_t bref, int64_t fref,
                            bool keyframe, bool discardable, bool references_valid);
  virtual void finish_file();

  virtual const char *get_container_name() {
    return "IVF";
  };
};

#endif
