/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_XTR_WAV_H
#define MTX_XTR_WAV_H

#include "common/common_pch.h"

#include "extract/xtr_base.h"

class xtr_wav_c: public xtr_base_c {
private:
  wave_header m_wh;

public:
  xtr_wav_c(const std::string &codec_id, int64_t tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *master, KaxTrackEntry &track);
  virtual void finish_file();

  virtual const char *get_container_name() {
    return "WAV";
  };
};

class xtr_wavpack4_c: public xtr_base_c {
private:
  uint32_t m_number_of_samples;
  int m_extract_blockadd_level;
  binary m_version[2];
  mm_io_cptr m_corr_out;
  int m_channels;

public:
  xtr_wavpack4_c(const std::string &codec_id, int64_t tid, track_spec_t &tspec);

  virtual void create_file(xtr_base_c *master, KaxTrackEntry &track);
  virtual void handle_frame(memory_cptr &frame, KaxBlockAdditions *additions, int64_t timecode, int64_t duration, int64_t bref, int64_t fref,
                            bool keyframe, bool discardable, bool references_valid);
  virtual void finish_file();

  virtual const char *get_container_name() {
    return "WAVPACK";
  };
};

#endif
