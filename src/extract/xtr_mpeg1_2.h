/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Matt Rice <topquark@sluggy.net>.
*/

#ifndef MTX_XTR_MPEG1_2_H
#define MTX_XTR_MPEG1_2_H

#include "common/common_pch.h"

#include "extract/xtr_base.h"

class xtr_mpeg1_2_video_c: public xtr_base_c {
public:
  memory_cptr m_seq_hdr;

public:
  xtr_mpeg1_2_video_c(const std::string &codec_id, int64_t tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *master, KaxTrackEntry &track);
  virtual void handle_frame(xtr_frame_t &f);
  virtual void handle_codec_state(memory_cptr &codec_state);

  virtual const char *get_container_name() {
    return "MPEG-1/-2 program stream";
  };
};

#endif
