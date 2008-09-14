/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   definitions and helper functions for AC3 data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __AC3COMMON_H
#define __AC3COMMON_H

#include "os.h"

#define A52_CHANNEL       0
#define A52_MONO          1
#define A52_STEREO        2
#define A52_3F            3
#define A52_2F1R          4
#define A52_3F1R          5
#define A52_2F2R          6
#define A52_3F2R          7
#define A52_CHANNEL1      8
#define A52_CHANNEL2      9
#define A52_DOLBY        10
#define A52_CHANNEL_MASK 15

#define A52_LFE 16

typedef struct {
  int sample_rate;
  int bit_rate;
  int channels;
  int flags;
  int bytes;
  int bsid;
  int samples;
} ac3_header_t;

int MTX_DLL_API find_ac3_header(const unsigned char *buf, int size, ac3_header_t *ac3_header);
int MTX_DLL_API find_consecutive_ac3_headers(const unsigned char *buf, int size, int num);

#endif // __AC3COMMON_H
