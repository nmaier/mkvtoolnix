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

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/bit_cursor.h"
#include "common/byte_buffer.h"
#include "common/checksums.h"
#include "common/endian.h"
#include "common/hacks.h"
#include "common/math.h"
#include "common/mm_io.h"
#include "common/mpeg4_p10.h"
#include "common/strings/formatting.h"

namespace mpeg4 {
  namespace p10 {
    struct poc_t {
      int poc, dec;
      int64_t timecode, duration;

      poc_t(int p, int d):
        poc(p), dec(d), timecode(0), duration(0) {
      };
    };

    static bool compare_poc_by_poc(const poc_t &poc1, const poc_t &poc2);
    static bool compare_poc_by_dec(const poc_t &poc1, const poc_t &poc2);

    static const struct {
      int numerator, denominator;
    } s_predefined_pars[AVC_NUM_PREDEFINED_PARS] = {
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
  };
};

static int
gecopy(bit_cursor_c &r,
       bit_writer_c &w) {
  int	n = 0, bit;

  while ((bit = r.get_bit()) == 0) {
    w.put_bit(0);
    ++n;
  }

  w.put_bit(1);

  bit = w.copy_bits(n, r);

  return (1 << n) - 1 + bit;
}

static int
geread(bit_cursor_c &r) {
  int	n = 0, bit;

  while ((bit = r.get_bit()) == 0)
    ++n;

  bit = r.get_bits(n);

  return (1 << n) - 1 + bit;
}

static int
sgeread(bit_cursor_c &r) {
  int v = geread(r);
  return v & 1 ? (v + 1) / 2 : -(v / 2);
}

static int
sgecopy(bit_cursor_c &r,
        bit_writer_c &w) {
  int v = gecopy(r, w);
  return v & 1 ? (v + 1) / 2 : -(v / 2);
}

static void
hrdcopy(bit_cursor_c &r,
        bit_writer_c &w) {
  int ncpb = gecopy(r,w);       // cpb_cnt_minus1
  w.copy_bits(4, r);            // bit_rate_scale
  w.copy_bits(4, r);            // cpb_size_scale
  for (int i = 0; i <= ncpb; ++i) {
    gecopy(r,w);                // bit_rate_value_minus1
    gecopy(r,w);                // cpb_size_value_minus1
    w.copy_bits(1, r);          // cbr_flag
  }
  w.copy_bits(5, r);            // initial_cpb_removal_delay_length_minus1
  w.copy_bits(5, r);            // cpb_removal_delay_length_minus1
  w.copy_bits(5, r);            // dpb_output_delay_length_minus1
  w.copy_bits(5, r);            // time_offset_length
}

static void
slcopy(bit_cursor_c &r,
       bit_writer_c &w,
       int size) {
  int delta, next = 8, j;

  for (j = 0; (j < size) && (0 != next); ++j) {
    delta = sgecopy(r, w);
    next  = (next + delta + 256) % 256;
  }
}

void
mpeg4::p10::sps_info_t::dump() {
  mxinfo(boost::format("sps_info dump:\n"
                       "  id:                                    %1%\n"
                       "  profile_idc:                           %2%\n"
                       "  profile_compat:                        %3%\n"
                       "  level_idc:                             %4%\n"
                       "  log2_max_frame_num:                    %5%\n"
                       "  pic_order_cnt_type:                    %6%\n"
                       "  log2_max_pic_order_cnt_lsb:            %7%\n"
                       "  offset_for_non_ref_pic:                %8%\n"
                       "  offset_for_top_to_bottom_field:        %9%\n"
                       "  num_ref_frames_in_pic_order_cnt_cycle: %10%\n"
                       "  delta_pic_order_always_zero_flag:      %11%\n"
                       "  frame_mbs_only:                        %12%\n"
                       "  vui_present:                           %13%\n"
                       "  ar_found:                              %14%\n"
                       "  par_num:                               %15%\n"
                       "  par_den:                               %16%\n"
                       "  timing_info_present:                   %17%\n"
                       "  num_units_in_tick:                     %18%\n"
                       "  time_scale:                            %19%\n"
                       "  fixed_frame_rate:                      %20%\n"
                       "  crop_left:                             %21%\n"
                       "  crop_top:                              %22%\n"
                       "  crop_right:                            %23%\n"
                       "  crop_bottom:                           %24%\n"
                       "  width:                                 %25%\n"
                       "  height:                                %26%\n"
                       "  checksum:                              %|27$08x|\n")
         % id
         % profile_idc
         % profile_compat
         % level_idc
         % log2_max_frame_num
         % pic_order_cnt_type
         % log2_max_pic_order_cnt_lsb
         % offset_for_non_ref_pic
         % offset_for_top_to_bottom_field
         % num_ref_frames_in_pic_order_cnt_cycle
         % delta_pic_order_always_zero_flag
         % frame_mbs_only
         % vui_present
         % ar_found
         % par_num
         % par_den
         % timing_info_present
         % num_units_in_tick
         % time_scale
         % fixed_frame_rate
         % crop_left
         % crop_top
         % crop_right
         % crop_bottom
         % width
         % height
         % checksum);
}

void
mpeg4::p10::pps_info_t::dump() {
  mxinfo(boost::format("pps_info dump:\n"
                       "id: %1%\n"
                       "sps_id: %2%\n"
                       "pic_order_present: %3%\n"
                       "checksum: %|4$08x|\n")
         % id
         % sps_id
         % pic_order_present
         % checksum);
}

void
mpeg4::p10::slice_info_t::dump() {
  mxinfo(boost::format("slice_info dump:\n"
                       "  nalu_type:                  %1%\n"
                       "  nal_ref_idc:                %2%\n"
                       "  type:                       %3%\n"
                       "  pps_id:                     %4%\n"
                       "  frame_num:                  %5%\n"
                       "  field_pic_flag:             %6%\n"
                       "  bottom_field_flag:          %7%\n"
                       "  idr_pic_id:                 %8%\n"
                       "  pic_order_cnt_lsb:          %9%\n"
                       "  delta_pic_order_cnt_bottom: %10%\n"
                       "  delta_pic_order_cnt:        %11%\n"
                       "  first_mb_in_slice:          %12%\n"
                       "  sps:                        %13%\n"
                       "  pps:                        %14%\n")
         % static_cast<unsigned int>(nalu_type)
         % static_cast<unsigned int>(nal_ref_idc)
         % static_cast<unsigned int>(type)
         % static_cast<unsigned int>(pps_id)
         % frame_num
         % field_pic_flag
         % bottom_field_flag
         % idr_pic_id
         % pic_order_cnt_lsb
         % delta_pic_order_cnt_bottom
         % (delta_pic_order_cnt[0] << 8 | delta_pic_order_cnt[1])
         % first_mb_in_slice
         % sps
         % pps);
}

void
mpeg4::p10::nalu_to_rbsp(memory_cptr &buffer) {
  int pos, size = buffer->get_size();
  mm_mem_io_c d(NULL, size, 100);
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
mpeg4::p10::rbsp_to_nalu(memory_cptr &buffer) {
  int pos, size = buffer->get_size();
  mm_mem_io_c d(NULL, size, 100);
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
mpeg4::p10::parse_sps(memory_cptr &buffer,
                      sps_info_t &sps,
                      bool keep_ar_info) {
  int size              = buffer->get_size();
  unsigned char *newsps = (unsigned char *)safemalloc(size + 100);
  memory_cptr mcptr_newsps(new memory_c(newsps, size, true));
  bit_cursor_c r(buffer->get_buffer(), size);
  bit_writer_c w(newsps, size);
  int i, nref, mb_width, mb_height;

  keep_ar_info = !hack_engaged(ENGAGE_REMOVE_BITSTREAM_AR_INFO);

  memset(&sps, 0, sizeof(sps));

  sps.par_num  = 1;
  sps.par_den  = 1;
  sps.ar_found = false;

  w.copy_bits(3, r);            // forbidden_zero_bit, nal_ref_idc
  if (w.copy_bits(5, r) != 7)   // nal_unit_type
    return false;
  sps.profile_idc    = w.copy_bits(8, r); // profile_idc
  sps.profile_compat = w.copy_bits(8, r); // constraints
  sps.level_idc      = w.copy_bits(8, r); // level_idc
  sps.id             = gecopy(r, w);      // sps id
  if (sps.profile_idc >= 100) {           // high profile
    if (gecopy(r, w) == 3)                // chroma_format_idc
      w.copy_bits(1, r);                  // residue_transform_flag
    gecopy(r, w);                         // bit_depth_luma_minus8
    gecopy(r, w);                         // bit_depth_chroma_minus8
    w.copy_bits(1, r);                    // qpprime_y_zero_transform_bypass_flag
    if (w.copy_bits(1, r) == 1)           // seq_scaling_matrix_present_flag
      for (i = 0; i < 8; ++i)
        if (w.copy_bits(1, r) == 1)       // seq_scaling_list_present_flag
          slcopy(r, w, i < 6 ? 16 : 64);
  }
  sps.log2_max_frame_num = gecopy(r, w) + 4; // log2_max_frame_num_minus4
  sps.pic_order_cnt_type = gecopy(r, w);     // pic_order_cnt_type
  switch (sps.pic_order_cnt_type) {
    case 0:
      sps.log2_max_pic_order_cnt_lsb = gecopy(r, w) + 4; // log2_max_pic_order_cnt_lsb_minus4
      break;

    case 1:
      sps.delta_pic_order_always_zero_flag      = w.copy_bits(1, r); // delta_pic_order_always_zero_flag
      sps.offset_for_non_ref_pic                = sgecopy(r, w);     // offset_for_non_ref_pic
      sps.offset_for_top_to_bottom_field        = sgecopy(r, w);     // offset_for_top_to_bottom_field
      nref                                      = gecopy(r, w);      // num_ref_frames_in_pic_order_cnt_cycle
      sps.num_ref_frames_in_pic_order_cnt_cycle = nref;
      for (i = 0; i < nref; ++i)
        gecopy(r, w);           // offset_for_ref_frame
      break;

    case 2:
      break;

    default:
      return false;
  }

  gecopy(r, w);                 // num_ref_frames
  w.copy_bits(1, r);            // gaps_in_frame_num_value_allowed_flag
  mb_width  = gecopy(r, w) + 1; // pic_width_in_mbs_minus1
  mb_height = gecopy(r, w) + 1; // pic_height_in_map_units_minus1
  if (w.copy_bits(1, r) == 0) { // frame_mbs_only_flag
    sps.frame_mbs_only = false;
    w.copy_bits(1, r);          // mb_adaptive_frame_field_flag
  } else
    sps.frame_mbs_only = true;

  sps.width  = mb_width * 16;
  sps.height = (2 - sps.frame_mbs_only) * mb_height * 16;

  w.copy_bits(1, r);            // direct_8x8_inference_flag
  if (w.copy_bits(1, r) == 1) {
    sps.crop_left   = gecopy(r, w); // frame_crop_left_offset
    sps.crop_right  = gecopy(r, w); // frame_crop_right_offset
    sps.crop_top    = gecopy(r, w); // frame_crop_top_offset
    sps.crop_bottom = gecopy(r, w); // frame_crop_bottom_offset

    sps.width   = 16 * mb_width - 2 * (sps.crop_left + sps.crop_right);
    sps.height -= (2 - sps.frame_mbs_only) * 2 *
      (sps.crop_top + sps.crop_bottom);
  }
  sps.vui_present = w.copy_bits(1, r);
  if (sps.vui_present == 1) {
    if (r.get_bit() == 1) {     // ar_info_present
      int ar_type = r.get_bits(8);

      if (keep_ar_info) {
        w.put_bit(1);
        w.put_bits(8, ar_type);
      } else
        w.put_bit(0);

      sps.ar_found = true;

      if (AVC_EXTENDED_SAR == ar_type) {
        sps.par_num = r.get_bits(16);
        sps.par_den = r.get_bits(16);

        if (keep_ar_info) {
          w.put_bits(16, sps.par_num);
          w.put_bits(16, sps.par_den);
        }

      } else if (AVC_NUM_PREDEFINED_PARS >= ar_type) {
        sps.par_num = s_predefined_pars[ar_type].numerator;
        sps.par_den = s_predefined_pars[ar_type].denominator;

      } else
        sps.ar_found = false;

    } else
      w.put_bit(0);             // ar_info_present

    // copy the rest
    if (w.copy_bits(1, r) == 1)   // overscan_info_present
      w.copy_bits(1, r);          // overscan_appropriate
    if (w.copy_bits(1, r) == 1) { // video_signal_type_present
      w.copy_bits(4, r);          // video_format, video_full_range
      if (w.copy_bits(1, r) == 1) // color_desc_present
        w.copy_bits(24, r);
    }
    if (w.copy_bits(1, r) == 1) { // chroma_loc_info_present
      gecopy(r, w);               // chroma_sample_loc_type_top_field
      gecopy(r, w);               // chroma_sample_loc_type_bottom_field
    }
    sps.timing_info_present = w.copy_bits(1, r);
    if (sps.timing_info_present) {
      sps.num_units_in_tick = w.copy_bits(32, r) * 2;
      sps.time_scale        = w.copy_bits(32, r);
      sps.fixed_frame_rate  = w.copy_bits(1, r);
    }

    bool f = false;
    if (w.copy_bits(1, r) == 1) { // nal_hrd_parameters_present
      hrdcopy(r, w);
      f = true;
    }
    if (w.copy_bits(1, r) == 1) { // vcl_hrd_parameters_present
      hrdcopy(r, w);
      f = true;
    }
    if (f)
      w.copy_bits(1, r);        // low_delay_hrd_flag
    w.copy_bits(1, r);          // pic_struct_present

    if (w.copy_bits(1, r) == 1) { // bitstream_restriction
      w.copy_bits(1, r);          // motion_vectors_over_pic_boundaries_flag
      gecopy(r, w);               // max_bytes_per_pic_denom
      gecopy(r, w);               // max_bits_per_pic_denom
      gecopy(r, w);               // log2_max_mv_length_h
      gecopy(r, w);               // log2_max_mv_length_v
      gecopy(r, w);               // num_reorder_frames
      gecopy(r, w);               // max_dec_frame_buffering
    }
  }

  w.put_bit(1);
  w.byte_align();

  buffer = mcptr_newsps;
  buffer->set_size(w.get_bit_position() / 8);

  sps.checksum = calc_adler32(buffer->get_buffer(), buffer->get_size());

  return true;
}

bool
mpeg4::p10::parse_pps(memory_cptr &buffer,
                      pps_info_t &pps) {
  try {
    bit_cursor_c r(buffer->get_buffer(), buffer->get_size());

    memset(&pps, 0, sizeof(pps));

    r.skip_bits(3);             // forbidden_zero_bit, nal_ref_idc
    if (r.get_bits(5) != 8)     // nal_unit_type
      return false;
    pps.id     = geread(r);
    pps.sps_id = geread(r);

    r.skip_bits(1);             // entropy_coding_mode_flag
    pps.pic_order_present = r.get_bit();
    pps.checksum          = calc_adler32(buffer->get_buffer(), buffer->get_size());

    return true;
  } catch (...) {
    return false;
  }
}

/** Extract the pixel aspect ratio from the MPEG4 layer 10 (AVC) codec data

   This function searches a buffer containing the MPEG4 layer 10 (AVC) codec
   initialization for the pixel aspectc ratio. If it is found then the
   numerator and the denominator are returned, and the aspect ratio
   information is removed from the buffer. The new buffer is returned
   in the variable \c buffer, and the new size is returned in \c buffer_size.

   \param buffer The buffer containing the MPEG4 layer 10 codec data.
   \param buffer_size The size of the buffer in bytes.
   \param par_num The numerator, if found, is stored in this variable.
   \param par_den The denominator, if found, is stored in this variable.

   \return \c true if the pixel aspect ratio was found and \c false
     otherwise.
*/
bool
mpeg4::p10::extract_par(uint8_t *&buffer,
                        size_t &buffer_size,
                        uint32_t &par_num,
                        uint32_t &par_den) {
  try {
    mm_mem_io_c avcc(buffer, buffer_size), new_avcc(NULL, buffer_size, 1024);
    memory_cptr nalu(new memory_c());

    par_num       = 1;
    par_den       = 1;
    bool ar_found = false;

    avcc.read(nalu, 5);
    new_avcc.write(nalu);

    unsigned int num_sps = avcc.read_uint8();
    new_avcc.write_uint8(num_sps);
    num_sps &= 0x1f;
    mxverb(4, boost::format("mpeg4_p10_extract_par: num_sps %1%\n") % num_sps);

    unsigned int sps;
    for (sps = 0; sps < num_sps; sps++) {
      unsigned int length = avcc.read_uint16_be();
      if ((length + avcc.getFilePointer()) >= buffer_size)
        length = buffer_size - avcc.getFilePointer();
      avcc.read(nalu, length);

      bool abort = false;
      if ((0 < length) && ((nalu->get_buffer()[0] & 0x1f) == 7)) {
        nalu_to_rbsp(nalu);
        sps_info_t sps_info;
        if (mpeg4::p10::parse_sps(nalu, sps_info)) {
          ar_found = sps_info.ar_found;
          if (ar_found) {
            par_num = sps_info.par_num;
            par_den = sps_info.par_den;
          }
        }
        rbsp_to_nalu(nalu);
        abort = true;
      }

      new_avcc.write_uint16_be(nalu->get_size());
      new_avcc.write(nalu);

      if (abort) {
        avcc.read(nalu, buffer_size - avcc.getFilePointer());
        new_avcc.write(nalu);
        break;
      }
    }

    buffer_size = new_avcc.getFilePointer();
    buffer      = new_avcc.get_and_lock_buffer();

    return ar_found;

  } catch(...) {
    return false;
  }
}

bool
mpeg4::p10::is_avc_fourcc(const char *fourcc) {
  return !strncasecmp(fourcc, "avc",  3)
      || !strncasecmp(fourcc, "h264", 4)
      || !strncasecmp(fourcc, "x264", 4);
}

static bool
mpeg4::p10::compare_poc_by_poc(const poc_t &poc1,
                               const poc_t &poc2) {
  return poc1.poc < poc2.poc;
}

static bool
mpeg4::p10::compare_poc_by_dec(const poc_t &poc1,
                               const poc_t &poc2) {
  return poc1.dec < poc2.dec;
}

memory_cptr
mpeg4::p10::avcc_to_nalus(const unsigned char *buffer,
                          size_t size) {
  try {
    if (6 > size)
      throw false;

    uint32_t marker = get_uint32_be(buffer);
    if (((marker & 0xffffff00) == 0x00000100) || (0x00000001 == marker))
      return clone_memory(buffer, size);

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
      return clone_memory(nalus.get_buffer(), nalus.get_size());

  } catch (...) {
  }

  return memory_cptr(NULL);
}

mpeg4::p10::avc_es_parser_c::avc_es_parser_c()
  : m_nalu_size_length(4)
  , m_keep_ar_info(true)
  , m_avcc_ready(false)
  , m_avcc_changed(false)
  , m_default_duration(40000000)
  , m_frame_number(0)
  , m_num_skipped_frames(0)
  , m_first_keyframe_found(false)
  , m_recovery_point_valid(false)
  , m_b_frames_since_keyframe(false)
  , m_max_timecode(0)
  , m_generate_timecodes(false)
  , m_have_incomplete_frame(false)
  , m_ignore_nalu_size_length_errors(false)
  , m_discard_actual_frames(false)
  , m_num_slices_by_type(11, 0)
  , m_debug_keyframe_detection(debugging_requested("avc_keyframe_detection") || debugging_requested("avc_parser"))
  , m_debug_nalu_types(debugging_requested("avc_nalu_types") || debugging_requested("avc_parser"))
{
  if (m_debug_nalu_types)
    init_nalu_names();
}

mpeg4::p10::avc_es_parser_c::~avc_es_parser_c() {
  if (!debugging_requested("avc_num_slices_by_type"))
    return;

  static const char *s_type_names[] = {
    "P",  "B",  "I",  "SP",  "SI",
    "P2", "B2", "I2", "SP2", "SI2",
    "unknown"
  };

  int i;
  mxinfo("mpeg4::p10: Number of slices by type:\n");
  for (i = 0; 10 >= i; ++i)
    if (0 != m_num_slices_by_type[i])
      mxinfo(boost::format("  %1%: %2%\n") % s_type_names[i] % m_num_slices_by_type[i]);
}

void
mpeg4::p10::avc_es_parser_c::discard_actual_frames(bool discard) {
  m_discard_actual_frames = discard;
}

void
mpeg4::p10::avc_es_parser_c::add_bytes(unsigned char *buffer,
                                       int size) {
  memory_slice_cursor_c cursor;
  int marker_size          = 0;
  int previous_marker_size = 0;
  int previous_pos         = -1;

  if (m_unparsed_buffer.is_set() && (0 != m_unparsed_buffer->get_size()))
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

  int new_size = cursor.get_size() - previous_pos;
  if (0 != new_size) {
    m_unparsed_buffer = memory_cptr(new memory_c(safemalloc(new_size), new_size, true));
    cursor.copy(m_unparsed_buffer->get_buffer(), previous_pos, new_size);

  } else
    m_unparsed_buffer = memory_cptr(NULL);
}

void
mpeg4::p10::avc_es_parser_c::flush() {
  if (m_unparsed_buffer.is_set() && (5 <= m_unparsed_buffer->get_size())) {
    int marker_size = get_uint32_be(m_unparsed_buffer->get_buffer()) == NALU_START_CODE ? 4 : 3;
    handle_nalu(clone_memory(m_unparsed_buffer->get_buffer() + marker_size, m_unparsed_buffer->get_size() - marker_size));
  }

  m_unparsed_buffer = memory_cptr(NULL);
  if (m_have_incomplete_frame) {
    m_frames.push_back(m_incomplete_frame);
    m_have_incomplete_frame = false;
  }

  cleanup();
}

void
mpeg4::p10::avc_es_parser_c::add_timecode(int64_t timecode) {
  std::deque<int64_t>::iterator i = m_timecodes.end();
  while (i != m_timecodes.begin()) {
    if (timecode >= *(i - 1))
      break;
    --i;
  }
  m_timecodes.insert(i, timecode);

  m_max_timecode = std::max(timecode, m_max_timecode);
}

void
mpeg4::p10::avc_es_parser_c::write_nalu_size(unsigned char *buffer,
                                             size_t size,
                                             int this_nalu_size_length) {
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

bool
mpeg4::p10::avc_es_parser_c::flush_decision(slice_info_t &si,
                                            slice_info_t &ref) {

  if (NALU_TYPE_IDR_SLICE == si.nalu_type) {
    if (0 != si.first_mb_in_slice)
      return false;

    if ((NALU_TYPE_IDR_SLICE == ref.nalu_type) && (si.idr_pic_id != ref.idr_pic_id))
      return true;

    if (NALU_TYPE_IDR_SLICE != ref.nalu_type)
      return true;
  }

  if (si.frame_num != ref.frame_num)
    return true;
  if (si.field_pic_flag != ref.field_pic_flag)
    return true;
  if ((si.nal_ref_idc != ref.nal_ref_idc) && (!si.nal_ref_idc || !ref.nal_ref_idc))
    return true;

  if (m_sps_info_list[si.sps].pic_order_cnt_type == m_sps_info_list[ref.sps].pic_order_cnt_type) {
    if (!m_sps_info_list[ref.sps].pic_order_cnt_type) {
      if (si.pic_order_cnt_lsb != ref.pic_order_cnt_lsb)
        return true;
      if (si.delta_pic_order_cnt_bottom != ref.delta_pic_order_cnt_bottom)
        return true;

    } else if ((1 == m_sps_info_list[ref.sps].pic_order_cnt_type)
               && (   (si.delta_pic_order_cnt[0] != ref.delta_pic_order_cnt[0])
                   || (si.delta_pic_order_cnt[1] != ref.delta_pic_order_cnt[1])))
      return true;
  }

  return false;
}

void
mpeg4::p10::avc_es_parser_c::flush_incomplete_frame() {
  if (!m_have_incomplete_frame || !m_avcc_ready)
    return;

  m_frames.push_back(m_incomplete_frame);
  m_incomplete_frame.clear();
  m_have_incomplete_frame = false;
}

void
mpeg4::p10::avc_es_parser_c::flush_unhandled_nalus() {
  std::deque<memory_cptr>::iterator nalu = m_unhandled_nalus.begin();

  while (m_unhandled_nalus.end() != nalu) {
    handle_nalu(*nalu);
    nalu++;
  }

  m_unhandled_nalus.clear();
}

void
mpeg4::p10::avc_es_parser_c::handle_slice_nalu(memory_cptr &nalu) {
  if (!m_avcc_ready) {
    m_unhandled_nalus.push_back(nalu);
    return;
  }

  slice_info_t si;
  if (!parse_slice(nalu, si))
    return;

  if (m_have_incomplete_frame && flush_decision(si, m_incomplete_frame.m_si))
    flush_incomplete_frame();

  if (m_have_incomplete_frame) {
    memory_c &mem = *(m_incomplete_frame.m_data.get_object());
    int offset    = mem.get_size();
    mem.resize(offset + m_nalu_size_length + nalu->get_size());
    write_nalu_size(mem.get_buffer() + offset, nalu->get_size());
    memcpy(mem.get_buffer() + offset + m_nalu_size_length, nalu->get_buffer(), nalu->get_size());

    return;
  }

  bool is_i_slice =  (AVC_SLICE_TYPE_I   == si.type)
                  || (AVC_SLICE_TYPE2_I  == si.type)
                  || (AVC_SLICE_TYPE_SI  == si.type)
                  || (AVC_SLICE_TYPE2_SI == si.type);
  bool is_b_slice =  (AVC_SLICE_TYPE_B   == si.type)
                  || (AVC_SLICE_TYPE2_B  == si.type);

  m_incomplete_frame.m_si       =  si;
  m_incomplete_frame.m_keyframe =  m_recovery_point_valid
                                || (   is_i_slice
                                    && (   (m_debug_keyframe_detection && !m_b_frames_since_keyframe)
                                        || (NALU_TYPE_IDR_SLICE == si.nalu_type)));
  m_recovery_point_valid        =  false;

  if (m_incomplete_frame.m_keyframe) {
    m_first_keyframe_found    = true;
    m_b_frames_since_keyframe = false;
    cleanup();

  } else
    m_b_frames_since_keyframe |= is_b_slice;

  m_incomplete_frame.m_data = create_nalu_with_size(nalu, true);
  m_have_incomplete_frame   = true;

  if (m_generate_timecodes)
    add_timecode(m_frame_number * m_default_duration);
  ++m_frame_number;
}

void
mpeg4::p10::avc_es_parser_c::handle_sps_nalu(memory_cptr &nalu) {
  sps_info_t sps_info;

  nalu_to_rbsp(nalu);
  if (!parse_sps(nalu, sps_info, m_keep_ar_info))
    return;
  rbsp_to_nalu(nalu);

  size_t i;
  for (i = 0; m_sps_info_list.size() > i; ++i)
    if (m_sps_info_list[i].id == sps_info.id)
      break;

  if (m_sps_info_list.size() == i) {
    m_sps_list.push_back(nalu);
    m_sps_info_list.push_back(sps_info);
    m_avcc_changed = true;

  } else if (m_sps_info_list[i].checksum != sps_info.checksum) {
    mxverb(2, boost::format("mpeg4::p10: SPS ID %|1$04x| changed; checksum old %|2$04x| new %|3$04x|\n") % sps_info.id % m_sps_info_list[i].checksum % sps_info.checksum);

    m_sps_info_list[i] = sps_info;
    m_sps_list[i]      = nalu;
    m_avcc_changed     = true;
  }

  m_extra_data.push_back(create_nalu_with_size(nalu));
}

void
mpeg4::p10::avc_es_parser_c::handle_pps_nalu(memory_cptr &nalu) {
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
    m_avcc_changed = true;

  } else if (m_pps_info_list[i].checksum != pps_info.checksum) {
    mxverb(2, boost::format("mpeg4::p10: PPS ID %|1$04x| changed; checksum old %|2$04x| new %|3$04x|\n") % pps_info.id % m_pps_info_list[i].checksum % pps_info.checksum);

    m_pps_info_list[i] = pps_info;
    m_pps_list[i]      = nalu;
    m_avcc_changed     = true;
  }

  m_extra_data.push_back(create_nalu_with_size(nalu));
}

void
mpeg4::p10::avc_es_parser_c::handle_sei_nalu(memory_cptr &nalu) {
  try {
    nalu_to_rbsp(nalu);

    bit_cursor_c r(nalu->get_buffer(), nalu->get_size());

    r.skip_bits(8);

    while (1) {
      int ptype = 0;
      int value;
      while ((value = r.get_bits(8)) == 0xff)
        ptype += value;
      ptype += value;

      int psize = 0;
      while ((value = r.get_bits(8)) == 0xff)
        psize += value;
      psize += value;

      if (6 == ptype) {         // recovery point
        m_recovery_point_valid = true;
        return;
      } else if (0x80 == ptype)
        return;

      r.skip_bits(psize * 8);
    }
  } catch (...) {
  }
}

void
mpeg4::p10::avc_es_parser_c::handle_nalu(memory_cptr nalu) {
  if (1 > nalu->get_size())
    return;

  int type = *(nalu->get_buffer()) & 0x1f;

  if (m_debug_nalu_types)
    mxinfo(boost::format("NALU type 0x%|1$02x| (%2%) size %3%\n") % type % get_nalu_type_name(type) % nalu->get_size());

  switch (type) {
    case NALU_TYPE_SEQ_PARAM:
      flush_incomplete_frame();
      handle_sps_nalu(nalu);
      break;

    case NALU_TYPE_PIC_PARAM:
      flush_incomplete_frame();
      handle_pps_nalu(nalu);
      break;

    case NALU_TYPE_END_OF_SEQ:
    case NALU_TYPE_END_OF_STREAM:
    case NALU_TYPE_ACCESS_UNIT:
      flush_incomplete_frame();
      break;

    case NALU_TYPE_FILLER_DATA:
      // Skip these.
      break;

    case NALU_TYPE_NON_IDR_SLICE:
    case NALU_TYPE_DP_A_SLICE:
    case NALU_TYPE_DP_B_SLICE:
    case NALU_TYPE_DP_C_SLICE:
    case NALU_TYPE_IDR_SLICE:
      if (!m_avcc_ready && !m_sps_info_list.empty() && !m_pps_info_list.empty()) {
        m_avcc_ready = true;
        flush_unhandled_nalus();
      }
      handle_slice_nalu(nalu);
      break;

    default:
      flush_incomplete_frame();
      if (!m_avcc_ready && !m_sps_info_list.empty() && !m_pps_info_list.empty()) {
        m_avcc_ready = true;
        flush_unhandled_nalus();
      }
      m_extra_data.push_back(create_nalu_with_size(nalu));

      if (NALU_TYPE_SEI == type)
        handle_sei_nalu(nalu);

      break;
  }
}

bool
mpeg4::p10::avc_es_parser_c::parse_slice(memory_cptr &buffer,
                                         slice_info_t &si) {
  try {
    bit_cursor_c r(buffer->get_buffer(), buffer->get_size());

    memset(&si, 0, sizeof(si));

    si.nal_ref_idc = r.get_bits(3); // forbidden_zero_bit, nal_ref_idc
    si.nalu_type   = r.get_bits(5); // si.nalu_type
    if (   (NALU_TYPE_NON_IDR_SLICE != si.nalu_type)
        && (NALU_TYPE_DP_A_SLICE    != si.nalu_type)
        && (NALU_TYPE_IDR_SLICE     != si.nalu_type))
      return false;

    si.first_mb_in_slice = geread(r); // first_mb_in_slice
    si.type              = geread(r); // slice_type

    ++m_num_slices_by_type[9 < si.type ? 10 : si.type];

    if (9 < si.type) {
      mxverb(3, boost::format("slice parser error: 9 < si.type: %1%\n") % si.type);
      return false;
    }

    si.pps_id = geread(r);      // pps_id

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

    si.frame_num = r.get_bits(sps.log2_max_frame_num);

    if (!sps.frame_mbs_only) {
      si.field_pic_flag = r.get_bit();
      if (si.field_pic_flag)
        si.bottom_field_flag = r.get_bit();
    }

    if (NALU_TYPE_IDR_SLICE == si.nalu_type)
      si.idr_pic_id = geread(r);

    if (0 == sps.pic_order_cnt_type) {
      si.pic_order_cnt_lsb = r.get_bits(sps.log2_max_pic_order_cnt_lsb);
      if (pps.pic_order_present && !si.field_pic_flag)
        si.delta_pic_order_cnt_bottom = sgeread(r);
    }

    if ((1 == sps.pic_order_cnt_type) && !sps.delta_pic_order_always_zero_flag) {
      si.delta_pic_order_cnt[0] = sgeread(r);
      if (pps.pic_order_present && !si.field_pic_flag)
        si.delta_pic_order_cnt[1] = sgeread(r);
    }

    return true;
  } catch (...) {
    return false;
  }
}

void
mpeg4::p10::avc_es_parser_c::default_cleanup() {
  if (m_frames.size() > m_timecodes.size())
    create_missing_timecodes();

  std::deque<avc_frame_t>::iterator i(m_frames.begin());
  std::deque<int64_t>::iterator t(m_timecodes.begin());

  int64_t r = i->m_start = i->m_end = *t;

  ++i;
  ++t;

  while ((m_frames.end() != i) && (m_timecodes.end() != t)) {
    i->m_ref1        = r - *t;
    r                =
      i->m_start     =
      (i - 1)->m_end = *t;
    ++i;
    ++t;
  }

  if ((m_frames.size() >= 2) && (m_timecodes.end() != t))
    (i - 1)->m_end = *t;

  m_frames_out.insert(m_frames_out.end(), m_frames.begin(), m_frames.end());
  m_timecodes.erase(m_timecodes.begin(), m_timecodes.begin() + m_frames.size());
  m_frames.clear();
}

void
mpeg4::p10::avc_es_parser_c::cleanup() {
  if (m_frames.empty())
    return;

  if (m_discard_actual_frames) {
    m_frames.clear();
    m_timecodes.clear();
    return;
  }

  if (m_frames.size() > m_timecodes.size())
    create_missing_timecodes();

  std::deque<avc_frame_t>::iterator i(m_frames.begin());
  std::deque<int64_t>::iterator t(m_timecodes.begin());

  // This may be wrong but is needed for mkvmerge to work correctly
  // (cluster_helper etc).
  i->m_keyframe = true;

  slice_info_t &idr = i->m_si;
  sps_info_t &sps = m_sps_info_list[idr.sps];
  if (   (   (AVC_SLICE_TYPE_I   != idr.type)
          && (AVC_SLICE_TYPE_SI  != idr.type)
          && (AVC_SLICE_TYPE2_I  != idr.type)
          && (AVC_SLICE_TYPE2_SI != idr.type))
      || (0 == idr.nal_ref_idc)
      || (2 == sps.pic_order_cnt_type)) {
    default_cleanup();
    return;
  }

  std::vector<poc_t> poc;

  if (0 == sps.pic_order_cnt_type) {
    unsigned int j = 0;

    unsigned int prev_pic_order_cnt_msb = 0;
    unsigned int prev_pic_order_cnt_lsb = 0;
    unsigned int pic_order_cnt_msb      = 0;
    bool first                          = true;

    while (m_frames.end() != i) {
      slice_info_t &si = i->m_si;

      if (si.sps != idr.sps) {
        default_cleanup();
        return;
      }

      if (first)
        first = false;

      else if ((si.pic_order_cnt_lsb < prev_pic_order_cnt_lsb) && ((prev_pic_order_cnt_lsb - si.pic_order_cnt_lsb) >= (1u << (sps.log2_max_pic_order_cnt_lsb - 1))))
        pic_order_cnt_msb = prev_pic_order_cnt_msb + (1 << sps.log2_max_pic_order_cnt_lsb);

      else if ((si.pic_order_cnt_lsb > prev_pic_order_cnt_lsb) && ((si.pic_order_cnt_lsb - prev_pic_order_cnt_lsb) > (1u << (sps.log2_max_pic_order_cnt_lsb - 1))))
        pic_order_cnt_msb = prev_pic_order_cnt_msb - (1 << sps.log2_max_pic_order_cnt_lsb);

      else
        pic_order_cnt_msb = prev_pic_order_cnt_msb;

      poc.push_back(poc_t(pic_order_cnt_msb + si.pic_order_cnt_lsb, j));

      if (0 != si.nal_ref_idc) {
        prev_pic_order_cnt_lsb = si.pic_order_cnt_lsb;
        prev_pic_order_cnt_msb = pic_order_cnt_msb;
      }

      ++i;
      ++j;
    }

  } else {
    default_cleanup();
    return;
  }

  std::sort(poc.begin(), poc.end(), compare_poc_by_poc);

  size_t num_frames    = m_frames.size();
  size_t num_timecodes = m_timecodes.size();

  unsigned int j;
  for (j = 0; num_frames > j; ++j, ++t) {
    poc[j].timecode = *t;
    poc[j].duration = num_frames > (j + 1)       ? *(t + 1)                - poc[j].timecode
                    : num_frames < num_timecodes ? m_timecodes[num_frames] - poc[j].timecode
                    :                              0;
  }

  std::sort(poc.begin(), poc.end(), compare_poc_by_dec);

  i          = m_frames.begin();
  i->m_start = poc[0].timecode;
  i->m_end   = poc[0].timecode + poc[0].duration;
  ++i;

  for (j = 1; poc.size() > j; ++j) {
    i->m_ref1  = poc[j-1].timecode - poc[j].timecode;
    i->m_start = poc[j].timecode;
    i->m_end   = poc[j].timecode + poc[j].duration;
    ++i;
  }

  m_frames_out.insert(m_frames_out.end(), m_frames.begin(), m_frames.end());
  m_timecodes.erase(m_timecodes.begin(), m_timecodes.begin() + std::min(num_frames, num_timecodes));
  m_frames.clear();
}

memory_cptr
mpeg4::p10::avc_es_parser_c::create_nalu_with_size(const memory_cptr &src,
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

// TODO:DRY
memory_cptr
mpeg4::p10::avc_es_parser_c::get_avcc() {
  int final_size = 6 + 1;
  int offset     = 6;

  for (auto &mem : m_sps_list)
    final_size += mem->get_size() + 2;
  for (auto &mem : m_pps_list)
    final_size += mem->get_size() + 2;

  unsigned char *buffer = (unsigned char *)safemalloc(final_size);

  assert(!m_sps_list.empty());
  sps_info_t &sps = *m_sps_info_list.begin();

  buffer[0] = 1;
  buffer[1] = sps.profile_idc;
  buffer[2] = sps.profile_compat;
  buffer[3] = sps.level_idc;
  buffer[4] = 0xfc | (m_nalu_size_length - 1);
  buffer[5] = 0xe0 | m_sps_list.size();

  for (auto &mem : m_sps_list) {
    int size = mem->get_size();

    write_nalu_size(buffer + offset, size, 2);
    memcpy(&buffer[offset + 2], mem->get_buffer(), size);
    offset += 2 + size;
  }

  buffer[offset] = m_pps_list.size();
  ++offset;

  for (auto &mem : m_pps_list) {
    int size = mem->get_size();

    write_nalu_size(buffer + offset, size, 2);
    memcpy(&buffer[offset + 2], mem->get_buffer(), size);
    offset += 2 + size;
  }

  return memory_cptr(new memory_c(buffer, final_size, true));
}

void
mpeg4::p10::avc_es_parser_c::dump_info() {
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
mpeg4::p10::avc_es_parser_c::get_nalu_type_name(int type) {
  std::string name = m_nalu_names_by_type[type];
  if (name.empty())
    return "unknown";

  return name;
}

void
mpeg4::p10::avc_es_parser_c::init_nalu_names() {
  m_nalu_names_by_type[NALU_TYPE_NON_IDR_SLICE] = "non IDR slice";
  m_nalu_names_by_type[NALU_TYPE_DP_A_SLICE]    = "DP A slice";
  m_nalu_names_by_type[NALU_TYPE_DP_B_SLICE]    = "DP B slice";
  m_nalu_names_by_type[NALU_TYPE_DP_C_SLICE]    = "DP C slice";
  m_nalu_names_by_type[NALU_TYPE_IDR_SLICE]     = "IDR slice";
  m_nalu_names_by_type[NALU_TYPE_SEI]           = "SEI";
  m_nalu_names_by_type[NALU_TYPE_SEQ_PARAM]     = "SEQ param";
  m_nalu_names_by_type[NALU_TYPE_PIC_PARAM]     = "PIC param";
  m_nalu_names_by_type[NALU_TYPE_ACCESS_UNIT]   = "access unit";
  m_nalu_names_by_type[NALU_TYPE_END_OF_SEQ]    = "end of sequence";
  m_nalu_names_by_type[NALU_TYPE_END_OF_STREAM] = "end of stream";
  m_nalu_names_by_type[NALU_TYPE_FILLER_DATA]   = "filler";
}

void
mpeg4::p10::avc_es_parser_c::create_missing_timecodes() {
  size_t num_timecodes = m_timecodes.size();
  size_t num_frames    = m_frames.size();

  while (num_timecodes < num_frames) {
    ++num_timecodes;
    add_timecode(m_max_timecode + m_default_duration);
  }
}
