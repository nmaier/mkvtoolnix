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
    \version \$Id: aac_common.cpp,v 1.3 2003/05/18 20:57:07 mosu Exp $
    \brief helper function for AAC data
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "aac_common.h"

static const int sample_rates[16] = {96000, 88200, 64000, 48000, 44100, 32000,
                                     24000, 22050, 16000, 12000, 11025,  8000,
                                     0, 0, 0, 0}; // filling

int parse_aac_adif_header(unsigned char *buf, int size,
                          aac_header_t *aac_header) {
  int i, k, pos;
  int64_t bits;
  int nprogram_conf_e, bitstream_type, profile, sfreq_index;
  int nfront_c_e, nside_c_e, nback_c_e, nlfe_c_e;
  int nassoc_data_e, nvalid_cc_e, comment_field_bytes;
  int channels;

  pos = 0;
  if ((bits = get_bits(buf, size, pos, 32)) < 0)
    return 0;
  if ((uint32_t)bits != FOURCC('A', 'D', 'I', 'F'))
    return 0;
  if ((bits = get_bits(buf, size, pos, 1)) < 0) // copyright_id_present
    return 0;
  if (bits == 1)
    if ((bits = get_bits(buf, size, pos, 72)) < 0) // copyright_id
      return 0;
  if ((bits = get_bits(buf, size, pos, 1)) < 0) // original_copy
    return 0;
  if ((bits = get_bits(buf, size, pos, 1)) < 0) // home
    return 0;
  if ((bitstream_type = get_bits(buf, size, pos, 1)) < 0)
    return 0;
  if ((bits = get_bits(buf, size, pos, 23)) < 0) // bitrate
    return 0;
  if ((nprogram_conf_e = get_bits(buf, size, pos, 4)) < 0)
    return 0;
  for (i = 0; i <= nprogram_conf_e; i++) {
    channels = 0;
    if (bitstream_type == 0)
      if ((bits = get_bits(buf, size, pos, 20)) < 0)
        return 0;
    if ((bits = get_bits(buf, size, pos, 4)) < 0) // element_instance_tag
      return 0;
    if ((profile = get_bits(buf, size, pos, 2)) < 0)
      return 0;
    if ((sfreq_index = get_bits(buf, size, pos, 4)) < 0)
      return 0;
    if ((nfront_c_e = get_bits(buf, size, pos, 4)) < 0)
      return 0;
    if ((nside_c_e = get_bits(buf, size, pos, 4)) < 0)
      return 0;
    if ((nback_c_e = get_bits(buf, size, pos, 4)) < 0)
      return 0;
    if ((nlfe_c_e = get_bits(buf, size, pos, 2)) < 0)
      return 0;
    if ((nassoc_data_e = get_bits(buf, size, pos, 3)) < 0)
      return 0;
    if ((nvalid_cc_e = get_bits(buf, size, pos, 4)) < 0)
      return 0;
    if ((bits = get_bits(buf, size, pos, 1)) < 0) // mono_mixdown_present
      return 0;
    if (bits == 1)
      if ((bits = get_bits(buf, size, pos, 4)) < 0) // mono_mixdown_el_num
        return 0;
    if ((bits = get_bits(buf, size, pos, 1)) < 0) // stereo_mixdown_present
      return 0;
    if (bits == 1)
      if ((bits = get_bits(buf, size, pos, 4)) < 0) // stereo_mixdown_el_num
        return 0;
    if ((bits = get_bits(buf, size, pos, 1)) < 0) // matrix_mixdown_idx_present
      return 0;
    if (bits == 1) {
      if ((bits = get_bits(buf, size, pos, 2)) < 0) // matrix_mixdown_idx
        return 0;
      if ((bits = get_bits(buf, size, pos, 1)) < 0) // pseudo_surround_enable
        return 0;
    }
    channels = nfront_c_e + nside_c_e + nback_c_e;
    for (k = 0; k < (nfront_c_e + nside_c_e + nback_c_e); k++) {
      if ((bits = get_bits(buf, size, pos, 1)) < 0) // *_element_is_cpe
        return 0;
      if (bits)
        channels++;
      if ((bits = get_bits(buf, size, pos, 4)) < 0) // *_element_tag_select
        return 0;
    }
    channels += nlfe_c_e;
    for (k = 0; k < (nlfe_c_e + nassoc_data_e); k++)
      if ((bits = get_bits(buf, size, pos, 4)) < 0) // *_element_tag_select
        return 0;
    for (k = 0; k < nvalid_cc_e; k++) {
      if ((bits = get_bits(buf, size, pos, 1)) < 0) // cc_e_is_ind_sw
        return 0;
      if ((bits = get_bits(buf, size, pos, 4)) < 0) // valid_cc_e_tag_select
        return 0;
    }
    pos = ((pos + 7) / 8) * 8;  // byte aligntment
    if (pos >= (size * 8))
      return 0;
    if ((comment_field_bytes = get_bits(buf, size, pos, 8)) < 0)
      return 0;
    for (k = 0; k < comment_field_bytes; k++)
      if ((bits = get_bits(buf, size, pos, 8)) < 0)
        return 0;
  }

  aac_header->sample_rate = sample_rates[sfreq_index];
  aac_header->id = 0;           // MPEG-4
  aac_header->profile = profile;
  aac_header->bytes = 0;
  aac_header->channels = channels > 6 ? 2 : channels;
  aac_header->bit_rate = 1024;
  aac_header->header_bit_size = pos;
  aac_header->header_byte_size = (aac_header->header_bit_size + 7) / 8;

  return 1;
}

