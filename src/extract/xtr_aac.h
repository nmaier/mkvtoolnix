/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_XTR_AAC_H
#define MTX_XTR_AAC_H

#include "common/common_pch.h"

#include "extract/xtr_base.h"

class xtr_aac_c: public xtr_base_c {
public:
  int m_channels, m_id, m_profile, m_srate_idx;

public:
  xtr_aac_c(const std::string &codec_id, int64_t tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *master, KaxTrackEntry &track);
  virtual void handle_frame(memory_cptr &frame, KaxBlockAdditions *additions, int64_t timecode, int64_t duration, int64_t bref, int64_t fref,
                            bool keyframe, bool discardable, bool references_valid);

  virtual const char *get_container_name() {
    return "raw AAC file with ADTS headers";
  };
};

#endif
