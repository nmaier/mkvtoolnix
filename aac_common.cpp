/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  aac_common.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version \$Id: aac_common.cpp,v 1.1 2003/05/17 21:01:28 mosu Exp $
    \brief helper function for AAC data
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "aac_common.h"

static const int sample_rates[16] = {96000, 88200, 64000, 48000, 44100, 32000,
                                     24000, 22050, 16000, 12000, 11025,  8000,
                                     0, 0, 0, 0}; // filling

int find_aac_header(unsigned char *buf, int size, aac_header_t *aac_header) {
  int pos, bits, found_at;
  int id, protection_absent, profile, sfreq_index, channels, frame_length;

  pos = 0;

  while (1) {
    found_at = pos / 8;

    if ((bits = get_bits(buf, size, pos, 12)) < 0)
      return -1;                // All bytes used up

    if (bits != 0xfff) {        // ADTS header
      pos = ((pos + 7) / 8) * 8;
      continue;
    }

    if ((id = get_bits(buf, size, pos, 1)) < 0)
      return -1;

    if ((bits = get_bits(buf, size, pos, 2)) < 0)
      return -1;

    if (bits != 0) {
      pos = (found_at + 1) * 8;
      continue;
    }

    if ((protection_absent = get_bits(buf, size, pos, 1)) < 0)
      return -1;

    if ((profile = get_bits(buf, size, pos, 2)) < 0)
      return -1;

    if ((sfreq_index = get_bits(buf, size, pos, 4)) < 0)
      return -1;

    if ((bits = get_bits(buf, size, pos, 1)) < 0)
      return -1;

    if ((channels = get_bits(buf, size, pos, 3)) < 0)
      return -1;

    if ((bits = get_bits(buf, size, pos, 1)) < 0)
      return -1;

    if ((bits = get_bits(buf, size, pos, 1)) < 0)
      return -1;

    if (id == 0) {
      if ((bits = get_bits(buf, size, pos, 2)) < 0)
        return -1;
    }

    if ((bits = get_bits(buf, size, pos, 1)) < 0)
      return -1;

    if ((bits = get_bits(buf, size, pos, 1)) < 0)
      return -1;

    if ((frame_length = get_bits(buf, size, pos, 13)) < 0)
      return -1;

    if ((bits = get_bits(buf, size, pos, 11)) < 0)
      return -1;

    if ((bits = get_bits(buf, size, pos, 2)) < 0)
      return -1;

    if (!protection_absent) {
      if ((bits = get_bits(buf, size, pos, 16)) < 0)
        return -1;
    }

    aac_header->sample_rate = sample_rates[sfreq_index];
    aac_header->id = id;
    aac_header->profile = profile;
    aac_header->bytes = frame_length;
    aac_header->channels = channels > 6 ? 2 : channels;
    aac_header->bit_rate = 1024;
    if (id == 0)                // MPEG-4
      aac_header->header_bit_size = 58;
    else
      aac_header->header_bit_size = 56;
    if (!protection_absent)
      aac_header->header_bit_size += 16;
    aac_header->header_byte_size = (aac_header->header_bit_size + 7) / 8;

    return found_at;
  }

  return -1;
}