static int is_adts_header(unsigned char *buf, int size, int bpos,
                          aac_header_t *aac_header) {
  int pos = bpos * 8;
  int64_t bits;
  int id, protection_absent, profile, sfreq_index, channels, frame_length;

  if ((bits = get_bits(buf, size, pos, 12)) < 0)
    return -1;                  // All bytes used up

  if (bits != 0xfff)            // ADTS header
    return 0;

  if ((id = get_bits(buf, size, pos, 1)) < 0) // ID: 0 = MPEG-4, 1 = MPEG-2
    return 0;
  if ((bits = get_bits(buf, size, pos, 2)) < 0) // layer: = 0 !!
    return 0;
  if (bits != 0)
    return 0;
  if ((protection_absent = get_bits(buf, size, pos, 1)) < 0)
    return 0;
  if ((profile = get_bits(buf, size, pos, 2)) < 0)
    return 0;
  if ((sfreq_index = get_bits(buf, size, pos, 4)) < 0)
    return 0;
  if ((bits = get_bits(buf, size, pos, 1)) < 0) // private
    return 0;
  if ((channels = get_bits(buf, size, pos, 3)) < 0)
    return 0;
  if ((bits = get_bits(buf, size, pos, 1)) < 0) // original/copy
    return 0;
  if ((bits = get_bits(buf, size, pos, 1)) < 0) // home
    return 0;
  if (id == 0) {
    if ((bits = get_bits(buf, size, pos, 2)) < 0) // emphasis, MPEG-4 only
      return 0;
  }
  if ((bits = get_bits(buf, size, pos, 1)) < 0) // copyright_id_bit
    return 0;
  if ((bits = get_bits(buf, size, pos, 1)) < 0) // copyright_id_start
    return 0;
  if ((frame_length = get_bits(buf, size, pos, 13)) < 0)
    return 0;
  if ((bits = get_bits(buf, size, pos, 11)) < 0) // adts_buffer_fullness
    return 0;
  if ((bits = get_bits(buf, size, pos, 2)) < 0) // no_raw_blocks_in_frame
    return 0;
  if (!protection_absent) {
    if ((bits = get_bits(buf, size, pos, 16)) < 0)
      return 0;
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

  return 1;
}

int find_aac_header(unsigned char *buf, int size, aac_header_t *aac_header) {
  int bpos;

  bpos = 0;
  while (bpos < size) {
    if (is_adts_header(buf, size, bpos, aac_header))
      return bpos;
    bpos++;
  }

  return -1;
}
