/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper functions for ALAC data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_ALAC_H
#define MTX_COMMON_ALAC_H

#include "common/common_pch.h"

namespace alac {

#if defined(COMP_MSC)
#pragma pack(push,1)
#endif

struct PACKED_STRUCTURE codec_config_t {
  uint32_t frame_length;
  uint8_t  compatible_version;
  uint8_t  bit_depth;
  uint8_t  rice_history_mult;
  uint8_t  rice_initial_history;
  uint8_t  rice_limit;
  uint8_t  num_channels;
  uint16_t max_run;
  uint32_t max_frame_bytes;
  uint32_t avg_bit_rate;
  uint32_t sample_rate;
};

#if defined(COMP_MSC)
#pragma pack(pop)
#endif

}

#endif // MTX_COMMON_ALAC_H
