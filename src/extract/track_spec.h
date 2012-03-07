/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   extracts tracks and other items from Matroska files into other files

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __EXTRACT_TRACK_SPEC_H
#define __EXTRACT_TRACK_SPEC_H

#include "common/common_pch.h"

struct track_spec_t {
  enum target_mode_e {
    tm_normal,
    tm_raw,
    tm_full_raw
  };

  int64_t tid, tuid;
  std::string out_name;

  std::string sub_charset;
  bool extract_cuesheet;

  target_mode_e target_mode;
  int extract_blockadd_level;

  bool done;

  track_spec_t();
};

#endif // __EXTRACT_TRACK_SPEC_H
