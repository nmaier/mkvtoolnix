/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  tta_common.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief definitions for the TTA file format
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __TTA_COMMON_H
#define __TTA_COMMON_H

#include "os.h"

/* All integers are little endian. */

#define TTA_FRAME_TIME (double)1.04489795918367346939l

typedef struct __attribute__((__packed__)) {
  char signature[4];            /* TTA1 */
  uint16_t audio_format;        /* 1 for 32 bits per sample, 3 otherwise? */
  uint16_t channels;
  uint16_t bits_per_sample;
  uint32_t sample_rate;
  uint32_t data_length;
  uint32_t crc;
} tta_file_header_t;

#endif // __TTA_COMMON_H
