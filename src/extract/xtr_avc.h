/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Matt Rice <topquark@sluggy.net>.
*/

#ifndef MTX_XTR_AVC_H
#define MTX_XTR_AVC_H

#include "common/common_pch.h"

#include "extract/xtr_base.h"

class xtr_avc_c: public xtr_base_c {
private:
  int m_nal_size_size;

public:
  xtr_avc_c(const std::string &codec_id, int64_t tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *master, KaxTrackEntry &track);
  virtual void handle_frame(memory_cptr &frame, KaxBlockAdditions *additions, int64_t timecode, int64_t duration, int64_t bref, int64_t fref,
                            bool keyframe, bool discardable, bool references_valid);
  bool write_nal(const binary *data, size_t &pos, size_t data_size, size_t nal_size_size);

  virtual const char *get_container_name() {
    return "AVC/h.264 elementary stream";
  };
};

#endif
