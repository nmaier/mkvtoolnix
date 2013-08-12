/** MPEG video helper functions (MPEG 1, 2 and 4)

   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   Part of this code (the functions \c decode_rbsp_trailing() ) were
   taken or inspired and modified from the ffmpeg project (
   http://ffmpeg.sourceforge.net/index.php ). These functions were
   licensed under the LGPL.

   \file

*/

#include "common/common_pch.h"

#include <unordered_map>

#include "common/bit_cursor.h"
#include "common/byte_buffer.h"
#include "common/checksums.h"
#include "common/endian.h"
#include "common/hacks.h"
#include "common/math.h"
#include "common/mm_io.h"
#include "common/hevc.h"
#include "common/strings/formatting.h"

namespace hevc {

hevcc_c::hevcc_c()
  : m_configuration_version{}
  , m_general_profile_space{}
  , m_general_tier_flag{}
  , m_general_profile_idc{}
  , m_general_profile_compatibility_flag{}
  , m_general_progressive_source_flag{}
  , m_general_interlace_source_flag{}
  , m_general_nonpacked_constraint_flag{}
  , m_general_frame_only_constraint_flag{}
  , m_general_level_idc{}
  , m_min_spatial_segmentation_idc{}
  , m_parallelism_type{}
  , m_chroma_format_idc{}
  , m_bit_depth_luma_minus8{}
  , m_bit_depth_chroma_minus8{}
  , m_max_sub_layers{}
  , m_temporal_id_nesting_flag{}
  , m_size_nalu_minus_one{}
  , m_nalu_size_length{}
{
}

hevcc_c::hevcc_c(unsigned int nalu_size_length,
                 std::vector<memory_cptr> const &vps_list,
                 std::vector<memory_cptr> const &sps_list,
                 std::vector<memory_cptr> const &pps_list)
  : m_configuration_version{}
  , m_general_profile_space{}
  , m_general_tier_flag{}
  , m_general_profile_idc{}
  , m_general_profile_compatibility_flag{}
  , m_general_progressive_source_flag{}
  , m_general_interlace_source_flag{}
  , m_general_nonpacked_constraint_flag{}
  , m_general_frame_only_constraint_flag{}
  , m_general_level_idc{}
  , m_min_spatial_segmentation_idc{}
  , m_parallelism_type{}
  , m_chroma_format_idc{}
  , m_bit_depth_luma_minus8{}
  , m_bit_depth_chroma_minus8{}
  , m_max_sub_layers{}
  , m_temporal_id_nesting_flag{}
  , m_size_nalu_minus_one{}
  , m_nalu_size_length{nalu_size_length}
  , m_vps_list{vps_list}
  , m_sps_list{sps_list}
  , m_pps_list{pps_list}
{
}

hevcc_c::operator bool()
  const {
  return m_nalu_size_length
      && !m_vps_list.empty()
      && !m_sps_list.empty()
      && !m_pps_list.empty()
      && (m_vps_info_list.empty() || (m_vps_info_list.size() == m_vps_list.size()))
      && (m_sps_info_list.empty() || (m_sps_info_list.size() == m_sps_list.size()))
      && (m_pps_info_list.empty() || (m_pps_info_list.size() == m_pps_list.size()));
}

bool
hevcc_c::parse_vps_list(bool ignore_errors) {
  if (m_vps_info_list.size() == m_vps_list.size())
    return true;

  m_vps_info_list.clear();
  for (auto &vps: m_vps_list) {
    vps_info_t vps_info;
    auto vps_as_rbsp = vps->clone();
    nalu_to_rbsp(vps_as_rbsp);

    if (ignore_errors) {
      try {
        parse_vps(vps_as_rbsp, vps_info);
      } catch (mtx::mm_io::end_of_file_x &) {
      }
    } else if (!parse_vps(vps_as_rbsp, vps_info))
      return false;

    m_vps_info_list.push_back(vps_info);
  }

  return true;
}

bool
hevcc_c::parse_sps_list(bool ignore_errors) {
  if (m_sps_info_list.size() == m_sps_list.size())
    return true;

  m_sps_info_list.clear();
  for (auto &sps: m_sps_list) {
    sps_info_t sps_info;
    auto sps_as_rbsp = sps->clone();
    nalu_to_rbsp(sps_as_rbsp);

    if (ignore_errors) {
      try {
        parse_sps(sps_as_rbsp, sps_info, m_vps_info_list);
      } catch (mtx::mm_io::end_of_file_x &) {
      }
    } else if (!parse_sps(sps_as_rbsp, sps_info, m_vps_info_list))
      return false;

    m_sps_info_list.push_back(sps_info);
  }

  return true;
}

bool
hevcc_c::parse_pps_list(bool ignore_errors) {
  if (m_pps_info_list.size() == m_pps_list.size())
    return true;

  m_pps_info_list.clear();
  for (auto &pps: m_pps_list) {
    pps_info_t pps_info;
    auto pps_as_rbsp = pps->clone();
    nalu_to_rbsp(pps_as_rbsp);

    if (ignore_errors) {
      try {
        parse_pps(pps_as_rbsp, pps_info);
      } catch (mtx::mm_io::end_of_file_x &) {
      }
    } else if (!parse_pps(pps_as_rbsp, pps_info))
      return false;

    m_pps_info_list.push_back(pps_info);
  }

  return true;
}

bool
hevcc_c::parse_sei_list(bool ignore_errors) {
  if (m_sei_info_list.size() == m_sei_list.size())
    return true;

  m_sei_info_list.clear();
  for (auto &sei: m_sei_list) {
    sei_info_t sei_info;
    auto sei_as_rbsp = sei->clone();
    nalu_to_rbsp(sei_as_rbsp);

    if (ignore_errors) {
      try {
        parse_sei(sei_as_rbsp, sei_info);
      } catch (mtx::mm_io::end_of_file_x &) {
      }
    } else if (!parse_sei(sei_as_rbsp, sei_info))
      return false;

    m_sei_info_list.push_back(sei_info);
  }

  return true;
}

/* Codec Private Data

The format of the MKV CodecPrivate element for HEVC has been aligned with MP4 and GPAC/MP4Box.
The definition of MP4 for HEVC has not been finalized. The version of MP4Box appears to be
aligned with the latest version of the HEVC standard. The configuration_version field should be
kept 0 until CodecPrivate for HEVC have been finalized. Thereafter it shall have the required value of 1.
The CodecPrivate format is flexible and allows storage of arbitrary NAL units.
However it is restricted by MP4 to VPS, SPS and PPS headers and SEI messages that apply to the
whole stream as for example user data. The table below specifies the format:

Value                               Bits  Description
-----                               ----  -----------
configuration_version               8   The value should be 0 until the format has been finalized. Thereafter is should have the specified value (probably 1). This allows us to recognize (and ignore) non-standard CodecPrivate
general_profile_space               2     Specifies the context for the interpretation of general_profile_idc and  general_profile_compatibility_flag
general_tier_flag                   1     Specifies the context for the interpretation of general_level_idc
general_profile_idc                 5     Defines the profile of the bitstream
general_profile_compatibility_flag  32    Defines profile compatibility, see [2] for interpretation
general_progressive_source_flag     1     Source is progressive, see [2] for interpretation.
general_interlace_source_flag       1     Source is interlaced, see [2] for interpretation.
general_nonpacked_constraint_flag   1     If 1 then no frame packing arrangement SEI messages, see [2] for more information
general_frame_only_constraint_flag  1     If 1 then no fields, see [2] for interpretation
reserved                            44    Reserved field, value TBD 0
general_level_idc                   8     Defines the level of the bitstream
reserved                            4     Reserved Field, value '1111'b
min_spatial_segmentation_idc        12    Maximum possible size of distinct coded spatial segmentation regions in the pictures of the CVS
reserved                            6     Reserved Field, value '111111'b
parallelism_type                    2     0=unknown, 1=slices, 2=tiles, 3=WPP
reserved                            6     Reserved field, value '111111'b
chroma_format_idc                   2     See table 6-1, HEVC
reserved                            5     Reserved Field, value '11111'b
bit_depth_luma_minus8               3     Bit depth luma minus 8
reserved                            5     Reserved Field, value '11111'b
bit_depth_chroma_minus8             3     Bit depth chroma minus 8
reserved                            16    Reserved Field, value 0
reserved                            2     Reserved Field, value 0
max_sub_layers                      3     maximum number of temporal sub-layers
temporal_id_nesting_flag            1     Specifies whether inter prediction is additionally restricted. see [2] for interpretation.
size_nalu_minus_one                 2     Size of field NALU Length – 1
num_parameter_sets                  8     Number of parameter sets

for (i=0;i<num_parameter_sets;i++) {
  array_completeness                1     1 when there is no duplicate parameter set with same id in the stream, 0 otherwise or unknown
  reserved                          1     Value '1'b
  nal_unit_type                     6     Nal unit type, restricted to VPS, SPS, PPS and SEI, SEI must be of declarative nature which applies to the whole stream such as user data sei.
  nal_unit_count                    16    Number of nal units
  for (j=0;j<nalu_unit_count;j+) {
    size                            16    Size of nal unit
    for(k=0;k<size;k++) {
      data[k]                       8     Nalu data+
    }
  }
}

*/
memory_cptr
hevcc_c::pack() {
  parse_vps_list(true);
  parse_sps_list(true);
  parse_pps_list(true);
  parse_sei_list(true);

  if (!*this)
    return memory_cptr{};

  unsigned int total_size = 23;

  for (auto &mem : m_vps_list)
    total_size += mem->get_size() + 1 + 2 + 2; // NALU bytes + (array_comp|reserved|nal_unit_type) + nal_unit_count + NALU size
  for (auto &mem : m_sps_list)
    total_size += mem->get_size() + 1 + 2 + 2; // NALU bytes + (array_comp|reserved|nal_unit_type) + nal_unit_count + NALU size
  for (auto &mem : m_pps_list)
    total_size += mem->get_size() + 1 + 2 + 2; // NALU bytes + (array_comp|reserved|nal_unit_type) + nal_unit_count + NALU size
  for (auto &mem : m_sei_list)
    total_size += mem->get_size() + 1 + 2 + 2; // NALU bytes + (array_comp|reserved|nal_unit_type) + nal_unit_count + NALU size

  auto destination = memory_c::alloc(total_size);
  auto buffer      = destination->get_buffer();

  // for (i=0;i<num_parameter_sets;i++) {
  //   array_completeness                   1  1 when there is no duplicate parameter set with same id in the stream, 0 otherwise or unknown
  //   reserved                             1  Value '1'b
  //   nal_unit_type                        6  Nal unit type, restricted to VPS, SPS, PPS and SEI, SEI must be of declarative nature which applies to the
  //                                           whole stream such as user data sei.
  //   nal_unit_count                       16 Number of nal units
  //   for (j=0;j<nalu_unit_count;j+) {
  //     size                               16 Size of nal unit
  //     for(k=0;k<size;k++) {
  //       data[k]                          8  Nalu data+
  //     }
  //   }
  // }
  auto write_list = [&buffer](std::vector<memory_cptr> const &list, uint8 nal_unit_type) {
    *buffer++ = (0 << 7) | (1 << 6) | (nal_unit_type & 0x3F);
    put_uint16_be(buffer, list.size());
    buffer += 2;

    for (auto &mem : list) {
      auto size = mem->get_size();
      put_uint16_be(buffer, size);
      memcpy(buffer + 2, mem->get_buffer(), size);
      buffer += 2 + size;
    }
  };

  // configuration version
  *buffer++ = m_vps_info_list[0].codec_private.configuration_version;
  // general parameters block
  memcpy(buffer, &(m_vps_info_list[0].codec_private.general_params_block), GENERAL_PARAMS_BLOCK_SIZE);
  buffer += GENERAL_PARAMS_BLOCK_SIZE;
  // reserved                            4     Reserved Field, value '1111'b
  // min_spatial_segmentation_idc        12    Maximum possible size of distinct coded spatial segmentation regions in the pictures of the CVS
  *buffer++= 0xF0 | ((m_sps_info_list[0].min_spatial_segmentation_idc >> 8) & 0x0F);
  *buffer++ = m_sps_info_list[0].min_spatial_segmentation_idc & 0XFF;
  // reserved                            6     Reserved Field, value '111111'b
  // parallelism_type                    2     0=unknown, 1=slices, 2=tiles, 3=WPP
  *buffer++ = 0xFC | 0x00; // unknown
  // reserved                            6     Reserved field, value '111111'b
  // chroma_format_idc                   2     See table 6-1, HEVC
  *buffer++ = 0xFC | (m_sps_info_list[0].chroma_format_idc & 0x03);
  // reserved                            5     Reserved Field, value '11111'b
  // bit_depth_luma_minus8               3     Bit depth luma minus 8
  *buffer++ = 0xF8 | ((m_sps_info_list[0].bit_depth_luma_minus8) & 0x07);
  // reserved                            5     Reserved Field, value '11111'b
  // bit_depth_chroma_minus8             3     Bit depth chroma minus 8
  *buffer++ = 0xF8 | ((m_sps_info_list[0].bit_depth_chroma_minus8) & 0x07);
  // reserved                            16    Reserved Field, value 0
  *buffer++ = 0;
  *buffer++ = 0;
  // reserved                            2     Reserved Field, value 0
  // max_sub_layers                      3     maximum number of temporal sub-layers
  // temporal_id_nesting_flag            1     Specifies whether inter prediction is additionally restricted. see [2] for interpretation.
  // size_nalu_minus_one                 2     Size of field NALU Length – 1
  *buffer++ = (((m_sps_info_list[0].max_sub_layers_minus1 + 1) & 0x03) << 6) |
              ((m_sps_info_list[0].temporal_id_nesting_flag & 0x01) << 2) |
              ((m_nalu_size_length - 1) & 0x03);
  // num_parameter_sets                  8     Number of parameter sets
  unsigned int num_parameter_sets = m_vps_list.size() + m_sps_list.size() + m_pps_list.size() + m_sei_list.size();
  *buffer++ = num_parameter_sets;

  if(m_vps_list.size())
    write_list(m_vps_list, HEVC_NALU_TYPE_VIDEO_PARAM);
  if(m_sps_list.size())
    write_list(m_sps_list, HEVC_NALU_TYPE_SEQ_PARAM);
  if(m_pps_list.size())
    write_list(m_pps_list, HEVC_NALU_TYPE_PIC_PARAM);
  if(m_sei_list.size())
    write_list(m_sei_list, HEVC_NALU_TYPE_PREFIX_SEI);

  return destination;
}

hevcc_c
hevcc_c::unpack(memory_cptr const &mem) {
  hevcc_c hevcc;

  if (!mem)
    return hevcc;

  try {
    bit_reader_c bit_reader(mem->get_buffer(), mem->get_size());
    mm_mem_io_c byte_reader{*mem};

    auto read_list = [&byte_reader](std::vector<memory_cptr> &list, uint8 nal_unit_type) {

      auto type = byte_reader.read_uint8() & 0x3F;

      if(type == nal_unit_type) {
        auto nal_unit_count = byte_reader.read_uint16_be();

        while (nal_unit_count) {
          auto size = byte_reader.read_uint16_be();
          list.push_back(byte_reader.read(size));
          --nal_unit_count;
        }
      }
    };

    // configuration_version               8     The value should be 0 until the format has been finalized. Thereafter is should have the specified value
    //                                           (probably 1). This allows us to recognize (and ignore) non-standard CodecPrivate
    hevcc.m_configuration_version = bit_reader.get_bits(8);
    // general_profile_space               2     Specifies the context for the interpretation of general_profile_idc and
    //                                           general_profile_compatibility_flag
    hevcc.m_general_profile_space = bit_reader.get_bits(2);
    // general_tier_flag                   1     Specifies the context for the interpretation of general_level_idc
    hevcc.m_general_tier_flag = bit_reader.get_bits(1);
    // general_profile_idc                 5     Defines the profile of the bitstream
    hevcc.m_general_profile_idc = bit_reader.get_bits(5);
    // general_profile_compatibility_flag  32    Defines profile compatibility, see [2] for interpretation
    hevcc.m_general_profile_compatibility_flag = bit_reader.get_bits(32);
    // general_progressive_source_flag     1     Source is progressive, see [2] for interpretation.
    hevcc.m_general_progressive_source_flag = bit_reader.get_bits(1);
    // general_interlace_source_flag       1     Source is interlaced, see [2] for interpretation.
    hevcc.m_general_interlace_source_flag = bit_reader.get_bits(1);
    // general_nonpacked_constraint_flag  1     If 1 then no frame packing arrangement SEI messages, see [2] for more information
    hevcc.m_general_nonpacked_constraint_flag = bit_reader.get_bits(1);
    // general_frame_only_constraint_flag  1     If 1 then no fields, see [2] for interpretation
    hevcc.m_general_frame_only_constraint_flag = bit_reader.get_bits(1);
    // reserved                            44    Reserved field, value TBD 0
    bit_reader.skip_bits(44);
    // general_level_idc                   8     Defines the level of the bitstream
    hevcc.m_general_level_idc = bit_reader.get_bits(8);
    // reserved                            4     Reserved Field, value '1111'b
    bit_reader.skip_bits(4);
    // min_spatial_segmentation_idc        12    Maximum possible size of distinct coded spatial segmentation regions in the pictures of the CVS
    hevcc.m_min_spatial_segmentation_idc = bit_reader.get_bits(12);
    // reserved                            6     Reserved Field, value '111111'b
    bit_reader.skip_bits(6);
    // parallelism_type                    2     0=unknown, 1=slices, 2=tiles, 3=WPP
    hevcc.m_parallelism_type = bit_reader.get_bits(2);
    // reserved                            6     Reserved field, value '111111'b
    bit_reader.skip_bits(6);
    // chroma_format_idc                   2     See table 6-1, HEVC
    hevcc.m_chroma_format_idc = bit_reader.get_bits(2);
    // reserved                            5     Reserved Field, value '11111'b
    bit_reader.skip_bits(5);
    // bit_depth_luma_minus8               3     Bit depth luma minus 8
    hevcc.m_bit_depth_luma_minus8 = bit_reader.get_bits(3);
    // reserved                            5     Reserved Field, value '11111'b
    bit_reader.skip_bits(5);
    // bit_depth_chroma_minus8             3     Bit depth chroma minus 8
    hevcc.m_bit_depth_chroma_minus8 = bit_reader.get_bits(3);
    // reserved                            16    Reserved Field, value 0
    bit_reader.skip_bits(16);
    // reserved                            2     Reserved Field, value 0
    bit_reader.skip_bits(2);
    // max_sub_layers                      3     maximum number of temporal sub-layers
    hevcc.m_max_sub_layers = bit_reader.get_bits(3);
    // temporal_id_nesting_flag            1     Specifies whether inter prediction is additionally restricted. see [2] for interpretation.
    hevcc.m_temporal_id_nesting_flag = bit_reader.get_bits(1);
    // size_nalu_minus_one                 2     Size of field NALU Length – 1
    hevcc.m_size_nalu_minus_one = bit_reader.get_bits(2);

    unsigned int num_parameter_sets = bit_reader.get_bits(8);

    // now skip over initial data and read in parameter sets, use byte reader
    byte_reader.skip(23);

    if(num_parameter_sets) {
      read_list(hevcc.m_vps_list, HEVC_NALU_TYPE_VIDEO_PARAM);
      num_parameter_sets -= hevcc.m_vps_list.size();
    }

    if(num_parameter_sets) {
      read_list(hevcc.m_sps_list, HEVC_NALU_TYPE_SEQ_PARAM);
      num_parameter_sets -= hevcc.m_sps_list.size();
    }

    if(num_parameter_sets) {
      read_list(hevcc.m_pps_list, HEVC_NALU_TYPE_PIC_PARAM);
      num_parameter_sets -= hevcc.m_pps_list.size();
    }

    if(num_parameter_sets) {
      read_list(hevcc.m_sei_list, HEVC_NALU_TYPE_PREFIX_SEI);
      num_parameter_sets -= hevcc.m_sei_list.size();
    }

    return hevcc;

  } catch (mtx::mm_io::exception &) {
    return hevcc_c{};
  }
}

static const struct {
  int numerator, denominator;
} s_predefined_pars[HEVC_NUM_PREDEFINED_PARS] = {
  {   0,  0 },
  {   1,  1 },
  {  12, 11 },
  {  10, 11 },
  {  16, 11 },
  {  40, 33 },
  {  24, 11 },
  {  20, 11 },
  {  32, 11 },
  {  80, 33 },
  {  18, 11 },
  {  15, 11 },
  {  64, 33 },
  { 160, 99 },
  {   4,  3 },
  {   3,  2 },
  {   2,  1 },
};

bool
par_extraction_t::is_valid()
  const {
  return successful && numerator && denominator;
}

};

