/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   helper function for AAC data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include <stdio.h>
#include <string.h>

#include "bit_cursor.h"
#include "common.h"
#include "aac_common.h"
#include "matroska.h"

const int aac_sampling_freq[16] = {96000, 88200, 64000, 48000, 44100, 32000,
                                   24000, 22050, 16000, 12000, 11025,  8000,
                                   0, 0, 0, 0}; // filling

bool
parse_aac_adif_header_internal(unsigned char *buf,
                               int size,
                               aac_header_t *aac_header) {
  int i, k;
  unsigned int bits;
  int nprogram_conf_e, bitstream_type, profile, sfreq_index;
  int nfront_c_e, nside_c_e, nback_c_e, nlfe_c_e;
  int nassoc_data_e, nvalid_cc_e, comment_field_bytes;
  int channels;
  bit_cursor_c bc(buf, size);

  profile = 0;
  sfreq_index = 0;
  comment_field_bytes = 0;
  channels = 0;
  bc.get_bits(32, bits);
  if (bits != FOURCC('A', 'D', 'I', 'F'))
    return false;
  if (bc.get_bit())             // copyright_id_present
    bc.skip_bits(3 * 72);       // copyright_id
  bc.skip_bits(2);              // original_copy & home
  bitstream_type = bc.get_bit();
  bc.skip_bits(23);             // bitrate
  bc.get_bits(4, nprogram_conf_e);
  for (i = 0; i <= nprogram_conf_e; i++) {
    channels = 0;
    if (bitstream_type == 0)
      bc.skip_bits(20);
    bc.skip_bits(4);            // element_instance_tag
    bc.get_bits(2, profile);
    bc.get_bits(4, sfreq_index);
    bc.get_bits(4, nfront_c_e);
    bc.get_bits(4, nside_c_e);
    bc.get_bits(4, nback_c_e);
    bc.get_bits(2, nlfe_c_e);
    bc.get_bits(3, nassoc_data_e);
    bc.get_bits(4, nvalid_cc_e);
    if (bc.get_bit())           // mono_mixdown_present
      bc.skip_bits(4);          // mono_mixdown_el_num
    if (bc.get_bit())           // stereo_mixdown_present
      bc.skip_bits(4);          // stereo_mixdown_el_num
    if (bc.get_bit()) {         // matrix_mixdown_idx_present
      bc.skip_bits(2 + 1);      // matrix_mixdown_idx & pseudo_surround_table
    }
    channels = nfront_c_e + nside_c_e + nback_c_e;
    for (k = 0; k < (nfront_c_e + nside_c_e + nback_c_e); k++) {
      if (bc.get_bit())         // *_element_is_cpe
        channels++;
      bc.skip_bits(4);          // *_element_tag_select
    }
    channels += nlfe_c_e;
    bc.skip_bits(4 * (nlfe_c_e + nassoc_data_e)); // *_element_tag_select
    bc.skip_bits((1 + 4) * nvalid_cc_e); // cc_e_is_ind_sw & valid_cc_e_tag_sel
    bc.byte_align();
    bc.skip_bits(8);
    bc.skip_bits(8 * comment_field_bytes);
  }

  aac_header->sample_rate = aac_sampling_freq[sfreq_index];
  aac_header->id = 0;           // MPEG-4
  aac_header->profile = profile;
  aac_header->bytes = 0;
  aac_header->channels = channels > 6 ? 2 : channels;
  aac_header->bit_rate = 1024;
  aac_header->header_bit_size = bc.get_bit_position();
  aac_header->header_byte_size = (aac_header->header_bit_size + 7) / 8;

  return true;
}

bool
parse_aac_adif_header(unsigned char *buf,
                      int size,
                      aac_header_t *aac_header) {
  try {
    return parse_aac_adif_header_internal(buf, size, aac_header);
  } catch (...) {
    return false;
  }
}

static bool
is_adts_header(unsigned char *buf,
               int size,
               int bpos,
               aac_header_t *aac_header,
               bool emphasis_present) {
  int id, profile, sfreq_index, channels, frame_length;
  bool protection_absent;
  unsigned int bits;
  bit_cursor_c bc(buf, size);

  bc.get_bits(12, bits);
  if (bits != 0xfff)            // ADTS header
    return false;

  id = bc.get_bit();            // ID: 0 = MPEG-4, 1 = MPEG-2
  bc.get_bits(2, bits);         // layer: = 0 !!
  if (bits != 0)
    return false;
  protection_absent = bc.get_bit();
  bc.get_bits(2, profile);
  bc.get_bits(4, sfreq_index);
  bc.skip_bits(1);              // private
  bc.get_bits(3, channels);
  bc.skip_bits(1 + 1);          // original/copy & home
  if ((id == 0) && emphasis_present)
    bc.skip_bits(2);            // emphasis, MPEG-4 only
  bc.skip_bits(1 + 1);          // copyright_id_bit & copyright_id_start
  bc.get_bits(13, frame_length);
  bc.skip_bits(11);             // adts_buffer_fullness
  bc.skip_bits(2);              // no_raw_blocks_in_frame
  if (!protection_absent)
    bc.skip_bits(16);

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

  return true;
}

int
find_aac_header(unsigned char *buf,
                int size,
                aac_header_t *aac_header,
                bool emphasis_present) {
  int bpos;

  try {
    bpos = 0;
    while (bpos < size) {
      if (is_adts_header(buf, size, bpos, aac_header, emphasis_present))
        return bpos;
      bpos++;
    }
  } catch (...) {
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

  profile = data[0] >> 3;
  if (0 == profile)
    return false;
  --profile;
  sample_rate = aac_sampling_freq[((data[0] & 0x07) << 1) | (data[1] >> 7)];
  channels = (data[1] & 0x7f) >> 3;
  if (size == 5) {
    output_sample_rate = aac_sampling_freq[(data[4] & 0x7f) >> 3];
    sbr = true;
  } else if (sample_rate <= 24000) {
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
  string sprofile;

  if (codec_id.size() < strlen(MKV_A_AAC_2LC))
    return false;

  if (codec_id[10] == '2')
    id = AAC_ID_MPEG2;
  else if (codec_id[10] == '4')
    id = AAC_ID_MPEG4;
  else
    return false;

  sprofile = codec_id.substr(12);
  if (sprofile == "MAIN")
    profile = AAC_PROFILE_MAIN;
  else if (sprofile == "LC")
    profile = AAC_PROFILE_LC;
  else if (sprofile == "SSR")
    profile = AAC_PROFILE_SSR;
  else if (sprofile == "LTP")
    profile = AAC_PROFILE_LTP;
  else if (sprofile == "LC/SBR")
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
