/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * helper function for AAC data
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#include <stdio.h>
#include <string.h>

#include "common.h"
#include "aac_common.h"
#include "matroska.h"

const int aac_sampling_freq[16] = {96000, 88200, 64000, 48000, 44100, 32000,
                                   24000, 22050, 16000, 12000, 11025,  8000,
                                   0, 0, 0, 0}; // filling

int
parse_aac_adif_header(unsigned char *buf,
                      int size,
                      aac_header_t *aac_header) {
  int i, k;
  bool b, eob;
  unsigned int bits;
  int nprogram_conf_e, bitstream_type, profile, sfreq_index;
  int nfront_c_e, nside_c_e, nback_c_e, nlfe_c_e;
  int nassoc_data_e, nvalid_cc_e, comment_field_bytes;
  int channels;
  bit_cursor_c bc(buf, size);

  eob = false;
  comment_field_bytes = 0;
  channels = 0;
  bc.get_bits(32, bits);
  if (bits != FOURCC('A', 'D', 'I', 'F'))
    return 0;
  bc.get_bit(b);                // copyright_id_present
  if (b) {
    for (i = 0; i < 3; i++)
      eob = !bc.get_bits(24, bits); // copyright_id
  }
  bc.get_bit(b);                // original_copy
  bc.get_bit(b);                // home
  bc.get_bits(1, bitstream_type);
  bc.get_bits(23, bits);        // bitrate
  bc.get_bits(4, nprogram_conf_e);
  for (i = 0; i <= nprogram_conf_e; i++) {
    channels = 0;
    if (bitstream_type == 0)
      bc.get_bits(20, bits);
    bc.get_bits(4, bits);       // element_instance_tag
    bc.get_bits(2, profile);
    bc.get_bits(4, sfreq_index);
    bc.get_bits(4, nfront_c_e);
    bc.get_bits(4, nside_c_e);
    bc.get_bits(4, nback_c_e);
    bc.get_bits(2, nlfe_c_e);
    bc.get_bits(3, nassoc_data_e);
    bc.get_bits(4, nvalid_cc_e);
    bc.get_bit(b);              // mono_mixdown_present
    if (b)
      bc.get_bits(4, bits);     // mono_mixdown_el_num
    bc.get_bit(b);              // stereo_mixdown_present
    if (b)
      bc.get_bits(4, bits);     // stereo_mixdown_el_num
    bc.get_bit(b);              // matrix_mixdown_idx_present
    if (b) {
      bc.get_bits(2, bits);     // matrix_mixdown_idx
      eob = !bc.get_bits(1, bits); // pseudo_surround_enable
    }
    channels = nfront_c_e + nside_c_e + nback_c_e;
    for (k = 0; k < (nfront_c_e + nside_c_e + nback_c_e); k++) {
      bc.get_bit(b);            // *_element_is_cpe
      if (b)
        channels++;
      bc.get_bits(4, bits);     // *_element_tag_select
    }
    channels += nlfe_c_e;
    for (k = 0; k < (nlfe_c_e + nassoc_data_e); k++)
      bc.get_bits(4, bits);     // *_element_tag_select
    for (k = 0; k < nvalid_cc_e; k++) {
      bc.get_bits(1, bits);     // cc_e_is_ind_sw
      bc.get_bits(4, bits);     // valid_cc_e_tag_select
    }
    bc.byte_align();
    eob = !bc.get_bits(8, bits);
    for (k = 0; k < comment_field_bytes; k++)
      eob = !bc.get_bits(8, bits);
  }

  if (eob)
    return 0;

  aac_header->sample_rate = aac_sampling_freq[sfreq_index];
  aac_header->id = 0;           // MPEG-4
  aac_header->profile = profile;
  aac_header->bytes = 0;
  aac_header->channels = channels > 6 ? 2 : channels;
  aac_header->bit_rate = 1024;
  aac_header->header_bit_size = bc.get_bit_position();
  aac_header->header_byte_size = (aac_header->header_bit_size + 7) / 8;

  return 1;
}