static int
gecopy(bit_reader_c &r,
       bit_writer_c &w) {
  int n = 0, bit;

  while ((bit = r.get_bit()) == 0) {
    w.put_bit(0);
    ++n;
  }

  w.put_bit(1);

  bit = w.copy_bits(n, r);

  return (1 << n) - 1 + bit;
}

static int
geread(bit_reader_c &r) {
  int n = 0, bit;

  while ((bit = r.get_bit()) == 0)
    ++n;

  bit = r.get_bits(n);

  return (1 << n) - 1 + bit;
}

static int
sgecopy(bit_reader_c &r,
        bit_writer_c &w) {
  int v = gecopy(r, w);
  return v & 1 ? (v + 1) / 2 : -(v / 2);
}

static void
profile_tier_copy(bit_reader_c &r,
                  bit_writer_c &w,
                  hevc::vps_info_t &vps,
                  unsigned int maxNumSubLayersMinus1) {
  unsigned int i;
  std::vector<bool> sub_layer_profile_present_flag, sub_layer_level_present_flag;

  w.copy_bits(2+1, r);    // general_profile_space, general_tier_flag
  vps.profile_idc = w.copy_bits(5, r);  // general_profile_idc
  w.copy_bits(32, r);     // general_profile_compatibility_flag[]
  w.copy_bits(4, r);      // general_progressive_source_flag, general_interlaced_source_flag, general_non_packed_constraint_flag, general_frame_only_constraint_flag
  w.copy_bits(44, r);     // general_reserved_zero_44bits
  vps.level_idc = w.copy_bits(8, r);    // general_level_idc

  for (i = 0; i < maxNumSubLayersMinus1; i++) {
    sub_layer_profile_present_flag.push_back(w.copy_bits(1, r)); // sub_layer_profile_present_flag[i]
    sub_layer_level_present_flag.push_back(w.copy_bits(1, r));   // sub_layer_level_present_flag[i]
  }

  if (maxNumSubLayersMinus1 > 0)
    for (i = maxNumSubLayersMinus1; i < 8; i++)
      w.copy_bits(2, r);  // reserved_zero_2bits

  for (i = 0; i < maxNumSubLayersMinus1; i++) {
    if (sub_layer_profile_present_flag[i]) {
      w.copy_bits(2+1+5, r);  // sub_layer_profile_space[i], sub_layer_tier_flag[i], sub_layer_profile_idc[i]
      w.copy_bits(32, r);     // sub_layer_profile_compatibility_flag[i][]
      w.copy_bits(4, r);      // sub_layer_progressive_source_flag[i], sub_layer_interlaced_source_flag[i], sub_layer_non_packed_constraint_flag[i], sub_layer_frame_only_constraint_flag[i]
      w.copy_bits(44, r);     // sub_layer_reserved_zero_44bits[i]
    }
    if (sub_layer_level_present_flag[i]) {
      w.copy_bits(8, r);      // sub_layer_level_idc[i]
    }
  }
}

