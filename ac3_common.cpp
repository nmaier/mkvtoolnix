/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  ac3_common.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: ac3_common.cpp,v 1.4 2003/05/18 20:57:07 mosu Exp $
    \brief helper function for AC3 data
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <string.h>

#include "ac3_common.h"

int find_ac3_header(unsigned char *buf, int size, ac3_header_t *ac3_header) {
  static int rate[] = { 32,  40,  48,  56,  64,  80,  96, 112, 128, 160,
                       192, 224, 256, 320, 384, 448, 512, 576, 640};
  static unsigned char lfeon[8] = {0x10, 0x10, 0x04, 0x04, 0x04, 0x01, 
                                   0x04, 0x01};
  static unsigned char halfrate[12] = {0, 0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3};
  ac3_header_t header;
  int i;
  int frmsizecod;
  int bitrate;
  int half;
  int acmod;

  for (i = 0; i < (size - 7); i++) {
    if ((buf[i] != 0x0b) || (buf[i + 1] != 0x77))
      continue;
    if (buf[i + 5] >= 0x60)
      continue;
    half = halfrate[buf[i + 5] >> 3];
    acmod = buf[i + 6] >> 5;
    header.flags = ((((buf[i + 6] & 0xf8) == 0x50) ? A52_DOLBY : acmod) |
                     ((buf[i + 6] & lfeon[acmod]) ? A52_LFE : 0));
    frmsizecod = buf[i + 4] & 63;
    if (frmsizecod >= 38)
      return -1;
    bitrate = rate[frmsizecod >> 1];
    header.bit_rate = (bitrate * 1000) >> half;
    switch (buf[i + 4] & 0xc0) {
      case 0:
        header.sample_rate = 48000 >> half;
        header.bytes = 4 * bitrate;
        break;
      case 0x40:
        header.sample_rate = 44100 >> half;
        header.bytes = 2 * (320 * bitrate / 147 + (frmsizecod & 1));
        break;
      case 0x80:
        header.sample_rate = 32000 >> half;
        header.bytes = 6 * bitrate;
        break;
      default:
        return -1;
    }
    switch(header.flags & A52_CHANNEL_MASK) {
      case A52_MONO: 
        header.channels=1;
        break;
      case A52_CHANNEL: 
      case A52_STEREO:
      case A52_CHANNEL1:
      case A52_CHANNEL2:
      case A52_DOLBY: 
        header.channels=2;
        break;
      case A52_2F1R:
      case A52_3F: 
        header.channels=3;
        break;
      case A52_3F1R:
      case A52_2F2R:
        header.channels=4;
        break;
      case A52_3F2R:
        header.channels=5;
        break;
    }
    if (header.flags & A52_LFE)
      header.channels++;
    memcpy(ac3_header, &header, sizeof(ac3_header_t));
    
    return i;
  }
  
  return -1;
}
