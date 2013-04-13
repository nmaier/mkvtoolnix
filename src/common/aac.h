/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   definitions and helper functions for AAC data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_AACCOMMON_H
#define MTX_COMMON_AACCOMMON_H

#include "common/common_pch.h"

#include "common/bit_cursor.h"

#define AAC_ID_MPEG4 0
#define AAC_ID_MPEG2 1

#define AAC_PROFILE_MAIN 0
#define AAC_PROFILE_LC   1
#define AAC_PROFILE_SSR  2
#define AAC_PROFILE_LTP  3
#define AAC_PROFILE_SBR  4

#define AAC_SYNC_EXTENSION_TYPE 0x02b7

#define AAC_MAX_PRIVATE_DATA_SIZE 5

extern const int g_aac_sampling_freq[16];

class aac_header_c {
public:
  int object_type, extension_object_type, profile, sample_rate, output_sample_rate, bit_rate, channels, bytes;
  int id;                       // 0 = MPEG-4, 1 = MPEG-2
  int header_bit_size, header_byte_size, data_byte_size;

  bool is_sbr, is_valid;

protected:
  bit_reader_cptr m_bc;

public:
  aac_header_c();

  std::string to_string() const;

public:
  static aac_header_c from_audio_specific_config(const unsigned char *data, int size);

protected:
  int read_object_type();
  int read_sample_rate();
  void read_ga_specific_config();
  void read_program_config_element();
  void read_error_protection_specific_config();
  void parse(const unsigned char *data, int size);
};

bool operator ==(const aac_header_c &h1, const aac_header_c &h2);

bool parse_aac_adif_header(const unsigned char *buf, int size, aac_header_c *aac_header);
int find_aac_header(const unsigned char *buf, int size, aac_header_c *aac_header, bool emphasis_present);
int find_consecutive_aac_headers(const unsigned char *buf, int size, int num);

int get_aac_sampling_freq_idx(int sampling_freq);

bool parse_aac_data(const unsigned char *data, int size, int &profile, int &channels, int &sample_rate, int &output_sample_rate, bool &sbr);
int create_aac_data(unsigned char *data, int profile, int channels, int sample_rate, int output_sample_rate, bool sbr);
bool parse_aac_codec_id(const std::string &codec_id, int &id, int &profile);

#endif // MTX_COMMON_AACCOMMON_H
