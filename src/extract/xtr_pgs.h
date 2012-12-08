/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Extraction of Blu-Ray subtitles.

   Written by Moritz Bunkus and Mike Chen.
*/

#ifndef MTX_XTR_PGS_H
#define MTX_XTR_PGS_H

#include "common/common_pch.h"

#include "extract/xtr_base.h"

class xtr_pgs_c: public xtr_base_c {

public:
  xtr_pgs_c(const std::string &codec_id, int64_t tid, track_spec_t &tspec);

  virtual void handle_frame(memory_cptr &frame, KaxBlockAdditions *additions, int64_t timecode, int64_t duration, int64_t bref, int64_t fref,
                            bool keyframe, bool discardable, bool references_valid);

  virtual const char *get_container_name() {
    return "SUP";
  }
};

#endif
