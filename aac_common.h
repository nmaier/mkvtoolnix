/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  aac_common.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: aac_common.h,v 1.1 2003/05/17 21:01:28 mosu Exp $
    \brief definitions and helper functions for AAC data
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __AACCOMMON_H
#define __AACCOMMON_H

typedef struct {
  int sample_rate;
  int bit_rate;
  int channels;
  int bytes;
  int id;                       // 0 = MPEG-4, 1 = MPEG-2
  int profile;
  int header_bit_size, header_byte_size;
} aac_header_t;

int find_aac_header(unsigned char *buf, int size, aac_header_t *aac_header);

#endif // __AACCOMMON_H
