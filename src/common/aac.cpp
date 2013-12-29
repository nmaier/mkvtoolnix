/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper function for AAC data

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include <stdio.h>
#include <string.h>

#include "common/aac.h"
#include "common/bit_cursor.h"
#include "common/codec.h"
#include "common/strings/formatting.h"

const int g_aac_sampling_freq[16] = {96000, 88200, 64000, 48000, 44100, 32000,
                                     24000, 22050, 16000, 12000, 11025,  8000,
                                      7350,     0,     0,     0}; // filling

aac_header_c::aac_header_c()
  : object_type{}
  , extension_object_type{}
  , profile{}
  , sample_rate{}
  , output_sample_rate{}
  , bit_rate{}
  , channels{}
  , bytes{}
  , id{}
  , header_bit_size{}
  , header_byte_size{}
  , data_byte_size{}
  , is_sbr{}
  , is_valid{}
{
}

std::string
aac_header_c::to_string()
  const {
  return (boost::format("sample_rate: %1%; bit_rate: %2%; channels: %3%; bytes: %4%; id: %5%; profile: %6%; header_bit_size: %7%; header_byte_size: %8%; data_byte_size: %9%")
          % sample_rate % bit_rate % channels % bytes % id % profile % header_bit_size % header_byte_size % data_byte_size).str();
}

int
aac_header_c::read_object_type() {
  int object_type = m_bc->get_bits(5);
  return 31 == object_type ? 32 + m_bc->get_bits(6) : object_type;
}

int
aac_header_c::read_sample_rate() {
  int idx = m_bc->get_bits(4);
  return 0x0f == idx ? m_bc->get_bits(24) : g_aac_sampling_freq[idx];
}

aac_header_c
aac_header_c::from_audio_specific_config(const unsigned char *data,
                                         int size) {
  aac_header_c header;
  header.parse(data, size);
  return header;
}

void
aac_header_c::read_ga_specific_config() {
  m_bc->skip_bit();             // frame_length_flag
  if (m_bc->get_bit())          // depends_on_core_coder
    m_bc->skip_bits(14);        // core_coder_delay
  bool extension_flag = m_bc->get_bit();

  if (!channels)
    read_program_config_element();

  if ((6 == object_type) || (20 == object_type))
    m_bc->skip_bits(3);         // layer_nr

  if (!extension_flag)
    return;

  if (22 == object_type)
    m_bc->skip_bits(5 + 11);    // num_of_sub_frame, layer_length

  if ((17 == object_type) || (19 == object_type) || (20 == object_type) || (23 == object_type))
    m_bc->skip_bits(1 + 1 + 1); // aac_section_data_resilience_flag, aac_scalefactor_data_resilience_flag, aac_spectral_data_resilience_flag

  m_bc->skip_bit();             // extension_flag3
}

void
aac_header_c::read_error_protection_specific_config() {
  mxerror(boost::format("aac_error_proection_specific_config. %1%\n") % BUGMSG);
}

void
aac_header_c::read_program_config_element() {
  m_bc->skip_bits(4);           // element_instance_tag
  object_type        = m_bc->get_bits(2);
  sample_rate        = g_aac_sampling_freq[m_bc->get_bits(4)];
  int num_front_chan = m_bc->get_bits(4);
  int num_side_chan  = m_bc->get_bits(4);
  int num_back_chan  = m_bc->get_bits(4);
  int num_lfe_chan   = m_bc->get_bits(2);
  int num_assoc_data = m_bc->get_bits(3);
  int num_valid_cc   = m_bc->get_bits(4);

  if (m_bc->get_bit())          // mono_mixdown_present_flag
    m_bc->skip_bits(4);         // mono_mixdown_element_number
  if (m_bc->get_bit())          // stereo_mixdown_present_flag
    m_bc->skip_bits(4);         // stereo_mixdown_element_number
  if (m_bc->get_bit())          // matrix_mixdown_idx_present_flag
    m_bc->skip_bits(2 + 1);     // matrix_mixdown_idx, pseudo_surround_enable

  channels = num_front_chan + num_side_chan + num_back_chan + num_lfe_chan;

  for (int idx = 0; idx < (num_front_chan + num_side_chan + num_back_chan); ++idx) {
    if (m_bc->get_bit())        // *_element_is_cpe
      ++channels;
    m_bc->skip_bits(4);         // *_element_tag_select
  }
  m_bc->skip_bits(num_lfe_chan   *      4);  //                       lfe_element_tag_select
  m_bc->skip_bits(num_assoc_data *      4);  //                       assoc_data_element_tag_select
  m_bc->skip_bits(num_valid_cc   * (1 + 4)); // cc_element_is_ind_sw, valid_cc_element_tag_select

  m_bc->byte_align();
  m_bc->skip_bits(m_bc->get_bits(8) * 8);    // comment_field_bytes, comment_field_data
}

