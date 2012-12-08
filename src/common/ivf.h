/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions and helper functions for IVF data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_IVF_COMMON_H
#define MTX_COMMON_IVF_COMMON_H

#include "common/common_pch.h"

/* All integers are little endian. */

namespace ivf {

#if defined(COMP_MSC)
#pragma pack(push,1)
#endif
  struct PACKED_STRUCTURE file_header_t {
    unsigned char file_magic[4];  // "DKIF"
    uint16_t      version;
    uint16_t      header_size;
    unsigned char fourcc[4];      // "VP80"
    uint16_t      width;
    uint16_t      height;
    uint32_t      frame_rate_num;
    uint32_t      frame_rate_den;
    uint32_t      frame_count;
    uint32_t      unused;

    file_header_t();
  };

  struct PACKED_STRUCTURE frame_header_t {
    uint32_t frame_size;
    uint64_t timestamp;

    frame_header_t();
  };

  bool is_keyframe(const memory_cptr &buffer);

#if defined(COMP_MSC)
#pragma pack(pop)
#endif
};

#endif // MTX_COMMON_IVF_COMMON_H