static void
sub_layer_hrd_parameters_copy(bit_reader_c &r,
                              bit_writer_c &w,
                              unsigned int CpbCnt,
                              bool sub_pic_cpb_params_present_flag) {
  unsigned int i;

  for (i = 0; i <= CpbCnt; i++) {
    gecopy(r, w); // bit_rate_value_minus1[i]
    gecopy(r, w); // cpb_size_value_minus1[i]

    if (sub_pic_cpb_params_present_flag) {
      gecopy(r, w); // cpb_size_du_value_minus1[i]
      gecopy(r, w); // bit_rate_du_value_minus1[i]
    }

    w.copy_bits(1, r); // cbr_flag[i]
  }
}

static void
hrd_parameters_copy(bit_reader_c &r,
                    bit_writer_c &w,
                    bool commonInfPresentFlag,
                    unsigned int maxNumSubLayersMinus1) {
  unsigned int i;

  bool nal_hrd_parameters_present_flag = false;
  bool vcl_hrd_parameters_present_flag = false;
  bool sub_pic_cpb_params_present_flag = false;

  if (commonInfPresentFlag) {
    nal_hrd_parameters_present_flag = w.copy_bits(1, r); // nal_hrd_parameters_present_flag
    vcl_hrd_parameters_present_flag = w.copy_bits(1, r); // vcl_hrd_parameters_present_flag

    if (nal_hrd_parameters_present_flag || vcl_hrd_parameters_present_flag) {
      sub_pic_cpb_params_present_flag = w.copy_bits(1, r); // sub_pic_cpb_params_present_flag
      if (sub_pic_cpb_params_present_flag) {
        w.copy_bits(8, r);  // tick_divisor_minus2
        w.copy_bits(5, r);  // du_cpb_removal_delay_increment_length_minus1
        w.copy_bits(1, r);  // sub_pic_cpb_params_in_pic_timing_sei_flag
        w.copy_bits(5, r);  // dpb_output_delay_du_length_minus1
      }

      w.copy_bits(4+4, r);  // bit_rate_scale, cpb_size_scale

      if (sub_pic_cpb_params_present_flag)
        w.copy_bits(4, r);  // cpb_size_du_scale

      w.copy_bits(5, r);  // initial_cpb_removal_delay_length_minus1
      w.copy_bits(5, r);  // au_cpb_removal_delay_length_minus1
      w.copy_bits(5, r);  // dpb_output_delay_length_minus1
    }
  }

  for (i = 0; i <= maxNumSubLayersMinus1; i++) {
    bool fixed_pic_rate_general_flag = w.copy_bits(1, r); // fixed_pic_rate_general_flag[i]
    bool fixed_pic_rate_within_cvs_flag = false;
    bool low_delay_hrd_flag = false;
    unsigned int CpbCnt = 0;

    if (!fixed_pic_rate_general_flag)
      fixed_pic_rate_within_cvs_flag = w.copy_bits(1, r); // fixed_pic_rate_within_cvs_flag[i]

    if (fixed_pic_rate_within_cvs_flag)
      gecopy(r, w);                                       // elemental_duration_in_tc_minus1[i]
    else
      low_delay_hrd_flag = w.copy_bits(1, r);             // low_delay_hrd_flag[i]

    if (!low_delay_hrd_flag)
      CpbCnt = gecopy(r, w);                              // cpb_cnt_minus1[i]

    if (nal_hrd_parameters_present_flag)
      sub_layer_hrd_parameters_copy(r, w, CpbCnt, sub_pic_cpb_params_present_flag);

    if (vcl_hrd_parameters_present_flag)
      sub_layer_hrd_parameters_copy(r, w, CpbCnt, sub_pic_cpb_params_present_flag);
  }
}

static void
scaling_list_data_copy(bit_reader_c &r,
                       bit_writer_c &w) {
  unsigned int i;
  unsigned int sizeId;
  for (sizeId = 0; sizeId < 4; sizeId++) {
    unsigned int matrixId;
    for (matrixId = 0; matrixId < ((sizeId == 3) ? 2 : 6); matrixId++) {
      if (w.copy_bits(1, r) == 0) {  // scaling_list_pred_mode_flag[sizeId][matrixId]
        gecopy(r, w); // scaling_list_pred_matrix_id_delta[sizeId][matrixId]
      } else {
        unsigned int coefNum = std::min(64, (1 << (4 + (sizeId << 1))));

        if (sizeId > 1) {
          sgecopy(r, w);  // scaling_list_dc_coef_minus8[sizeId - 2][matrixId]
        }

        for (i = 0; i < coefNum; i++) {
          sgecopy(r, w);  // scaling_list_delta_coef
        }
      }
    }
  }
}

static void
short_term_ref_pic_set_copy(bit_reader_c &r,
                            bit_writer_c &w,
                            hevc::short_term_ref_pic_set_t *short_term_ref_pic_sets,
                            unsigned int idxRps,
                            unsigned int num_short_term_ref_pic_sets) {
  hevc::short_term_ref_pic_set_t* ref_st_rp_set;
  hevc::short_term_ref_pic_set_t* cur_st_rp_set = short_term_ref_pic_sets + idxRps;
  unsigned int inter_rps_pred_flag = cur_st_rp_set->inter_ref_pic_set_prediction_flag = 0;

  if (idxRps > 0)
    inter_rps_pred_flag = cur_st_rp_set->inter_ref_pic_set_prediction_flag = w.copy_bits(1, r); // inter_ref_pic_set_prediction_flag

  if (inter_rps_pred_flag) {
    int ref_idx;
    int delta_rps;
    int i;
    int k = 0;
    int k0 = 0;
    int k1 = 0;
    int code = 0;

    if (idxRps == num_short_term_ref_pic_sets)
      code = gecopy(r, w); // delta_idx_minus1

    cur_st_rp_set->delta_idx = code + 1;
    ref_idx = idxRps - 1 - code;

    ref_st_rp_set = short_term_ref_pic_sets + ref_idx;

    cur_st_rp_set->delta_rps_sign = w.copy_bits(1, r);  // delta_rps_sign
    cur_st_rp_set->delta_idx = gecopy(r, w) + 1;  // abs_delta_rps_minus1

    delta_rps = (1 - cur_st_rp_set->delta_rps_sign*2) * cur_st_rp_set->delta_idx;

    for (i = 0; i <= ref_st_rp_set->num_pics; i++) {
      int ref_id = w.copy_bits(1, r); // used_by_curr_pic_flag
      if (ref_id == 0) {
        int bit = w.copy_bits(1, r);  // use_delta_flag
        ref_id = bit*2;
      }

      if (ref_id != 0) {
        int delta_POC = delta_rps + ((i < ref_st_rp_set->num_pics) ? ref_st_rp_set->delta_poc[i] : 0);
        cur_st_rp_set->delta_poc[k] = delta_POC;
        cur_st_rp_set->used[k] = (ref_id == 1) ? 1 : 0;

        k0 += delta_POC < 0;
        k1 += delta_POC >= 0;
        k++;
      }

      cur_st_rp_set->ref_id[i] = ref_id;
    }

    cur_st_rp_set->num_ref_id = ref_st_rp_set->num_pics + 1;
    cur_st_rp_set->num_pics = k;
    cur_st_rp_set->num_negative_pics = k0;
    cur_st_rp_set->num_positive_pics = k1;

    // Sort in increasing order (smallest first)
    for (i = 1; i < cur_st_rp_set->num_pics; i++ ) {
      int delta_POC = cur_st_rp_set->delta_poc[i];
      int used = cur_st_rp_set->used[i];
      for (k = i - 1; k >= 0; k--) {
        int temp = cur_st_rp_set->delta_poc[k];

        if (delta_POC < temp) {
          cur_st_rp_set->delta_poc[k + 1] = temp;
          cur_st_rp_set->used[k + 1] = cur_st_rp_set->used[k];
          cur_st_rp_set->delta_poc[k] = delta_POC;
          cur_st_rp_set->used[k] = used;
        }
      }
    }

    // Flip the negative values to largest first
    for (i = 0, k = cur_st_rp_set->num_negative_pics - 1; i < cur_st_rp_set->num_negative_pics >> 1; i++, k--) {
      int delta_POC = cur_st_rp_set->delta_poc[i];
      int used = cur_st_rp_set->used[i];
      cur_st_rp_set->delta_poc[i] = cur_st_rp_set->delta_poc[k];
      cur_st_rp_set->used[i] = cur_st_rp_set->used[k];
      cur_st_rp_set->delta_poc[k] = delta_POC;
      cur_st_rp_set->used[k] = used;
    }
  } else {
    int prev = 0;
    int poc;
    int i;

    cur_st_rp_set->num_negative_pics = gecopy(r, w);  // num_negative_pics
    cur_st_rp_set->num_positive_pics = gecopy(r, w);  // num_positive_pics

    for (i = 0; i < cur_st_rp_set->num_negative_pics; i++) {
      int code = gecopy(r, w); // delta_poc_s0_minus1
      poc = prev - code - 1;
      prev = poc;
      cur_st_rp_set->delta_poc[i] = poc;
      cur_st_rp_set->used[i] = w.copy_bits(1, r); // used_by_curr_pic_s0_flag
    }

    prev = 0;
    cur_st_rp_set->num_pics = cur_st_rp_set->num_negative_pics + cur_st_rp_set->num_positive_pics;
    for (i = cur_st_rp_set->num_negative_pics; i < cur_st_rp_set->num_pics; i++) {
      int code = gecopy(r, w); // delta_poc_s1_minus1
      poc = prev + code + 1;
      prev = poc;
      cur_st_rp_set->delta_poc[i] = poc;
      cur_st_rp_set->used[i] = w.copy_bits(1, r); // used_by_curr_pic_s1_flag
    }
  }
}

static void
vui_parameters_copy(bit_reader_c &r,
                    bit_writer_c &w,
                    hevc::sps_info_t &sps,
                    bool keep_ar_info,
                    unsigned int max_sub_layers_minus1) {
  if (r.get_bit() == 1) {                   // aspect_ratio_info_present_flag
    unsigned int ar_type = r.get_bits(8);   // aspect_ratio_idc

    if (keep_ar_info) {
      w.put_bit(1);
      w.put_bits(8, ar_type);
    } else
      w.put_bit(0);

    sps.ar_found = true;

    if (HEVC_EXTENDED_SAR == ar_type) {
      sps.par_num = r.get_bits(16); // sar_width
      sps.par_den = r.get_bits(16); // sar_height
    } else if (HEVC_NUM_PREDEFINED_PARS >= ar_type) {
      sps.par_num = hevc::s_predefined_pars[ar_type].numerator;
      sps.par_den = hevc::s_predefined_pars[ar_type].denominator;
    }

    if (keep_ar_info) {
      w.put_bits(16, sps.par_num);
      w.put_bits(16, sps.par_den);
    }
  } else
    sps.ar_found = false;

  // copy the rest
  if (w.copy_bits(1, r) == 1)   // overscan_info_present_flag
    w.copy_bits(1, r);          // overscan_appropriate_flag
  if (w.copy_bits(1, r) == 1) { // video_signal_type_present_flag
    w.copy_bits(4, r);          // video_format, video_full_range_flag
    if (w.copy_bits(1, r) == 1) // color_desc_present_flag
      w.copy_bits(24, r);       // colour_primaries, transfer_characteristics, matrix_coefficients
  }
  if (w.copy_bits(1, r) == 1) { // chroma_loc_info_present_flag
    gecopy(r, w);               // chroma_sample_loc_type_top_field
    gecopy(r, w);               // chroma_sample_loc_type_bottom_field
  }
  w.copy_bits(3, r);            // neutral_chroma_indication_flag, field_seq_flag, frame_field_info_present_flag
  if (w.copy_bits(1, r) == 1) { // default_display_window_flag
    gecopy(r, w);               // def_disp_win_left_offset
    gecopy(r, w);               // def_disp_win_right_offset
    gecopy(r, w);               // def_disp_win_top_offset
    gecopy(r, w);               // def_disp_win_bottom_offset
  }
  sps.timing_info_present = w.copy_bits(1, r); // vui_timing_info_present_flag
  if (sps.timing_info_present) {
    sps.num_units_in_tick = w.copy_bits(32, r); // vui_num_units_in_tick
    sps.time_scale        = w.copy_bits(32, r); // vui_time_scale
    if (w.copy_bits(1, r) == 1) { // vui_poc_proportional_to_timing_flag
      gecopy(r, w); // vui_num_ticks_poc_diff_one_minus1
    }
    if (w.copy_bits(1, r) == 1) { // vui_hrd_parameters_present_flag
      hrd_parameters_copy(r, w, 1, max_sub_layers_minus1); // hrd_parameters
    }
    if (w.copy_bits(1, r) == 1) { // bitstream_restriction_flag
      w.copy_bits(3, r);  // tiles_fixed_structure_flag, motion_vectors_over_pic_boundaries_flag, restricted_ref_pic_lists_flag
      sps.min_spatial_segmentation_idc = gecopy(r, w); // min_spatial_segmentation_idc
      gecopy(r, w); // max_bytes_per_pic_denom
      gecopy(r, w); // max_bits_per_mincu_denom
      gecopy(r, w); // log2_max_mv_length_horizontal
      gecopy(r, w); // log2_max_mv_length_vertical
    }
  }
}

