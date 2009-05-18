/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Steve Lhomme <steve.lhomme@free.fr>.
*/

#ifndef __XTR_CPIC_H
#define __XTR_CPIC_H

#include "common/os.h"

#include "extract/xtr_base.h"

class xtr_cpic_c: public xtr_base_c {
public:
  std::string m_file_name_root;
  int m_frame_counter;

public:
  xtr_cpic_c(const std::string &codec_id, int64_t tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *master, KaxTrackEntry &track);
  virtual void handle_frame(memory_cptr &frame, KaxBlockAdditions *additions, int64_t timecode, int64_t duration, int64_t bref, int64_t fref,
                            bool keyframe, bool discardable, bool references_valid);

  virtual const char *get_container_name() {
    return "CorePicture/CorePanorama";
  };
};

#endif
