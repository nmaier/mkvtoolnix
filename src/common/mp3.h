/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper functions for MP3 data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_MP3_COMMON_H
#define MTX_COMMON_MP3_COMMON_H

#include "common/common_pch.h"

typedef struct {
  int version;
  int layer;
  int protection;
  int bitrate_index, bitrate;
  int sampling_frequency;
  int padding, is_private;
  int channel_mode, channels;
  int mode_extension;
  int copyright, original, emphasis;
  size_t framesize;
  int samples_per_channel;
  bool is_tag;
} mp3_header_t;

int find_mp3_header(const unsigned char *buf, int size);
int find_consecutive_mp3_headers(const unsigned char *buf, int size, int num, mp3_header_t *header_found = nullptr);

bool decode_mp3_header(const unsigned char *buf, mp3_header_t *h);

#endif // MTX_COMMON_MP3_COMMON_H
