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
    \version $Id$
    \brief definitions and helper functions for AAC data
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __AACCOMMON_H
#define __AACCOMMON_H

extern const int aac_sampling_freq[16];

typedef struct {
  int sample_rate;
  int bit_rate;
  int channels;
  int bytes;
  int id;                       // 0 = MPEG-4, 1 = MPEG-2
  int profile;
  int header_bit_size, header_byte_size, data_byte_size;
} aac_header_t;

int parse_aac_adif_header(unsigned char *buf, int size,
                          aac_header_t *aac_header);
int find_aac_header(unsigned char *buf, int size, aac_header_t *aac_header,
                    bool emphasis_present);
int get_aac_sampling_freq_idx(int sampling_freq);

#endif // __AACCOMMON_H
