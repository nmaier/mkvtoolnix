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
    \version \$Id: mp3_common.h,v 1.2 2003/02/16 17:04:38 mosu Exp $
    \brief helper functions for MP3 data
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __MP3_COMMON_H
#define __MP3_COMMON_H

extern int mp3_tabsel[2][16];
extern long mp3_freqs[9];

typedef struct {
  int lsf;
  int mpeg25;
  int mode;
  int error_protection;
  int stereo;
  int ssize;
  int bitrate_index;
  int sampling_frequency;
  int padding;
  int framesize;
} mp3_header_t;

int find_mp3_header(char *buf, int size, unsigned long *_header);
void decode_mp3_header(unsigned long header, mp3_header_t *h);

#endif
