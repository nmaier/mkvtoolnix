/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  dts_common.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: dts_common.h,v 1.1 2003/05/15 08:58:52 mosu Exp $
    \brief definitions and helper functions for DTS data
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __DTSCOMMON_H
#define __DTSCOMMON_H

#define A52_CHANNEL 0
#define A52_MONO 1
#define A52_STEREO 2
#define A52_3F 3
#define A52_2F1R 4
#define A52_3F1R 5
#define A52_2F2R 6
#define A52_3F2R 7
#define A52_CHANNEL1 8
#define A52_CHANNEL2 9
#define A52_DOLBY 10
#define A52_CHANNEL_MASK 15

#define A52_LFE 16

typedef struct {
  int sample_rate;
  int bit_rate;
  // int channels;
  int flags;
  int bytes;
} dts_header_t;

int find_dts_header(unsigned char *buf, int size, dts_header_t *dts_header);

#endif // __DTSCOMMON_H