void
hevc::sps_info_t::dump() {
  mxinfo(boost::format("sps_info dump:\n"
                       "  id:                                    %1%\n"
                       "  log2_max_pic_order_cnt_lsb:            %2%\n"
                       "  vui_present:                           %3%\n"
                       "  ar_found:                              %4%\n"
                       "  par_num:                               %5%\n"
                       "  par_den:                               %6%\n"
                       "  timing_info_present:                   %7%\n"
                       "  num_units_in_tick:                     %8%\n"
                       "  time_scale:                            %9%\n"
                       "  width:                                 %10%\n"
                       "  height:                                %11%\n"
                       "  checksum:                              %|12$08x|\n")
         % id
         % log2_max_pic_order_cnt_lsb
         % vui_present
         % ar_found
         % par_num
         % par_den
         % timing_info_present
         % num_units_in_tick
         % time_scale
         % width
         % height
         % checksum);
}

bool
hevc::sps_info_t::timing_info_valid()
  const {
  return timing_info_present
      && (0 != num_units_in_tick)
      && (0 != time_scale);
}

int64_t
hevc::sps_info_t::default_duration()
  const {
  return 1000000000ll * num_units_in_tick / time_scale;
}

void
hevc::pps_info_t::dump() {
  mxinfo(boost::format("pps_info dump:\n"
                       "id: %1%\n"
                       "sps_id: %2%\n"
                       "checksum: %|3$08x|\n")
         % id
         % sps_id
         % checksum);
}

void
hevc::slice_info_t::dump()
  const {
  mxinfo(boost::format("slice_info dump:\n"
                       "  nalu_type:                  %1%\n"
                       "  type:                       %2%\n"
                       "  pps_id:                     %3%\n"
                       "  pic_order_cnt_lsb:          %4%\n"
                       "  sps:                        %5%\n"
                       "  pps:                        %6%\n")
         % static_cast<unsigned int>(nalu_type)
         % static_cast<unsigned int>(type)
         % static_cast<unsigned int>(pps_id)
         % pic_order_cnt_lsb
         % sps
         % pps);
}

void
hevc::nalu_to_rbsp(memory_cptr &buffer) {
  int pos, size = buffer->get_size();
  mm_mem_io_c d(nullptr, size, 100);
  unsigned char *b = buffer->get_buffer();

  for (pos = 0; pos < size; ++pos) {
    if (   ((pos + 2) < size)
        && (0 == b[pos])
        && (0 == b[pos + 1])
        && (3 == b[pos + 2])) {
      d.write_uint8(0);
      d.write_uint8(0);
      pos += 2;

    } else
      d.write_uint8(b[pos]);
  }

  buffer = memory_cptr(new memory_c(d.get_and_lock_buffer(), d.getFilePointer(), true));
}

void
hevc::rbsp_to_nalu(memory_cptr &buffer) {
  int pos, size = buffer->get_size();
  mm_mem_io_c d(nullptr, size, 100);
  unsigned char *b = buffer->get_buffer();

  for (pos = 0; pos < size; ++pos) {
    if (   ((pos + 2) < size)
        && (0 == b[pos])
        && (0 == b[pos + 1])
        && (3 >= b[pos + 2])) {
      d.write_uint8(0);
      d.write_uint8(0);
      d.write_uint8(3);
      ++pos;

    } else
      d.write_uint8(b[pos]);
  }

  buffer = memory_cptr(new memory_c(d.get_and_lock_buffer(), d.getFilePointer(), true));
}

bool
hevc::parse_vps(memory_cptr &buffer,
                vps_info_t &vps) {
  int size              = buffer->get_size();
  unsigned char *newsps = (unsigned char *)safemalloc(size + 100);
  memset(newsps, 0, sizeof(char) * (size+100));
  memory_cptr mcptr_newsps(new memory_c(newsps, size + 100, true));
  bit_reader_c r(buffer->get_buffer(), size);
  bit_writer_c w(newsps, size + 100);
  unsigned int i, j;

  memset(&vps, 0, sizeof(vps));

  w.copy_bits(1, r);            // forbidden_zero_bit
  if (w.copy_bits(6, r) != 32)  // nal_unit_type
      return false;
  w.copy_bits(6, r);            // nuh_reserved_zero_6bits
  w.copy_bits(3, r);            // nuh_temporal_id_plus1

  vps.id = w.copy_bits(4, r);                       // vps_video_parameter_set_id
  w.copy_bits(2+6, r);                              // vps_reserved_three_2bits, vps_reserved_zero_6bits
  vps.max_sub_layers_minus1 = w.copy_bits(3, r);    // vps_max_sub_layers_minus1
  w.copy_bits(1+16, r);                             // vps_temporal_id_nesting_flag, vps_reserved_0xffff_16bits

  // At this point we are at newsps + 6 bytes, profile_tier_level follows
  profile_tier_copy(r, w, vps, vps.max_sub_layers_minus1);  // profile_tier_level(vps_max_sub_layers_minus1)

  // First 12 bytes of profile_tier_level belong in codec private data under the general_params_block
  memcpy(&vps.codec_private.general_params_block, newsps+6, 12);

  bool vps_sub_layer_ordering_info_present_flag = w.copy_bits(1, r);  // vps_sub_layer_ordering_info_present_flag
  for (i = (vps_sub_layer_ordering_info_present_flag ? 0 : vps.max_sub_layers_minus1); i <= vps.max_sub_layers_minus1; i++) {
    gecopy(r, w); // vps_max_dec_pic_buffering_minus1[i]
    gecopy(r, w); // vps_max_num_reorder_pics[i]
    gecopy(r, w); // vps_max_latency_increase[i]
  }

  unsigned int vps_max_nuh_reserved_zero_layer_id = w.copy_bits(6, r);  // vps_max_nuh_reserved_zero_layer_id
  bool vps_num_op_sets_minus1 = gecopy(r, w);       // vps_num_op_sets_minus1
  for (i = 1; i <= vps_num_op_sets_minus1; i++) {
    for (j = 0; j <= vps_max_nuh_reserved_zero_layer_id; j++) { // operation_point_set(i)
      w.copy_bits(1, r);  // layer_id_included_flag
    }
  }

  if (w.copy_bits(1, r) == 1) { // vps_timing_info_present_flag
    w.copy_bits(32, r);         // vps_num_units_in_tick
    w.copy_bits(32, r);         // vps_time_scale
    if (w.copy_bits(1, r) == 1)  // vps_poc_proportional_to_timing_flag
      gecopy(r, w);             // vps_num_ticks_poc_diff_one_minus1
    unsigned int vps_num_hrd_parameters = gecopy(r, w); // vps_num_hrd_parameters
    for (i = 0; i < vps_num_hrd_parameters; i++) {
      bool cprms_present_flag = false;
      gecopy(r, w);             // hrd_op_set_idx[i]
      if (i > 0)
        cprms_present_flag = w.copy_bits(1, r); // cprms_present_flag[i]
      hrd_parameters_copy(r, w, cprms_present_flag, vps.max_sub_layers_minus1);
    }
  }

  if (w.copy_bits(1, r) == 1)    // vps_extension_flag
    while (r.get_remaining_bits())
      w.copy_bits(1, r);        // vps_extension_data_flag

  w.put_bit(1);
  w.byte_align();

  buffer = mcptr_newsps;
  buffer->set_size(w.get_bit_position() / 8);

  vps.checksum = calc_adler32(buffer->get_buffer(), buffer->get_size());

  return true;
}