void
aac_header_c::parse(const unsigned char *data,
                    int size) {
  static debugging_option_c s_debug{"parse_aac_data|aac_full"};

  if (size < 2)
    return;

  mxdebug_if(s_debug, boost::format("parse_aac_data: size %1%, data: %2%\n") % size % to_hex(data, size));

  try {
    m_bc        = std::make_shared<bit_reader_c>(data, size);
    object_type = read_object_type();

    if (!object_type)
      return;

    is_sbr      = false;
    profile     = object_type - 1;
    sample_rate = read_sample_rate();
    channels    = m_bc->get_bits(4);

    if (5 == object_type) {
      is_sbr                = true;
      output_sample_rate    = read_sample_rate();
      extension_object_type = object_type;
      object_type           = read_object_type();
    }

    if (   ( 1 == object_type) || ( 2 == object_type) || ( 3 == object_type) || ( 4 == object_type) || ( 6 == object_type) || ( 7 == object_type)
        || (17 == object_type) || (19 == object_type) || (20 == object_type) || (21 == object_type) || (22 == object_type) || (23 == object_type))
      read_ga_specific_config();
    else
      mxerror(boost::format("aac_object_type_not_ga_specific. %1%\n") % BUGMSG);

    if ((17 == object_type) || ((19 <= object_type) && (27 >= object_type))) {
      int ep_config = m_bc->get_bits(2);
      if ((2 == ep_config) || (3 == ep_config))
        read_error_protection_specific_config();
      if (3 == ep_config)
        m_bc->skip_bit();       // direct_mapping
    }

    if ((5 != extension_object_type) && (m_bc->get_remaining_bits() >= 16)) {
      int sync_extension_type = m_bc->get_bits(11);
      if (0x2b7 == sync_extension_type) {
        extension_object_type = read_object_type();
        if (5 == extension_object_type) {
          is_sbr = m_bc->get_bit();
          if (is_sbr)
            output_sample_rate = read_sample_rate();
        }
      }
    }

    is_valid = true;

    if (sample_rate <= 24000) {
      output_sample_rate = 2 * sample_rate;
      is_sbr             = true;
    }

  } catch (mtx::exception &ex) {
    mxdebug_if(s_debug, boost::format("parse_aac_data: exception: %1%\n") % ex);
  }
}

