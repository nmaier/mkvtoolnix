/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_XTR_RMFF_H
#define MTX_XTR_RMFF_H

#include "common/common_pch.h"

#include "librmff/librmff.h"

#include "extract/xtr_base.h"

class xtr_rmff_c: public xtr_base_c {
public:
  rmff_file_t *m_file;
  rmff_track_t *m_rmtrack;

public:
  xtr_rmff_c(const std::string &codec_id, int64_t tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *master, KaxTrackEntry &track);
  virtual void handle_frame(memory_cptr &frame, KaxBlockAdditions *additions, int64_t timecode, int64_t duration, int64_t bref, int64_t fref,
                            bool keyframe, bool discardable, bool references_valid);
  virtual void finish_file();
  virtual void headers_done();

  virtual const char *get_container_name() {
    return "RMFF (RealMedia File Format)";
  };
};

#endif