bool
hevc::parse_sps(memory_cptr &buffer,
                sps_info_t &sps,
                std::vector<vps_info_t> &m_vps_info_list,
                bool keep_ar_info) {
  int size              = buffer->get_size();
  unsigned char *newsps = (unsigned char *)safemalloc(size + 100);
  memory_cptr mcptr_newsps(new memory_c(newsps, size + 100, true));
  bit_reader_c r(buffer->get_buffer(), size);
  bit_writer_c w(newsps, size + 100);
  unsigned int i;

  keep_ar_info = !hack_engaged(ENGAGE_REMOVE_BITSTREAM_AR_INFO);

  memset(&sps, 0, sizeof(sps));

  sps.par_num  = 1;
  sps.par_den  = 1;
  sps.ar_found = false;

  w.copy_bits(1, r);            // forbidden_zero_bit
  if (w.copy_bits(6, r) != 33)  // nal_unit_type
      return false;
  w.copy_bits(6, r);            // nuh_reserved_zero_6bits
  w.copy_bits(3, r);            // nuh_temporal_id_plus1

  sps.vps_id = w.copy_bits(4, r);                   // sps_video_parameter_set_id
  sps.max_sub_layers_minus1 = w.copy_bits(3, r);    // sps_max_sub_layers_minus1
  sps.temporal_id_nesting_flag = w.copy_bits(1, r); // sps_temporal_id_nesting_flag

  size_t vps_idx;
  for (vps_idx = 0; m_vps_info_list.size() > vps_idx; ++vps_idx)
    if (m_vps_info_list[vps_idx].id == sps.vps_id)
      break;
  if (m_vps_info_list.size() == vps_idx)
    return false;

  sps.vps = vps_idx;

  vps_info_t &vps = m_vps_info_list[vps_idx];

  profile_tier_copy(r, w, vps, sps.max_sub_layers_minus1);  // profile_tier_level(sps_max_sub_layers_minus1)

  sps.id = gecopy(r, w);  // sps_seq_parameter_set_id

  if ((sps.chroma_format_idc = gecopy(r, w)) == 3) // chroma_format_idc
    sps.separate_colour_plane_flag = w.copy_bits(1, r);    // separate_colour_plane_flag

  sps.width = gecopy(r, w); // pic_width_in_luma_samples
  sps.height = gecopy(r, w); // pic_height_in_luma_samples

  if (w.copy_bits(1, r) == 1) {
    gecopy(r, w); // conf_win_left_offset
    gecopy(r, w); // conf_win_right_offset
    gecopy(r, w); // conf_win_top_offset
    gecopy(r, w); // conf_win_bottom_offset
  }

  sps.bit_depth_luma_minus8 = gecopy(r, w); // bit_depth_luma_minus8
  sps.bit_depth_chroma_minus8 = gecopy(r, w); // bit_depth_chroma_minus8
  sps.log2_max_pic_order_cnt_lsb = gecopy(r, w) + 4; // log2_max_pic_order_cnt_lsb_minus4

  bool sps_sub_layer_ordering_info_present_flag = w.copy_bits(1, r);  // sps_sub_layer_ordering_info_present_flag
  for (i = (sps_sub_layer_ordering_info_present_flag ? 0 : sps.max_sub_layers_minus1); i <= sps.max_sub_layers_minus1; i++) {
    gecopy(r, w); // sps_max_dec_pic_buffering_minus1[i]
    gecopy(r, w); // sps_max_num_reorder_pics[i]
    gecopy(r, w); // sps_max_latency_increase[i]
  }

  sps.log2_min_luma_coding_block_size_minus3 = gecopy(r, w); // log2_min_luma_coding_block_size_minus3
  sps.log2_diff_max_min_luma_coding_block_size = gecopy(r, w); // log2_diff_max_min_luma_coding_block_size
  gecopy(r, w); // log2_min_transform_block_size_minus2
  gecopy(r, w); // log2_diff_max_min_transform_block_size
  gecopy(r, w); // max_transform_hierarchy_depth_inter
  gecopy(r, w); // max_transform_hierarchy_depth_intra

  if (w.copy_bits(1, r) == 1)   // scaling_list_enabled_flag
    if (w.copy_bits(1, r) == 1) // sps_scaling_list_data_present_flag
      scaling_list_data_copy(r, w);

  w.copy_bits(1, r);  // amp_enabled_flag
  w.copy_bits(1, r);  // sample_adaptive_offset_enabled_flag

  if (w.copy_bits(1, r) == 1) { // pcm_enabled_flag
    w.copy_bits(4, r);  // pcm_sample_bit_depth_luma_minus1
    w.copy_bits(4, r);  // pcm_sample_bit_depth_chroma_minus1
    gecopy(r, w); // log2_min_pcm_luma_coding_block_size_minus3
    gecopy(r, w); // log2_diff_max_min_pcm_luma_coding_block_size
    w.copy_bits(1, r);  // pcm_loop_filter_disable_flag
  }

  unsigned int num_short_term_ref_pic_sets = gecopy(r, w);  // num_short_term_ref_pic_sets
  for (i = 0; i < num_short_term_ref_pic_sets; i++) {
    short_term_ref_pic_set_copy(r, w, sps.short_term_ref_pic_sets, i, num_short_term_ref_pic_sets); // short_term_ref_pic_set(i)
  }

  if (w.copy_bits(1, r) == 1) { // long_term_ref_pics_present_flag
    unsigned int num_long_term_ref_pic_sets = gecopy(r, w); // num_long_term_ref_pic_sets
    for (i = 0; i < num_long_term_ref_pic_sets; i++) {
      w.copy_bits(sps.log2_max_pic_order_cnt_lsb, r);  // lt_ref_pic_poc_lsb_sps[i]
      w.copy_bits(1, r);  // used_by_curr_pic_lt_sps_flag[i]
    }
  }

  w.copy_bits(1, r);  // sps_temporal_mvp_enabled_flag
  w.copy_bits(1, r);  // strong_intra_smoothing_enabled_flag

  sps.vui_present = w.copy_bits(1, r); // vui_parameters_present_flag
  if (sps.vui_present == 1) {
    vui_parameters_copy(r, w, sps, keep_ar_info, sps.max_sub_layers_minus1);  //vui_parameters()
  }

  if (w.copy_bits(1, r) == 1) // sps_extension_flag
    while (r.get_remaining_bits())
      w.copy_bits(1, r);  // sps_extension_data_flag

  w.put_bit(1);
  w.byte_align();

  buffer = mcptr_newsps;
  buffer->set_size(w.get_bit_position() / 8);

  sps.checksum = calc_adler32(buffer->get_buffer(), buffer->get_size());

  return true;
}

bool
hevc::parse_pps(memory_cptr &buffer,
                pps_info_t &pps) {
  try {
    bit_reader_c r(buffer->get_buffer(), buffer->get_size());

    memset(&pps, 0, sizeof(pps));

    r.skip_bits(1);             // forbidden_zero_bit
    if (r.get_bits(6) != 34)    // nal_unit_type
      return false;
    r.skip_bits(6);             // nuh_reserved_zero_6bits
    r.skip_bits(3);             // nuh_temporal_id_plus1

    pps.id     = geread(r);     // pps_pic_parameter_set_id
    pps.sps_id = geread(r);     // pps_seq_parameter_set_id
    pps.dependent_slice_segments_enabled_flag = r.get_bits(1);  // dependent_slice_segments_enabled_flag
    pps.output_flag_present_flag = r.get_bits(1);  // output_flag_present_flag
    pps.num_extra_slice_header_bits = r.get_bits(3);  // num_extra_slice_header_bits

    pps.checksum          = calc_adler32(buffer->get_buffer(), buffer->get_size());

    return true;
  } catch (...) {
    return false;
  }
}

bool
hevc::handle_sei_payload(mm_mem_io_c &byte_reader,
                         unsigned int sei_payload_type,
                         unsigned int sei_payload_size,
                         sei_info_t &sei) {
  // See HEVC spec, A.2.1
  // DivXID uses is user_data_unregistered
  if(sei_payload_type == 5) {
    memset(&sei, 0, sizeof(sei));

    byte_reader.read(&sei.divx_uuid, 16);        // divx_uuid
    byte_reader.read(&sei.divx_code, 9);         // divx_code
    byte_reader.read(&sei.divx_message_type, 1); // divx_message_type

    // Is there payload data?
    unsigned int payload_size = sei_payload_size - 26;
    if(payload_size) {
      sei.divx_payload_size = payload_size;
      sei.divx_payload = (unsigned char*) malloc(payload_size);
      byte_reader.read(sei.divx_payload, payload_size);
    }

    return true;
  }
  else {
      byte_reader.skip(sei_payload_size);
  }

  return false;
}

// HEVC spec, 7.3.2.4
bool
hevc::parse_sei(memory_cptr &buffer,
                sei_info_t &sei) {
  try {
    mm_mem_io_c byte_reader{*buffer};

    unsigned int bytes_read = 0;
    unsigned int buffer_size = buffer->get_size();
    unsigned int payload_type = 0;
    unsigned int payload_size = 0;

    unsigned char *p = buffer->get_buffer();
    p = p;

    byte_reader.skip(2); // skip the nalu header
    bytes_read+=2;

    while(bytes_read < buffer_size-2) {
      payload_type = 0;

      unsigned int i = byte_reader.read_uint8();
      bytes_read++;

      while(i == 0xFF) {
        payload_type += 255;
        i = byte_reader.read_uint8();
        bytes_read++;
      }
      payload_type += i;

      payload_size = 0;

      i = byte_reader.read_uint8();
      bytes_read++;

      while(i == 0xFF) {
        payload_size += 255;
        i = byte_reader.read_uint8();
        bytes_read++;
      }
      payload_size += i;

      handle_sei_payload(byte_reader, payload_type, payload_size, sei);

      bytes_read += payload_size;
    }

    return true;
  } catch (...) {
    return false;
  }
}

/** Extract the pixel aspect ratio from the HEVC codec data

   This function searches a buffer containing the HEVC
   codec initialization for the pixel aspectc ratio. If it is found
   then the numerator and the denominator are extracted, and the
   aspect ratio information is removed from the buffer. A structure
   containing the new buffer, the numerator/denominator and the
   success status is returned.

   \param buffer The buffer containing the HEVC codec data.

   \return A \c par_extraction_t structure.
*/
hevc::par_extraction_t
hevc::extract_par(memory_cptr const &buffer) {
  static debugging_option_c s_debug_ar{"extract_par|hevc_sps|sps_aspect_ratio"};

  try {
    auto hevcc     = hevcc_c::unpack(buffer);
    auto new_hevcc = hevcc;
    bool ar_found = false;
    unsigned int par_num = 1;
    unsigned int par_den = 1;

    new_hevcc.m_sps_list.clear();

    for (auto &nalu : hevcc.m_sps_list) {
      if (!ar_found) {
        nalu_to_rbsp(nalu);

        try {
          sps_info_t sps_info;
          if (hevc::parse_sps(nalu, sps_info, new_hevcc.m_vps_info_list)) {
            if (s_debug_ar)
              sps_info.dump();

            ar_found = sps_info.ar_found;
            if (ar_found) {
              par_num = sps_info.par_num;
              par_den = sps_info.par_den;
            }
          }
        } catch (mtx::mm_io::end_of_file_x &) {
        }

        rbsp_to_nalu(nalu);
      }

      new_hevcc.m_sps_list.push_back(nalu);
    }

    if (!new_hevcc)
      return par_extraction_t{buffer, 0, 0, false};

    return par_extraction_t{new_hevcc.pack(), ar_found ? par_num : 0, ar_found ? par_den : 0, true};

  } catch(...) {
    return par_extraction_t{buffer, 0, 0, false};
  }
}

bool
hevc::is_hevc_fourcc(const char *fourcc) {
  return !strncasecmp(fourcc, "hevc",  4);
}

memory_cptr
hevc::hevcc_to_nalus(const unsigned char *buffer,
                          size_t size) {
  try {
    if (6 > size)
      throw false;

    uint32_t marker = get_uint32_be(buffer);
    if (((marker & 0xffffff00) == 0x00000100) || (0x00000001 == marker))
      return memory_c::clone(buffer, size);

    mm_mem_io_c mem(buffer, size);
    byte_buffer_c nalus(size * 2);

    if (0x01 != mem.read_uint8())
      throw false;

    mem.setFilePointer(4, seek_beginning);
    size_t nal_size_size = 1 + (mem.read_uint8() & 3);
    if (2 > nal_size_size)
      throw false;

    size_t sps_or_pps;
    for (sps_or_pps = 0; 2 > sps_or_pps; ++sps_or_pps) {
      unsigned int num = mem.read_uint8();
      if (0 == sps_or_pps)
        num &= 0x1f;

      size_t i;
      for (i = 0; num > i; ++i) {
        uint16_t element_size   = mem.read_uint16_be();
        memory_cptr copy_buffer = memory_c::alloc(element_size + 4);
        if (element_size != mem.read(copy_buffer->get_buffer() + 4, element_size))
          throw false;

        put_uint32_be(copy_buffer->get_buffer(), NALU_START_CODE);
        nalus.add(copy_buffer->get_buffer(), element_size + 4);
      }
    }

    if (mem.getFilePointer() == size)
      return memory_c::clone(nalus.get_buffer(), nalus.get_size());

  } catch (...) {
  }

  return memory_cptr{};
}

hevc::hevc_es_parser_c::hevc_es_parser_c()
  : m_nalu_size_length(4)
  , m_keep_ar_info(true)
  , m_hevcc_ready(false)
  , m_hevcc_changed(false)
  , m_stream_default_duration(-1)
  , m_forced_default_duration(-1)
  , m_container_default_duration(-1)
  , m_frame_number(0)
  , m_num_skipped_frames(0)
  , m_first_keyframe_found(false)
  , m_recovery_point_valid(false)
  , m_b_frames_since_keyframe(false)
  , m_par_found(false)
  , m_max_timecode(0)
  , m_stream_position(0)
  , m_parsed_position(0)
  , m_have_incomplete_frame(false)
  , m_ignore_nalu_size_length_errors(false)
  , m_discard_actual_frames(false)
  , m_debug_keyframe_detection(debugging_c::requested("hevc_parser|hevc_keyframe_detection"))
  , m_debug_nalu_types(        debugging_c::requested("hevc_parser|hevc_nalu_types"))
  , m_debug_timecodes(         debugging_c::requested("hevc_parser|hevc_timecodes"))
  , m_debug_sps_info(          debugging_c::requested("hevc_parser|hevc_sps|hevc_sps_info"))
{
  if (m_debug_nalu_types)
    init_nalu_names();
}