static bool
parse_aac_adif_header_internal(const unsigned char *buf,
                               int size,
                               aac_header_c *aac_header) {
  bit_reader_c bc(buf, size);

  int profile             = 0;
  int sfreq_index         = 0;
  int comment_field_bytes = 0;
  int channels            = 0;

  if (bc.get_bits(32) != FOURCC('A', 'D', 'I', 'F'))
    return false;

  if (bc.get_bit())             // copyright_id_present
    bc.skip_bits(3 * 72);       // copyright_id
  bc.skip_bits(2);              // original_copy & home
  int bitstream_type = bc.get_bit();
  bc.skip_bits(23);             // bitrate
  int nprogram_conf_e = bc.get_bits(4);

  int program_conf_e_idx;
  for (program_conf_e_idx = 0; program_conf_e_idx <= nprogram_conf_e; program_conf_e_idx++) {
    channels = 0;
    if (0 == bitstream_type)
      bc.skip_bits(20);
    bc.skip_bits(4);            // element_instance_tag
    profile           = bc.get_bits(2);
    sfreq_index       = bc.get_bits(4);
    int nfront_c_e    = bc.get_bits(4);
    int nside_c_e     = bc.get_bits(4);
    int nback_c_e     = bc.get_bits(4);
    int nlfe_c_e      = bc.get_bits(2);
    int nassoc_data_e = bc.get_bits(3);
    int nvalid_cc_e   = bc.get_bits(4);
    if (bc.get_bit())           // mono_mixdown_present
      bc.skip_bits(4);          // mono_mixdown_el_num
    if (bc.get_bit())           // stereo_mixdown_present
      bc.skip_bits(4);          // stereo_mixdown_el_num
    if (bc.get_bit())           // matrix_mixdown_idx_present
      bc.skip_bits(2 + 1);      // matrix_mixdown_idx & pseudo_surround_table

    channels = nfront_c_e + nside_c_e + nback_c_e;

    int channel_idx;
    for (channel_idx = 0; channel_idx < (nfront_c_e + nside_c_e + nback_c_e); channel_idx++) {
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

  aac_header->sample_rate      = g_aac_sampling_freq[sfreq_index];
  aac_header->id               = 0;           // MPEG-4
  aac_header->profile          = profile;
  aac_header->bytes            = 0;
  aac_header->channels         = channels > 6 ? 2 : channels;
  aac_header->bit_rate         = 1024;
  aac_header->header_bit_size  = bc.get_bit_position();
  aac_header->header_byte_size = (aac_header->header_bit_size + 7) / 8;

  return true;
}

bool
parse_aac_adif_header(const unsigned char *buf,
                      int size,
                      aac_header_c *aac_header) {
  try {
    return parse_aac_adif_header_internal(buf, size, aac_header);
  } catch (...) {
    return false;
  }
}

static bool
is_adts_header(const unsigned char *buf,
               int size,
               aac_header_c *aac_header,
               bool emphasis_present) {
  bit_reader_c bc(buf, size);

  if (bc.get_bits(12) != 0xfff)            // ADTS header
    return false;

  int id = bc.get_bit();        // ID: 0 = MPEG-4, 1 = MPEG-2
  if (bc.get_bits(2) != 0)      // layer == 0 !
    return false;
  bool protection_absent = bc.get_bit();
  int profile            = bc.get_bits(2);
  int sfreq_index        = bc.get_bits(4);
  bc.skip_bits(1);              // private
  int channels = bc.get_bits(3);
  bc.skip_bits(1 + 1);          // original/copy & home
  if ((0 == id) && emphasis_present)
    bc.skip_bits(2);            // emphasis, MPEG-4 only
  bc.skip_bits(1 + 1);          // copyright_id_bit & copyright_id_start

  int frame_length    = bc.get_bits(13);
  int header_bit_size = (((0 == id) && emphasis_present) ? 58 : 56) + (!protection_absent ? 16 : 0);

  if (header_bit_size >= frame_length * 8)
    return false;

  bc.skip_bits(11);             // adts_buffer_fullness
  bc.skip_bits(2);              // no_raw_blocks_in_frame
  if (!protection_absent)
    bc.skip_bits(16);

  aac_header->sample_rate      = g_aac_sampling_freq[sfreq_index];
  aac_header->id               = id;
  aac_header->profile          = profile;
  aac_header->bytes            = frame_length;
  aac_header->channels         = channels > 6 ? 2 : channels;
  aac_header->bit_rate         = 1024;
  aac_header->header_bit_size  = header_bit_size;
  aac_header->header_byte_size = (aac_header->header_bit_size + 7) / 8;
  aac_header->data_byte_size   = aac_header->bytes - aac_header->header_bit_size / 8;

  return true;
}

int
find_aac_header(const unsigned char *buf,
                int size,
                aac_header_c *aac_header,
                bool emphasis_present) {
  try {
    int bpos = 0;
    while (bpos < size) {
      if (is_adts_header(buf + bpos, size - bpos, aac_header, emphasis_present))
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
    if (sampling_freq >= (g_aac_sampling_freq[i] - 1000))
      return i;

  return 0;                     // should never happen
}

bool
parse_aac_data(const unsigned char *data,
               int size,
               int &profile,
               int &channels,
               int &sample_rate,
               int &output_sample_rate,
               bool &sbr) {
  auto h = aac_header_c::from_audio_specific_config(data, size);
  if (!h.is_valid)
    return false;

  profile            = h.profile;
  channels           = h.channels;
  sample_rate        = h.sample_rate;
  output_sample_rate = h.output_sample_rate;
  sbr                = h.is_sbr;

  return true;
}

bool
parse_aac_codec_id(const std::string &codec_id,
                   int &id,
                   int &profile) {
  std::string sprofile;

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
  int srate_idx = get_aac_sampling_freq_idx(sample_rate);
  data[0]       = ((profile + 1) << 3) | ((srate_idx & 0x0e) >> 1);
  data[1]       = ((srate_idx & 0x01) << 7) | (channels << 3);

  if (sbr) {
    srate_idx = get_aac_sampling_freq_idx(output_sample_rate);
    data[2]   = AAC_SYNC_EXTENSION_TYPE >> 3;
    data[3]   = ((AAC_SYNC_EXTENSION_TYPE & 0x07) << 5) | 5;
    data[4]   = (1 << 7) | (srate_idx << 3);

    return 5;
  }

  return 2;
}

int
find_consecutive_aac_headers(const unsigned char *buf,
                             int size,
                             int num) {
  aac_header_c aac_header, new_header;

  int base = 0;
  int pos  = find_aac_header(&buf[base], size - base, &aac_header, false);

  if (0 > pos)
    return -1;

  if (1 == num)
    return pos;
  base += pos;

  do {
    mxverb(4, boost::format("find_cons_aac_h: starting with base at %1%\n") % base);

    int offset = aac_header.bytes;
    int i;
    for (i = 0; (num - 1) > i; ++i) {
      if ((size - base - offset) < 2)
        break;

      pos = find_aac_header(&buf[base + offset], size - base - offset, &new_header, false);
      if (0 == pos) {
        if (new_header == aac_header) {
          mxverb(4, boost::format("find_cons_aac_h: found good header %1%\n") % i);
          offset += new_header.bytes;
          continue;
        } else
          break;
      } else
        break;
    }

    if (i == (num - 1))
      return base;

    ++base;
    offset = 0;
    pos    = find_aac_header(&buf[base], size - base, &aac_header, false);

    if (-1 == pos)
      return -1;

    base += pos;
  } while (base < (size - 5));

  return -1;
}

bool
operator ==(const aac_header_c &h1,
            const aac_header_c &h2) {
  return (h1.sample_rate == h2.sample_rate)
      && (h1.bit_rate    == h2.bit_rate)
      && (h1.channels    == h2.channels)
      && (h1.id          == h2.id)
      && (h1.profile     == h2.profile);
}