static int
is_adts_header(unsigned char *buf,
               int size,
               int bpos,
               aac_header_t *aac_header,
               bool emphasis_present) {
  int id, profile, sfreq_index, channels, frame_length;
  bool eob, protection_absent, b;
  unsigned int bits;
  bit_cursor_c bc(buf, size);

  bc.get_bits(12, bits);
  if (bits != 0xfff)            // ADTS header
    return 0;

  bc.get_bits(1, id);           // ID: 0 = MPEG-4, 1 = MPEG-2
  bc.get_bits(2, bits);         // layer: = 0 !!
  if (bits != 0)
    return 0;
  bc.get_bit(protection_absent);
  bc.get_bits(2, profile);
  bc.get_bits(4, sfreq_index);
  bc.get_bit(b);                // private
  bc.get_bits(3, channels);
  bc.get_bit(b);                // original/copy
  bc.get_bit(b);                // home
  if ((id == 0) && emphasis_present)
    bc.get_bits(2, bits);       // emphasis, MPEG-4 only
  bc.get_bit(b);                // copyright_id_bit
  bc.get_bit(b);                // copyright_id_start
  bc.get_bits(13, frame_length);
  bc.get_bits(11, bits);        // adts_buffer_fullness
  eob = !bc.get_bits(2, bits);   // no_raw_blocks_in_frame
  if (!protection_absent)
    eob = !bc.get_bits(16, bits);

  if (eob)
    return 0;

  aac_header->sample_rate = aac_sampling_freq[sfreq_index];
  aac_header->id = id;
  aac_header->profile = profile;
  aac_header->bytes = frame_length;
  aac_header->channels = channels > 6 ? 2 : channels;
  aac_header->bit_rate = 1024;
  if ((id == 0) && emphasis_present) // MPEG-4
    aac_header->header_bit_size = 58;
  else
    aac_header->header_bit_size = 56;
  if (!protection_absent)
    aac_header->header_bit_size += 16;
  aac_header->header_byte_size = (aac_header->header_bit_size + 7) / 8;
  aac_header->data_byte_size = aac_header->bytes -
    aac_header->header_bit_size / 8;

  return 1;
}

int
find_aac_header(unsigned char *buf,
                int size,
                aac_header_t *aac_header,
                bool emphasis_present) {
  int bpos;

  bpos = 0;
  while (bpos < size) {
    if (is_adts_header(buf, size, bpos, aac_header, emphasis_present))
      return bpos;
    bpos++;
  }

  return -1;
}

int
get_aac_sampling_freq_idx(int sampling_freq) {
  int i;

  for (i = 0; i < 16; i++)
    if (sampling_freq >= (aac_sampling_freq[i] - 1000))
      return i;

  return 0;                     // should never happen
}

bool
parse_aac_data(unsigned char *data,
               int size,
               int &profile,
               int &channels,
               int &sample_rate,
               int &output_sample_rate,
               bool &sbr) {
  int i;

  if (size < 2)
    return false;

  mxverb(4, "parse_aac_data: size %d, data: 0x", size);
  for (i = 0; i < size; i++)
    mxverb(4, "%02x ", data[i]);
  mxverb(4, "\n");

  profile = (data[0] >> 3) - 1;
  sample_rate = aac_sampling_freq[((data[0] & 0x07) << 1) | (data[1] >> 7)];
  channels = (data[1] & 0x7f) >> 3;
  if (size == 5) {
    output_sample_rate = aac_sampling_freq[(data[4] & 0x7f) >> 3];
    sbr = true;
  } else if (sample_rate < 44100) {
    output_sample_rate = 2 * sample_rate;
    sbr = true;
  } else
    sbr = false;

  return true;
}

bool
parse_aac_codec_id(const string &codec_id,
                   int &id,
                   int &profile) {
  if (codec_id.size() < strlen(MKV_A_AAC_2LC))
    return false;

  if (codec_id[10] == '2')
    id = AAC_ID_MPEG2;
  else if (codec_id[10] == '4')
    id = AAC_ID_MPEG4;
  else
    return false;

  if (!strcmp(&codec_id[12], "MAIN"))
    profile = AAC_PROFILE_MAIN;
  else if (!strcmp(&codec_id[12], "LC"))
    profile = AAC_PROFILE_LC;
  else if (!strcmp(&codec_id[12], "SSR"))
    profile = AAC_PROFILE_SSR;
  else if (!strcmp(&codec_id[12], "LTP"))
    profile = AAC_PROFILE_LTP;
  else if (!strcmp(&codec_id[12], "LC/SBR"))
    profile = AAC_PROFILE_SBR;
  else
    return false;

  return true;
}

int
create_aac_data(unsigned char *data,
                int profile,
                int channels,
                int sample_rate,
                int output_sample_rate,
                bool sbr) {
  int srate_idx;

  srate_idx = get_aac_sampling_freq_idx(sample_rate);
  data[0] = ((profile + 1) << 3) | ((srate_idx & 0x0e) >> 1);
  data[1] = ((srate_idx & 0x01) << 7) | (channels << 3);
  if (sbr) {
    srate_idx = get_aac_sampling_freq_idx(output_sample_rate);
    data[2] = AAC_SYNC_EXTENSION_TYPE >> 3;
    data[3] = ((AAC_SYNC_EXTENSION_TYPE & 0x07) << 5) | 5;
    data[4] = (1 << 7) | (srate_idx << 3);
    return 5;
  }
  return 2;
}