hevc::hevc_es_parser_c::~hevc_es_parser_c() {
  mxdebug_if(debugging_c::requested("hevc_statistics"),
             boost::format("HEVC statistics: #frames: out %1% discarded %2% #timecodes: in %3% generated %4% discarded %5% num_fields: %6% num_frames: %7%\n")
             % m_stats.num_frames_out % m_stats.num_frames_discarded % m_stats.num_timecodes_in % m_stats.num_timecodes_generated % m_stats.num_timecodes_discarded
             % m_stats.num_field_slices % m_stats.num_frame_slices);

  mxdebug_if(m_debug_timecodes, boost::format("stream_position %1% parsed_position %2%\n") % m_stream_position % m_parsed_position);

  if (!debugging_c::requested("hevc_num_slices_by_type"))
    return;

  static const char *s_type_names[] = {
    "B",  "P",  "I", "unknown"
  };

  int i;
  mxdebug("hevc: Number of slices by type:\n");
  for (i = 0; 2 >= i; ++i)
    if (0 != m_stats.num_slices_by_type[i])
      mxdebug(boost::format("  %1%: %2%\n") % s_type_names[i] % m_stats.num_slices_by_type[i]);
}

void
hevc::hevc_es_parser_c::discard_actual_frames(bool discard) {
  m_discard_actual_frames = discard;
}

void
hevc::hevc_es_parser_c::add_bytes(unsigned char *buffer,
                                  size_t size) {
  memory_slice_cursor_c cursor;
  int marker_size              = 0;
  int previous_marker_size     = 0;
  int previous_pos             = -1;
  uint64_t previous_parsed_pos = m_parsed_position;

  if (m_unparsed_buffer && (0 != m_unparsed_buffer->get_size()))
    cursor.add_slice(m_unparsed_buffer);
  cursor.add_slice(buffer, size);

  if (3 <= cursor.get_remaining_size()) {
    uint32_t marker =                               1 << 24
                    | (unsigned int)cursor.get_char() << 16
                    | (unsigned int)cursor.get_char() <<  8
                    | (unsigned int)cursor.get_char();

    while (1) {
      if (NALU_START_CODE == marker)
        marker_size = 4;
      else if (NALU_START_CODE == (marker & 0x00ffffff))
        marker_size = 3;

      if (0 != marker_size) {
        if (-1 != previous_pos) {
          int new_size = cursor.get_position() - marker_size - previous_pos - previous_marker_size;
          memory_cptr nalu(new memory_c(safemalloc(new_size), new_size, true));
          cursor.copy(nalu->get_buffer(), previous_pos + previous_marker_size, new_size);
          m_parsed_position = previous_parsed_pos + previous_pos;
          handle_nalu(nalu);
        }
        previous_pos         = cursor.get_position() - marker_size;
        previous_marker_size = marker_size;
        marker_size          = 0;
      }

      if (!cursor.char_available())
        break;

      marker <<= 8;
      marker  |= (unsigned int)cursor.get_char();
    }
  }

  if (-1 == previous_pos)
    previous_pos = 0;

  m_stream_position += size;
  m_parsed_position  = previous_parsed_pos + previous_pos;

  int new_size = cursor.get_size() - previous_pos;
  if (0 != new_size) {
    m_unparsed_buffer = memory_cptr(new memory_c(safemalloc(new_size), new_size, true));
    cursor.copy(m_unparsed_buffer->get_buffer(), previous_pos, new_size);

  } else
    m_unparsed_buffer.reset();
}

void
hevc::hevc_es_parser_c::flush() {
  if (m_unparsed_buffer && (5 <= m_unparsed_buffer->get_size())) {
    m_parsed_position += m_unparsed_buffer->get_size();
    int marker_size = get_uint32_be(m_unparsed_buffer->get_buffer()) == NALU_START_CODE ? 4 : 3;
    handle_nalu(memory_c::clone(m_unparsed_buffer->get_buffer() + marker_size, m_unparsed_buffer->get_size() - marker_size));
  }

  m_unparsed_buffer.reset();
  if (m_have_incomplete_frame) {
    m_frames.push_back(m_incomplete_frame);
    m_have_incomplete_frame = false;
  }

  cleanup();
}

void
hevc::hevc_es_parser_c::add_timecode(int64_t timecode) {
  m_provided_timecodes.push_back(timecode);
  m_provided_stream_positions.push_back(m_stream_position);
  ++m_stats.num_timecodes_in;
}

void
hevc::hevc_es_parser_c::write_nalu_size(unsigned char *buffer,
                                        size_t size,
                                        int this_nalu_size_length)
  const {
  unsigned int nalu_size_length = -1 == this_nalu_size_length ? m_nalu_size_length : this_nalu_size_length;

  if (!m_ignore_nalu_size_length_errors && (size >= ((uint64_t)1 << (nalu_size_length * 8)))) {
    unsigned int required_bytes = nalu_size_length + 1;
    while (size >= (1u << (required_bytes * 8)))
      ++required_bytes;

    throw nalu_size_length_x(required_bytes);
  }

  unsigned int i;
  for (i = 0; i < nalu_size_length; i++)
    buffer[i] = (size >> (8 * (nalu_size_length - 1 - i))) & 0xff;
}

void
hevc::hevc_es_parser_c::flush_incomplete_frame() {
  if (!m_have_incomplete_frame || !m_hevcc_ready)
    return;

  m_frames.push_back(m_incomplete_frame);
  m_incomplete_frame.clear();
  m_have_incomplete_frame = false;
}

void
hevc::hevc_es_parser_c::flush_unhandled_nalus() {
  std::deque<memory_cptr>::iterator nalu = m_unhandled_nalus.begin();

  while (m_unhandled_nalus.end() != nalu) {
    handle_nalu(*nalu);
    nalu++;
  }

  m_unhandled_nalus.clear();
}

void
hevc::hevc_es_parser_c::handle_slice_nalu(memory_cptr &nalu) {
  if (!m_hevcc_ready) {
    m_unhandled_nalus.push_back(nalu);
    return;
  }

  slice_info_t si;
  if (!parse_slice(nalu, si))
    return;

  if (m_have_incomplete_frame && si.first_slice_segment_in_pic_flag)
    flush_incomplete_frame();

  if (m_have_incomplete_frame) {
    memory_c &mem = *(m_incomplete_frame.m_data.get());
    int offset    = mem.get_size();
    mem.resize(offset + m_nalu_size_length + nalu->get_size());
    write_nalu_size(mem.get_buffer() + offset, nalu->get_size());
    memcpy(mem.get_buffer() + offset + m_nalu_size_length, nalu->get_buffer(), nalu->get_size());

    return;
  }

  bool is_i_slice =  (HEVC_SLICE_TYPE_I == si.type);
  bool is_b_slice =  (HEVC_SLICE_TYPE_B == si.type);

  m_incomplete_frame.m_si       =  si;
  m_incomplete_frame.m_keyframe =  m_recovery_point_valid
                                || (   is_i_slice
                                    && (   (m_debug_keyframe_detection && !m_b_frames_since_keyframe)
                                        || (HEVC_NALU_TYPE_IDR_W_RADL == si.nalu_type)
                                        || (HEVC_NALU_TYPE_CRA_NUT    == si.nalu_type)));
  m_recovery_point_valid        =  false;

  if (m_incomplete_frame.m_keyframe) {
    m_first_keyframe_found    = true;
    m_b_frames_since_keyframe = false;
    cleanup();

  } else
    m_b_frames_since_keyframe |= is_b_slice;

  m_incomplete_frame.m_data = create_nalu_with_size(nalu, true);
  m_have_incomplete_frame   = true;

  if (!m_provided_stream_positions.empty() && (m_parsed_position >= m_provided_stream_positions.front())) {
    m_incomplete_frame.m_has_provided_timecode = true;
    m_provided_stream_positions.pop_front();
  }

  ++m_frame_number;
}

void
hevc::hevc_es_parser_c::handle_vps_nalu(memory_cptr &nalu) {
  vps_info_t vps_info;

  nalu_to_rbsp(nalu);
  if (!parse_vps(nalu, vps_info))
    return;
  rbsp_to_nalu(nalu);

  size_t i;
  for (i = 0; m_vps_info_list.size() > i; ++i)
    if (m_vps_info_list[i].id == vps_info.id)
      break;

  if (m_vps_info_list.size() == i) {
    m_vps_list.push_back(nalu);
    m_vps_info_list.push_back(vps_info);
    m_hevcc_changed = true;

  } else if (m_vps_info_list[i].checksum != vps_info.checksum) {
    mxverb(2, boost::format("hevc: VPS ID %|1$04x| changed; checksum old %|2$04x| new %|3$04x|\n") % vps_info.id % m_vps_info_list[i].checksum % vps_info.checksum);

    m_vps_info_list[i] = vps_info;
    m_vps_list[i]      = nalu;
    m_hevcc_changed     = true;
  }

  m_extra_data.push_back(create_nalu_with_size(nalu));
}

void
hevc::hevc_es_parser_c::handle_sps_nalu(memory_cptr &nalu) {
  sps_info_t sps_info;

  nalu_to_rbsp(nalu);
  if (!parse_sps(nalu, sps_info, m_vps_info_list, m_keep_ar_info))
    return;
  rbsp_to_nalu(nalu);

  size_t i;
  for (i = 0; m_sps_info_list.size() > i; ++i)
    if (m_sps_info_list[i].id == sps_info.id)
      break;

  bool use_sps_info = true;
  if (m_sps_info_list.size() == i) {
    m_sps_list.push_back(nalu);
    m_sps_info_list.push_back(sps_info);
    m_hevcc_changed = true;

  } else if (m_sps_info_list[i].checksum != sps_info.checksum) {
    mxverb(2, boost::format("hevc: SPS ID %|1$04x| changed; checksum old %|2$04x| new %|3$04x|\n") % sps_info.id % m_sps_info_list[i].checksum % sps_info.checksum);

    m_sps_info_list[i] = sps_info;
    m_sps_list[i]      = nalu;
    m_hevcc_changed     = true;

  } else
    use_sps_info = false;

  m_extra_data.push_back(create_nalu_with_size(nalu));

  if (use_sps_info && m_debug_sps_info)
    sps_info.dump();

  if (!use_sps_info)
    return;

  if (   !has_stream_default_duration()
      && sps_info.timing_info_valid()) {
    m_stream_default_duration = sps_info.default_duration();
    mxdebug_if(m_debug_timecodes, boost::format("Stream default duration: %1%\n") % m_stream_default_duration);
  }

  if (   !m_par_found
      && sps_info.ar_found
      && (0 != sps_info.par_den)) {
    m_par_found = true;
    m_par       = int64_rational_c(sps_info.par_num, sps_info.par_den);
  }
}

void
hevc::hevc_es_parser_c::handle_pps_nalu(memory_cptr &nalu) {
  pps_info_t pps_info;

  nalu_to_rbsp(nalu);
  if (!parse_pps(nalu, pps_info))
    return;
  rbsp_to_nalu(nalu);

  size_t i;
  for (i = 0; m_pps_info_list.size() > i; ++i)
    if (m_pps_info_list[i].id == pps_info.id)
      break;

  if (m_pps_info_list.size() == i) {
    m_pps_list.push_back(nalu);
    m_pps_info_list.push_back(pps_info);
    m_hevcc_changed = true;

  } else if (m_pps_info_list[i].checksum != pps_info.checksum) {
    mxverb(2, boost::format("hevc: PPS ID %|1$04x| changed; checksum old %|2$04x| new %|3$04x|\n") % pps_info.id % m_pps_info_list[i].checksum % pps_info.checksum);

    m_pps_info_list[i] = pps_info;
    m_pps_list[i]      = nalu;
    m_hevcc_changed     = true;
  }

  m_extra_data.push_back(create_nalu_with_size(nalu));
}

