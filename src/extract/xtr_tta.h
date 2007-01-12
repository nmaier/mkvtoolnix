/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __XTR_TTA_H
#define __XTR_TTA_H

#include "os.h"

#include "xtr_base.h"

class xtr_tta_c: public xtr_base_c {
public:
  vector<int64_t> frame_sizes;
  int64_t last_duration;
  int bps, channels, sfreq;
  string temp_file_name;

  static const double tta_frame_time;

public:
  xtr_tta_c(const string &_codec_id, int64_t _tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *_master, KaxTrackEntry &track);
  virtual void handle_frame(memory_cptr &frame, KaxBlockAdditions *additions,
                            int64_t timecode, int64_t duration, int64_t bref,
                            int64_t fref, bool keyframe, bool discardable,
                            bool references_valid);
  virtual void finish_file();

  virtual const char *get_container_name() {
    return "TTA (TrueAudio)";
  };
};

#endif
