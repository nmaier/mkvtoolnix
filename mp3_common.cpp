/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  mp3_common.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: mp3_common.cpp,v 1.6 2003/05/17 20:59:21 mosu Exp $
    \brief helper functions for MP3 data
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include "mp3_common.h"
#include "common.h"

int mp3_tabsel[2][16] =
  {{0, 32, 40, 48, 56, 64, 80, 96, 112, 128, 160, 192, 224, 256, 320},
   {0, 8, 16, 24, 32, 40, 48, 56, 64, 80, 96, 112, 128, 144, 160}};

long mp3_freqs[9] =
  {44100, 48000, 32000, 22050, 24000, 16000, 11025, 12000, 8000};

int find_mp3_header(unsigned char *buf, int size, unsigned long *_header) {
  int i, pos;
  unsigned long header;
  
  if (size < 4)
    return -1;
  
  for (pos = 0; pos <= (size - 4); pos++) {
    for (i = 0, header = 0; i < 4; i++) {
      header <<= 8;
      header |= buf[i + pos];
    }

    if ((header == FOURCC('R', 'I', 'F', 'F')) ||
        ((header & 0xffffff00) == FOURCC('I', 'D', '3', ' ')))
      continue;

    if ((header & 0xffe00000) != 0xffe00000)
      continue;
    if (!((header >> 17) & 3))
      continue;
    if (((header >> 12) & 0xf) == 0xf)
      continue;
    if (!((header >> 12) & 0xf))
      continue;
    if (((header >> 10) & 0x3) == 0x3)
      continue;
    if ((((header >> 19) & 1) == 1) && (((header >> 17) & 3) == 3) &&
        (((header >> 16) & 1) == 1))
      continue;
    if ((header & 0xffff0000) == 0xfffe0000)
      continue;
    *_header = header;
    return pos;
  }
  return -1;
}

void decode_mp3_header(unsigned long header, mp3_header_t *h) {
  if (header & (1 << 20)) {
    h->lsf = (header & (1 << 19)) ? 0 : 1;
    h->mpeg25 = 0;
  } else {
    h->lsf = 1;
    h->mpeg25 = 1;
  }
  h->mode = (header >> 6) & 3;
  h->error_protection = ((header >> 16) & 1) ^ 1;
  h->stereo = (h->mode == 3 ? 0 : 1);
  if (h->lsf)
    h->ssize = (h->stereo == 1 ? 9 : 17);
  else
    h->ssize = (h->stereo == 1 ? 17: 32);
  if (h->error_protection)
    h->ssize += 2;
  h->bitrate_index = (header >> 12) & 15;
  if (h->mpeg25)
    h->sampling_frequency = 6 + ((header >> 10) & 3);
  else
    h->sampling_frequency = ((header >> 10) & 3) + (h->lsf * 3);
  h->padding = (header >> 9) & 1;
  h->framesize = (long)mp3_tabsel[h->lsf][h->bitrate_index] * 144000;
  h->framesize /= (mp3_freqs[h->sampling_frequency] << h->lsf);
  h->framesize = h->framesize + h->padding - 4;
}