void
hevc::hevc_es_parser_c::handle_sei_nalu(memory_cptr &nalu) {
  sei_info_t sei_info;

  nalu_to_rbsp(nalu);
  if (!parse_sei(nalu, sei_info))
    return;
  rbsp_to_nalu(nalu);

  m_sei_list.push_back(nalu);
  m_sei_info_list.push_back(sei_info);
  m_hevcc_changed = true;

  m_extra_data.push_back(create_nalu_with_size(nalu));
}

void
hevc::hevc_es_parser_c::handle_nalu(memory_cptr nalu) {
  if (1 > nalu->get_size())
    return;

  int type = (*(nalu->get_buffer()) >> 1) & 0x3F;

  mxdebug_if(m_debug_nalu_types, boost::format("NALU type 0x%|1$02x| (%2%) size %3%\n") % type % get_nalu_type_name(type) % nalu->get_size());

  switch (type) {
   case HEVC_NALU_TYPE_VIDEO_PARAM:
      flush_incomplete_frame();
      handle_vps_nalu(nalu);
      break;

   case HEVC_NALU_TYPE_SEQ_PARAM:
      flush_incomplete_frame();
      handle_sps_nalu(nalu);
      break;

    case HEVC_NALU_TYPE_PIC_PARAM:
      flush_incomplete_frame();
      handle_pps_nalu(nalu);
      break;

    case HEVC_NALU_TYPE_END_OF_SEQ:
    case HEVC_NALU_TYPE_END_OF_STREAM:
    case HEVC_NALU_TYPE_ACCESS_UNIT:
      flush_incomplete_frame();
      break;

    case HEVC_NALU_TYPE_FILLER_DATA:
      // Skip these.
      break;

    case HEVC_NALU_TYPE_TRAIL_N:
    case HEVC_NALU_TYPE_TRAIL_R:
    case HEVC_NALU_TYPE_TSA_N:
    case HEVC_NALU_TYPE_TSA_R:
    case HEVC_NALU_TYPE_STSA_N:
    case HEVC_NALU_TYPE_STSA_R:
    case HEVC_NALU_TYPE_RADL_N:
    case HEVC_NALU_TYPE_RADL_R:
    case HEVC_NALU_TYPE_RASL_N:
    case HEVC_NALU_TYPE_RASL_R:
    case HEVC_NALU_TYPE_BLA_W_LP:
    case HEVC_NALU_TYPE_BLA_W_RADL:
    case HEVC_NALU_TYPE_BLA_N_LP:
    case HEVC_NALU_TYPE_IDR_W_RADL:
    case HEVC_NALU_TYPE_IDR_N_LP:
    case HEVC_NALU_TYPE_CRA_NUT:
      if (!m_hevcc_ready && !m_vps_info_list.empty() && !m_sps_info_list.empty() && !m_pps_info_list.empty()) {
        m_hevcc_ready = true;
        flush_unhandled_nalus();
      }
      handle_slice_nalu(nalu);
      break;

    default:
      flush_incomplete_frame();
      if (!m_hevcc_ready && !m_vps_info_list.empty() && !m_sps_info_list.empty() && !m_pps_info_list.empty()) {
        m_hevcc_ready = true;
        flush_unhandled_nalus();
      }
      m_extra_data.push_back(create_nalu_with_size(nalu));

      if (HEVC_NALU_TYPE_PREFIX_SEI == type)
        handle_sei_nalu(nalu);

      break;
  }
}

bool
hevc::hevc_es_parser_c::parse_slice(memory_cptr &buffer,
                                    slice_info_t &si) {
  try {
    bit_reader_c r(buffer->get_buffer(), buffer->get_size());
    unsigned int i;

    memset(&si, 0, sizeof(si));

    r.get_bits(1);                // forbidden_zero_bit
    si.nalu_type = r.get_bits(6); // nal_unit_type
    r.get_bits(6);                // nuh_reserved_zero_6bits
    r.get_bits(3);                // nuh_temporal_id_plus1

    bool RapPicFlag = (si.nalu_type >= 16 && si.nalu_type <= 23); // RapPicFlag
    si.first_slice_segment_in_pic_flag = r.get_bits(1); // first_slice_segment_in_pic_flag

    if (RapPicFlag)
      r.get_bits(1);  // no_output_of_prior_pics_flag

    si.pps_id = geread(r);  // slice_pic_parameter_set_id

    size_t pps_idx;
    for (pps_idx = 0; m_pps_info_list.size() > pps_idx; ++pps_idx)
      if (m_pps_info_list[pps_idx].id == si.pps_id)
        break;
    if (m_pps_info_list.size() == pps_idx) {
      mxverb(3, boost::format("slice parser error: PPS not found: %1%\n") % si.pps_id);
      return false;
    }

    pps_info_t &pps = m_pps_info_list[pps_idx];
    size_t sps_idx;
    for (sps_idx = 0; m_sps_info_list.size() > sps_idx; ++sps_idx)
      if (m_sps_info_list[sps_idx].id == pps.sps_id)
        break;
    if (m_sps_info_list.size() == sps_idx)
      return false;

    si.sps = sps_idx;
    si.pps = pps_idx;

    sps_info_t &sps = m_sps_info_list[sps_idx];

    bool dependent_slice_segment_flag = false;
    if (!si.first_slice_segment_in_pic_flag) {
      if (pps.dependent_slice_segments_enabled_flag)
        dependent_slice_segment_flag = r.get_bits(1); // dependent_slice_segment_flag

      bool Log2MinCbSizeY = sps.log2_min_luma_coding_block_size_minus3 + 3;
      bool Log2CtbSizeY = Log2MinCbSizeY + sps.log2_diff_max_min_luma_coding_block_size;
      bool CtbSizeY = 1 << Log2CtbSizeY;
      bool PicWidthInCtbsY = ceil(sps.width / CtbSizeY);
      bool PicHeightInCtbsY = ceil(sps.height / CtbSizeY);
      bool PicSizeInCtbsY = PicWidthInCtbsY * PicHeightInCtbsY;

      unsigned int v = ceil(int_log2(PicSizeInCtbsY));
      r.get_bits(v);  // slice_segment_address
    }

    if (!dependent_slice_segment_flag) {
      for (i = 0; i < pps.num_extra_slice_header_bits; i++)
        r.get_bits(1);  // slice_reserved_undetermined_flag[i]

      si.type = geread(r);  // slice_type

      if (pps.output_flag_present_flag)
          r.get_bits(1);    // pic_output_flag

      if (sps.separate_colour_plane_flag == 1)
          r.get_bits(1);    // colour_plane_id

      if ( (si.nalu_type != HEVC_NALU_TYPE_IDR_W_RADL) && (si.nalu_type != HEVC_NALU_TYPE_IDR_N_LP) ) {
          si.pic_order_cnt_lsb = r.get_bits(sps.log2_max_pic_order_cnt_lsb); // slice_pic_order_cnt_lsb
      }

      ++m_stats.num_slices_by_type[1 < si.type ? 2 : si.type];
    }

    return true;
  } catch (...) {
    return false;
  }
}

int64_t
hevc::hevc_es_parser_c::duration_for(slice_info_t const &si)
  const {
  int64_t duration = -1 != m_forced_default_duration                                                  ? m_forced_default_duration * 2
                   : (m_sps_info_list.size() > si.sps) && m_sps_info_list[si.sps].timing_info_valid() ? m_sps_info_list[si.sps].default_duration()
                   : -1 != m_stream_default_duration                                                  ? m_stream_default_duration * 2
                   : -1 != m_container_default_duration                                               ? m_container_default_duration * 2
                   :                                                                                    20000000 * 2;
  return duration;
}

int64_t
hevc::hevc_es_parser_c::get_most_often_used_duration()
  const {
  int64_t const s_common_default_durations[] = {
    1000000000ll / 50,
    1000000000ll / 25,
    1000000000ll / 60,
    1000000000ll / 30,
    1000000000ll * 1001 / 48000,
    1000000000ll * 1001 / 24000,
    1000000000ll * 1001 / 60000,
    1000000000ll * 1001 / 30000,
  };

  auto most_often = m_duration_frequency.begin();
  for (auto current = m_duration_frequency.begin(); m_duration_frequency.end() != current; ++current)
    if (current->second > most_often->second)
      most_often = current;

  // No duration at all!? No frame?
  if (m_duration_frequency.end() == most_often) {
    mxdebug_if(m_debug_timecodes, boost::format("Duration frequency: none found, using 25 FPS\n"));
    return 1000000000ll / 25;
  }

  auto best = std::make_pair(most_often->first, std::numeric_limits<uint64_t>::max());

  for (auto common_default_duration : s_common_default_durations) {
    uint64_t diff = std::abs(most_often->first - common_default_duration);
    if ((diff < 20000) && (diff < best.second))
      best = std::make_pair(common_default_duration, diff);
  }

  mxdebug_if(m_debug_timecodes, boost::format("Duration frequency. Result: %1%, diff %2%. Best before adjustment: %3%. All: %4%\n")
             % best.first % best.second % most_often->first
             % boost::accumulate(m_duration_frequency, std::string(""), [](std::string const &accu, std::pair<int64_t, int64_t> const &pair) {
                 return accu + (boost::format(" <%1% %2%>") % pair.first % pair.second).str();
               }));

  return best.first;
}

