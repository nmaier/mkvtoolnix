/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions and helper functions for AC3 data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_AC3COMMON_H
#define __MTX_COMMON_AC3COMMON_H

#include "common/os.h"

#define AC3_SYNC_WORD           0x0b77

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

struct ac3_header_t {
  unsigned int sample_rate;
  unsigned int bit_rate;
  unsigned int channels;
  unsigned int flags;
  unsigned int bytes;
  unsigned int bsid;
  unsigned int samples;

  unsigned int frame_type;
  unsigned int sub_stream_id;

  bool has_dependent_frames;

  ac3_header_t();
};

int find_ac3_header(const unsigned char *buf, size_t size, ac3_header_t *ac3_header, bool look_for_second_header);
int find_consecutive_ac3_headers(const unsigned char *buf, size_t size, unsigned int num);
bool parse_ac3_header(const unsigned char *buf, ac3_header_t &header);
bool verify_ac3_checksum(const unsigned char *buf, size_t size);

#endif // __MTX_COMMON_AC3COMMON_H
