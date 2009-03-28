/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions and helper functions for AC3 data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __AC3COMMON_H
#define __AC3COMMON_H

#include "os.h"

#define AC3_CHANNEL                  0
#define AC3_MONO                     1
#define AC3_STEREO                   2
#define AC3_3F                       3
#define AC3_2F1R                     4
#define AC3_3F1R                     5
#define AC3_2F2R                     6
#define AC3_3F2R                     7
#define AC3_CHANNEL1                 8
#define AC3_CHANNEL2                 9
#define AC3_DOLBY                   10
#define AC3_CHANNEL_MASK            15

#define AC3_LFE                     16

#define EAC3_FRAME_TYPE_INDEPENDENT  0
#define EAC3_FRAME_TYPE_DEPENDENT    1
#define EAC3_FRAME_TYPE_AC3_CONVERT  2
#define EAC3_FRAME_TYPE_RESERVED     3

typedef struct {
  int sample_rate;
  int bit_rate;
  int channels;
  int flags;
  int bytes;
  int bsid;
  int samples;

  int frame_type;
  int sub_stream_id;

  bool has_dependent_frames;
} ac3_header_t;

int MTX_DLL_API find_ac3_header(const unsigned char *buf, int size, ac3_header_t *ac3_header, bool look_for_second_header);
int MTX_DLL_API find_consecutive_ac3_headers(const unsigned char *buf, int size, int num);
bool MTX_DLL_API parse_ac3_header(const unsigned char *buf, ac3_header_t &header);

#endif // __AC3COMMON_H
