/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions for the TTA file format

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_TTA_COMMON_H
#define __MTX_COMMON_TTA_COMMON_H

#include "common/common_pch.h"

/* All integers are little endian. */

#define TTA_FRAME_TIME (double)1.04489795918367346939l

#if defined(COMP_MSC)
#pragma pack(push,1)
#endif
typedef struct PACKED_STRUCTURE {
  char signature[4];            /* TTA1 */
  uint16_t audio_format;        /* 1 for 32 bits per sample, 3 otherwise? */
  uint16_t channels;
  uint16_t bits_per_sample;
  uint32_t sample_rate;
  uint32_t data_length;
  uint32_t crc;
} tta_file_header_t;
#if defined(COMP_MSC)
#pragma pack(pop)
#endif

#endif // __MTX_COMMON_TTA_COMMON_H
