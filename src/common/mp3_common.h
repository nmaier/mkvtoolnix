/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  mp3_common.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief helper functions for MP3 data
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __MP3_COMMON_H
#define __MP3_COMMON_H

#include "os.h"

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
  int framesize;
  int samples_per_channel;
  bool is_tag;
} mp3_header_t;

int MTX_DLL_API find_mp3_header(const unsigned char *buf, int size);
int MTX_DLL_API find_consecutive_mp3_headers(const unsigned char *buf,
                                             int size, int num);
void MTX_DLL_API decode_mp3_header(const unsigned char *buf, mp3_header_t *h);

#endif // __MP3_COMMON_H