void
hevc::hevc_es_parser_c::cleanup() {
  if (m_frames.empty())
    return;

  if (m_discard_actual_frames) {
    m_stats.num_frames_discarded    += m_frames.size();
    m_stats.num_timecodes_discarded += m_provided_timecodes.size();

    m_frames.clear();
    m_provided_timecodes.clear();
    m_provided_stream_positions.clear();

    return;
  }

  auto frames_begin = m_frames.begin();
  auto frames_end   = m_frames.end();
  auto frame_itr    = frames_begin;

  // This may be wrong but is needed for mkvmerge to work correctly
  // (cluster_helper etc).
  frame_itr->m_keyframe = true;

  slice_info_t &idr         = frame_itr->m_si;
  sps_info_t &sps           = m_sps_info_list[idr.sps];
  bool simple_picture_order = false;

  unsigned int idx                    = 0;
  unsigned int prev_pic_order_cnt_msb = 0;
  unsigned int prev_pic_order_cnt_lsb = 0;

  while (frames_end != frame_itr) {
    slice_info_t &si = frame_itr->m_si;

    if (si.sps != idr.sps) {
      simple_picture_order = true;
      break;
    }

    if ((HEVC_NALU_TYPE_IDR_W_RADL == idr.type) || (HEVC_NALU_TYPE_CRA_NUT == idr.type)) {
      frame_itr->m_presentation_order = 0;
      prev_pic_order_cnt_lsb = prev_pic_order_cnt_msb = 0;
    } else {
      unsigned int poc_msb;
      unsigned int max_poc_lsb = 1 << (sps.log2_max_pic_order_cnt_lsb);
      unsigned int poc_lsb = si.pic_order_cnt_lsb;

      if (poc_lsb < prev_pic_order_cnt_lsb && (prev_pic_order_cnt_lsb - poc_lsb) >= (max_poc_lsb / 2))
          poc_msb = prev_pic_order_cnt_msb + max_poc_lsb;
      else if (poc_lsb > prev_pic_order_cnt_lsb && (poc_lsb - prev_pic_order_cnt_lsb) > (max_poc_lsb / 2))
          poc_msb = prev_pic_order_cnt_msb - max_poc_lsb;
      else
          poc_msb = prev_pic_order_cnt_msb;

      frame_itr->m_presentation_order = poc_lsb + poc_msb;

      if (si.type != HEVC_SLICE_TYPE_B) {  // is this accurate?
          prev_pic_order_cnt_lsb = poc_lsb;
          prev_pic_order_cnt_msb = poc_msb;
      }
    }

    frame_itr->m_decode_order       = idx;

    ++frame_itr;
    ++idx;
  }

  if (!simple_picture_order)
    brng::sort(m_frames, [](const hevc_frame_t &f1, const hevc_frame_t &f2) { return f1.m_presentation_order < f2.m_presentation_order; });

  brng::sort(m_provided_timecodes);

  frames_begin               = m_frames.begin();
  frames_end                 = m_frames.end();
  auto previous_frame_itr    = frames_begin;
  auto provided_timecode_itr = m_provided_timecodes.begin();
  for (frame_itr = frames_begin; frames_end != frame_itr; ++frame_itr) {
    if (frame_itr->m_has_provided_timecode) {
      frame_itr->m_start = *provided_timecode_itr;
      ++provided_timecode_itr;

      if (frames_begin != frame_itr)
        previous_frame_itr->m_end = frame_itr->m_start;

    } else {
      frame_itr->m_start = frames_begin == frame_itr ? m_max_timecode : previous_frame_itr->m_end;
      ++m_stats.num_timecodes_generated;
    }

    if (frame_itr->m_start == 1033333333)
      mxinfo("muh\n");
    frame_itr->m_end = frame_itr->m_start + duration_for(frame_itr->m_si);

    previous_frame_itr = frame_itr;
  }

  m_max_timecode = m_frames.back().m_end;
  m_provided_timecodes.erase(m_provided_timecodes.begin(), provided_timecode_itr);

  mxdebug_if(m_debug_timecodes, boost::format("CLEANUP frames <pres_ord dec_ord has_prov_tc tc dur>: %1%\n")
             % boost::accumulate(m_frames, std::string(""), [](std::string const &accu, hevc_frame_t const &frame) {
                 return accu + (boost::format(" <%1% %2% %3% %4% %5%>") % frame.m_presentation_order % frame.m_decode_order % frame.m_has_provided_timecode % frame.m_start % (frame.m_end - frame.m_start)).str();
               }));


  if (!simple_picture_order)
    brng::sort(m_frames, [](const hevc_frame_t &f1, const hevc_frame_t &f2) { return f1.m_decode_order < f2.m_decode_order; });

  frames_begin       = m_frames.begin();
  frames_end         = m_frames.end();
  previous_frame_itr = frames_begin;
  for (frame_itr = frames_begin; frames_end != frame_itr; ++frame_itr) {
    if (frames_begin != frame_itr)
      frame_itr->m_ref1 = previous_frame_itr->m_start - frame_itr->m_start;

    previous_frame_itr = frame_itr;
    m_duration_frequency[frame_itr->m_end - frame_itr->m_start]++;

    ++m_stats.num_field_slices;
  }

  m_stats.num_frames_out += m_frames.size();
  m_frames_out.insert(m_frames_out.end(), frames_begin, frames_end);
  m_frames.clear();
}

memory_cptr
hevc::hevc_es_parser_c::create_nalu_with_size(const memory_cptr &src,
                                              bool add_extra_data) {
  int final_size = m_nalu_size_length + src->get_size(), offset = 0, size;
  unsigned char *buffer;

  if (add_extra_data) {
    for (auto &mem : m_extra_data)
      final_size += mem->get_size();
    buffer = (unsigned char *)safemalloc(final_size);

    for (auto &mem : m_extra_data) {
      memcpy(buffer + offset, mem->get_buffer(), mem->get_size());
      offset += mem->get_size();
    }

    m_extra_data.clear();
  } else
    buffer = (unsigned char *)safemalloc(final_size);

  size = src->get_size();
  write_nalu_size(buffer + offset, size);
  memcpy(buffer + offset + m_nalu_size_length, src->get_buffer(), size);

  return memory_cptr(new memory_c(buffer, final_size, true));
}

memory_cptr
hevc::hevc_es_parser_c::get_hevcc()
  const {
  return hevcc_c{static_cast<unsigned int>(m_nalu_size_length), m_vps_list, m_sps_list, m_pps_list}.pack();
}

bool
hevc::hevc_es_parser_c::has_par_been_found()
  const {
  assert(m_hevcc_ready);
  return m_par_found;
}

int64_rational_c const &
hevc::hevc_es_parser_c::get_par()
  const {
  assert(m_hevcc_ready && m_par_found);
  return m_par;
}

std::pair<int64_t, int64_t> const
hevc::hevc_es_parser_c::get_display_dimensions(int width,
                                               int height)
  const {
  assert(m_hevcc_ready && m_par_found);

  if (0 >= width)
    width = get_width();
  if (0 >= height)
    height = get_height();

  return std::make_pair<int64_t, int64_t>(1 <= m_par ? irnd(width * boost::rational_cast<double>(m_par)) : width,
                                          1 <= m_par ? height                                            : irnd(height / boost::rational_cast<double>(m_par)));
}

size_t
hevc::hevc_es_parser_c::get_num_field_slices()
  const {
  return m_stats.num_field_slices;
}

size_t
hevc::hevc_es_parser_c::get_num_frame_slices()
  const {
  return m_stats.num_frame_slices;
}

void
hevc::hevc_es_parser_c::dump_info()
  const {
  mxinfo("Dumping m_frames_out:\n");
  for (auto &frame : m_frames_out) {
    mxinfo(boost::format("size %1% key %2% start %3% end %4% ref1 %5% adler32 0x%|6$08x|\n")
           % frame.m_data->get_size()
           % frame.m_keyframe
           % format_timecode(frame.m_start)
           % format_timecode(frame.m_end)
           % format_timecode(frame.m_ref1)
           % calc_adler32(frame.m_data->get_buffer(), frame.m_data->get_size()));
  }
}

std::string
hevc::hevc_es_parser_c::get_nalu_type_name(int type)
  const {
  auto name = m_nalu_names_by_type.find(type);
  return (m_nalu_names_by_type.end() == name) ? "unknown" : name->second;
}

void
hevc::hevc_es_parser_c::init_nalu_names() {
  // VCL NALs
  m_nalu_names_by_type[HEVC_NALU_TYPE_TRAIL_N] = "TRAIL_N";
  m_nalu_names_by_type[HEVC_NALU_TYPE_TRAIL_R] = "TRAIL_R";
  m_nalu_names_by_type[HEVC_NALU_TYPE_TSA_N] = "TSA_N";
  m_nalu_names_by_type[HEVC_NALU_TYPE_TSA_R] = "TSA_R";
  m_nalu_names_by_type[HEVC_NALU_TYPE_STSA_N] = "STSA_N";
  m_nalu_names_by_type[HEVC_NALU_TYPE_STSA_R] = "STSA_R";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RADL_N] = "RADL_N";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RADL_R] = "RADL_R";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RASL_N] = "RASL_N";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RASL_R] = "RASL_R";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_VCL_N10] = "RSV_VCL_N10";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_VCL_N12] = "RSV_VCL_N12";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_VCL_N14] = "RSV_VCL_N14";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_VCL_R11] = "RSV_VCL_R11";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_VCL_R13] = "RSV_VCL_R13";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_VCL_R15] = "RSV_VCL_R15";
  m_nalu_names_by_type[HEVC_NALU_TYPE_BLA_W_LP] = "BLA_W_LP";
  m_nalu_names_by_type[HEVC_NALU_TYPE_BLA_W_RADL] = "BLA_W_RADL";
  m_nalu_names_by_type[HEVC_NALU_TYPE_BLA_N_LP] = "BLA_N_LP";
  m_nalu_names_by_type[HEVC_NALU_TYPE_IDR_W_RADL] = "IDR_W_RADL";
  m_nalu_names_by_type[HEVC_NALU_TYPE_IDR_N_LP] = "IDR_N_LP";
  m_nalu_names_by_type[HEVC_NALU_TYPE_CRA_NUT] = "CRA_NUT";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_RAP_VCL22] = "RSV_RAP_VCL22";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_RAP_VCL23] = "RSV_RAP_VCL23";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_VCL24] = "RSV_VCL24";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_VCL25] = "RSV_VCL25";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_VCL26] = "RSV_VCL26";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_VCL27] = "RSV_VCL27";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_VCL28] = "RSV_VCL28";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_VCL29] = "RSV_VCL29";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_VCL30] = "RSV_VCL30";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_VCL31] = "RSV_VCL31";

  //Non-VCL NALs
  m_nalu_names_by_type[HEVC_NALU_TYPE_VIDEO_PARAM] = "VIDEO_PARAM";
  m_nalu_names_by_type[HEVC_NALU_TYPE_SEQ_PARAM] = "SEQ_PARAM";
  m_nalu_names_by_type[HEVC_NALU_TYPE_PIC_PARAM] = "PIC_PARAM";
  m_nalu_names_by_type[HEVC_NALU_TYPE_ACCESS_UNIT] = "ACCESS_UNIT";
  m_nalu_names_by_type[HEVC_NALU_TYPE_END_OF_SEQ] = "END_OF_SEQ";
  m_nalu_names_by_type[HEVC_NALU_TYPE_END_OF_STREAM] = "END_OF_STREAM";
  m_nalu_names_by_type[HEVC_NALU_TYPE_FILLER_DATA] = "FILLER_DATA";
  m_nalu_names_by_type[HEVC_NALU_TYPE_PREFIX_SEI] = "PREFIX_SEI";
  m_nalu_names_by_type[HEVC_NALU_TYPE_SUFFIX_SEI] = "SUFFIX_SEI";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_NVCL41] = "RSV_NVCL41";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_NVCL42] = "RSV_NVCL42";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_NVCL43] = "RSV_NVCL43";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_NVCL44] = "RSV_NVCL44";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_NVCL45] = "RSV_NVCL45";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_NVCL46] = "RSV_NVCL46";
  m_nalu_names_by_type[HEVC_NALU_TYPE_RSV_NVCL47] = "RSV_NVCL47";
  m_nalu_names_by_type[HEVC_NALU_TYPE_UNSPEC48] = "UNSPEC48";
  m_nalu_names_by_type[HEVC_NALU_TYPE_UNSPEC49] = "UNSPEC49";
  m_nalu_names_by_type[HEVC_NALU_TYPE_UNSPEC50] = "UNSPEC58";
  m_nalu_names_by_type[HEVC_NALU_TYPE_UNSPEC51] = "UNSPEC51";
  m_nalu_names_by_type[HEVC_NALU_TYPE_UNSPEC52] = "UNSPEC52";
  m_nalu_names_by_type[HEVC_NALU_TYPE_UNSPEC53] = "UNSPEC53";
  m_nalu_names_by_type[HEVC_NALU_TYPE_UNSPEC54] = "UNSPEC54";
  m_nalu_names_by_type[HEVC_NALU_TYPE_UNSPEC55] = "UNSPEC55";
  m_nalu_names_by_type[HEVC_NALU_TYPE_UNSPEC56] = "UNSPEC56";
  m_nalu_names_by_type[HEVC_NALU_TYPE_UNSPEC57] = "UNSPEC57";
  m_nalu_names_by_type[HEVC_NALU_TYPE_UNSPEC58] = "UNSPEC58";
  m_nalu_names_by_type[HEVC_NALU_TYPE_UNSPEC59] = "UNSPEC59";
  m_nalu_names_by_type[HEVC_NALU_TYPE_UNSPEC60] = "UNSPEC60";
  m_nalu_names_by_type[HEVC_NALU_TYPE_UNSPEC61] = "UNSPEC61";
  m_nalu_names_by_type[HEVC_NALU_TYPE_UNSPEC62] = "UNSPEC62";
  m_nalu_names_by_type[HEVC_NALU_TYPE_UNSPEC63] = "UNSPEC63";
}
