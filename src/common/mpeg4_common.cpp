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
   \version $Id$

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include "bit_cursor.h"
#include "checksums.h"
#include "common.h"
#include "hacks.h"
#include "mm_io.h"
#include "mpeg4_common.h"

namespace mpeg4 {
  namespace p2 {
    static bool find_vol_header(bit_cursor_c &bits);
    static bool extract_size_internal(const unsigned char *buffer,
                                      int buffer_size,
                                      uint32_t &width, uint32_t &height);
    static bool extract_par_internal(const unsigned char *buffer,
                                     int buffer_size,
                                     uint32_t &par_num, uint32_t &par_den);
  };

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
  };
};

static bool
mpeg4::p2::find_vol_header(bit_cursor_c &bits) {
  uint32_t marker;

  while (!bits.eof()) {
    marker = bits.peek_bits(32);

    if ((marker & 0xffffff00) != 0x00000100) {
      bits.skip_bits(8);
      continue;
    }

    marker &= 0xff;
    if ((marker < 0x20) || (marker > 0x2f)) {
      bits.skip_bits(8);
      continue;
    }

    return true;
  }

  return false;
}

static bool
mpeg4::p2::extract_size_internal(const unsigned char *buffer,
                                 int buffer_size,
                                 uint32_t &width,
                                 uint32_t &height) {
  bit_cursor_c bits(buffer, buffer_size);
  int shape, time_base_den;

  if (!find_vol_header(bits))
    return false;

  mxverb(2, "mpeg4 size: found VOL header at %u\n",
         bits.get_bit_position() / 8);
  bits.skip_bits(32);

  // VOL header
  bits.skip_bits(1);            // random access
  bits.skip_bits(8);            // vo_type
  if (bits.get_bit()) {         // is_old_id
    bits.skip_bits(4);          // vo_ver_id
    bits.skip_bits(3);          // vo_priority
  }

  if (15 == bits.get_bits(4))   // ASPECT_EXTENDED
    bits.skip_bits(16);

  if (1 == bits.get_bit()) {    // vol control parameter
    bits.skip_bits(2);          // chroma format
    bits.skip_bits(1);          // low delay
    if (1 == bits.get_bit()) {  // vbv parameters
      bits.skip_bits(15 + 1);   // first half bitrate, marker
      bits.skip_bits(15 + 1);   // latter half bitrate, marker
      bits.skip_bits(15 + 1);   // first half vbv buffer size, marker
      bits.skip_bits(3);        // latter half vbv buffer size
      bits.skip_bits(11 + 1);   // first half vbv occupancy, marker
      bits.skip_bits(15 + 1);   // latter half vbv oocupancy, marker
    }
  }

  shape = bits.get_bits(2);
  if (3 == shape)               // GRAY_SHAPE
    bits.skip_bits(4);          // video object layer shape extension

  bits.skip_bits(1);            // marker

  time_base_den = bits.get_bits(16); // time base den
  bits.skip_bits(1);            // marker
  if (1 == bits.get_bit()) {    // fixed vop rate
    int time_increment_bits = int_log2(time_base_den - 1) + 1;
    bits.skip_bits(time_increment_bits); // time base num
  }

  if (0 == shape) {             // RECT_SHAPE
    uint32_t tmp_width, tmp_height;

    bits.skip_bits(1);
    tmp_width = bits.get_bits(13);
    bits.skip_bits(1);
    tmp_height = bits.get_bits(13);

    if ((0 != tmp_width) && (0 != tmp_height)) {
      width = tmp_width;
      height = tmp_height;
      return true;
    }
  }

  return false;
}

/** Extract the widht and height from a MPEG4 video frame

   This function searches a buffer containing a MPEG4 video frame
   for the width and height.

   \param buffer The buffer containing the MPEG4 video frame.
   \param buffer_size The size of the buffer in bytes.
   \param width The width, if found, is stored in this variable.
   \param height The height, if found, is stored in this variable.

   \return \c true if width and height were found and \c false
     otherwise.
*/
bool
mpeg4::p2::extract_size(const unsigned char *buffer,
                        int buffer_size,
                        uint32_t &width,
                        uint32_t &height) {
  try {
    return extract_size_internal(buffer, buffer_size, width, height);
  } catch (...) {
    return false;
  }
}

static bool
mpeg4::p2::extract_par_internal(const unsigned char *buffer,
                                int buffer_size,
                                uint32_t &par_num,
                                uint32_t &par_den) {
  const uint32_t ar_nums[16] = {0, 1, 12, 10, 16, 40, 0, 0,
                                0, 0,  0,  0,  0,  0, 0, 0};
  const uint32_t ar_dens[16] = {1, 1, 11, 11, 11, 33, 1, 1,
                                1, 1,  1,  1,  1,  1, 1, 1};
  uint32_t aspect_ratio_info, num, den;
  bit_cursor_c bits(buffer, buffer_size);

  if (!find_vol_header(bits))
    return false;

  mxverb(2, "mpeg4 AR: found VOL header at %u\n",
         bits.get_bit_position() / 8);
  bits.skip_bits(32);

  // VOL header
  bits.skip_bits(1);            // random access
  bits.skip_bits(8);            // vo_type
  if (bits.get_bit()) {         // is_old_id
    bits.skip_bits(4);          // vo_ver_id
    bits.skip_bits(3);          // vo_priority
  }

  aspect_ratio_info = bits.get_bits(4);
  mxverb(2, "mpeg4 AR: aspect_ratio_info: %u\n", aspect_ratio_info);
  if (aspect_ratio_info == 15) { // ASPECT_EXTENDED
    num = bits.get_bits(8);
    den = bits.get_bits(8);
  } else {
    num = ar_nums[aspect_ratio_info];
    den = ar_dens[aspect_ratio_info];
  }
  mxverb(2, "mpeg4 AR: %u den: %u\n", num, den);

  if ((num != 0) && (den != 0) && ((num != 1) || (den != 1)) &&
      (((float)num / (float)den) != 1.0)) {
    par_num = num;
    par_den = den;
    return true;
  }
  return false;
}

/** Extract the pixel aspect ratio from a MPEG4 video frame

   This function searches a buffer containing a MPEG4 video frame
   for the pixel aspectc ratio. If it is found then the numerator
   and the denominator are returned.

   \param buffer The buffer containing the MPEG4 video frame.
   \param buffer_size The size of the buffer in bytes.
   \param par_num The numerator, if found, is stored in this variable.
   \param par_den The denominator, if found, is stored in this variable.

   \return \c true if the pixel aspect ratio was found and \c false
     otherwise.
*/
bool
mpeg4::p2::extract_par(const unsigned char *buffer,
                       int buffer_size,
                       uint32_t &par_num,
                       uint32_t &par_den) {
  try {
    return extract_par_internal(buffer, buffer_size, par_num, par_den);
  } catch (...) {
    return false;
  }
}

/** Find frame boundaries and frame types in a packed video frame

   This function searches a buffer containing one or more MPEG4 video frames
   for the frame boundaries and their types. This may be the case for B frames
   if they're glued to another frame like they are in AVI files.

   \param buffer The buffer containing the MPEG4 video frame(s).
   \param buffer_size The size of the buffer in bytes.
   \param frames The data for each frame that is found is put into this
     variable. See ::video_frame_t

   \return Nothing. If no frames were found (e.g. only the dummy header for
     a dummy frame) then \a frames will contain no elements.
*/
void
mpeg4::p2::find_frame_types(const unsigned char *buffer,
                            int buffer_size,
                            vector<video_frame_t> &frames) {
  mm_mem_io_c bytes(buffer, buffer_size);
  uint32_t marker, frame_type;
  bool frame_found;
  video_frame_t frame;
  vector<video_frame_t>::iterator fit;
  int i;

  frames.clear();
  mxverb(3, "\nmpeg4_frames: start search in %d bytes\n", buffer_size);

  if (4 > buffer_size)
    return;

  frame.pos = 0;
  frame_found = false;

  try {
    marker = bytes.read_uint32_be();
    while (!bytes.eof()) {
      if ((marker & 0xffffff00) != 0x00000100) {
        marker <<= 8;
        marker |= bytes.read_uint8();
        continue;
      }

      mxverb(3, "mpeg4_frames:   found start code at " LLD ": 0x%02x\n",
             bytes.getFilePointer() - 4, marker & 0xff);
      if (marker == MPEGVIDEO_VOP_START_CODE) {
        if (frame_found) {
          frame.size = bytes.getFilePointer() - 4 - frame.pos;
          frames.push_back(frame);
        } else
          frame_found = true;

        frame.pos = bytes.getFilePointer() - 4;
        frame_type = bytes.read_uint8() >> 6;
        frame.type = 0 == frame_type ? FRAME_TYPE_I :
          2 == frame_type ? FRAME_TYPE_B : FRAME_TYPE_P;
      }

      marker = bytes.read_uint32_be();
    }

    if (frame_found) {
      frame.size = buffer_size - frame.pos;
      frames.push_back(frame);
    }

  } catch(...) {
  }

  if (2 <= verbose) {
    mxverb(2, "mpeg4_frames:   summary: found %u frames ",
           (unsigned int)frames.size());
    for (fit = frames.begin(); fit < frames.end(); fit++)
      mxverb(2, "'%c' (%d at %d) ", FRAME_TYPE_TO_CHAR(fit->type), fit->size,
             fit->pos);
    mxverb(2, "\n");
  }

  i = 0;
  while (frames.size() > i) {
    if (frames[i].size < 10)    // dummy frame
      frames.erase(frames.begin() + i);
    else
      ++i;
  }
}

/** Find MPEG-4 part 2 configuration data in a chunk of memory

   This function searches a buffer for MPEG-4 part 2 configuration data.
   AVI files usually store this in front of every key frame. MP4 however
   contains this only in the ESDS' decoder_config.

   \param buffer The buffer to be searched.
   \param buffer_size The size of the buffer in bytes.
   \param data_pos This is set to the start position of the configuration
     data inside the \c buffer if such data was found.
   \param data_pos This is set to the length of the configuration
     data inside the \c buffer if such data was found.

   \return \c NULL if no configuration data was found and a pointer to
     a memory_c object otherwise. This object has to be deleted manually.
*/
memory_c *
mpeg4::p2::parse_config_data(const unsigned char *buffer,
                             int buffer_size) {
  memory_c *mem;
  const unsigned char *p, *end;
  unsigned char *dst;
  uint32_t marker, size;
  int vos_offset;

  if (buffer_size < 5)
    return NULL;

  mxverb(3, "\nmpeg4_config_data: start search in %d bytes\n", buffer_size);

  marker = get_uint32_be(buffer) >> 8;
  p = buffer + 3;
  end = buffer + buffer_size;
  vos_offset = -1;
  size = 0;

  while (p < end) {
    marker = (marker << 8) | *p;
    ++p;
    if (!mpeg_is_start_code(marker))
      continue;

    mxverb(3, "mpeg4_config_data:   found start code at %u: 0x%02x\n",
           (unsigned int)(p - buffer - 4), marker & 0xff);
    if (MPEGVIDEO_VOS_START_CODE == marker)
      vos_offset = p - 4 - buffer;
    else if ((MPEGVIDEO_VOP_START_CODE == marker) ||
             (MPEGVIDEO_GOP_START_CODE == marker)) {
      size = p - 4 - buffer;
      break;
    }
  }

  if (0 == size)
    return NULL;

  if (-1 == vos_offset) {
    mem = new memory_c((unsigned char *)safemalloc(size + 5), size + 5, true);
    dst = mem->get();
    put_uint32_be(dst, MPEGVIDEO_VOS_START_CODE);
    dst[4] = 0xf5;
    memcpy(dst + 5, buffer, size);

  } else {
    mem = new memory_c((unsigned char *)safemalloc(size), size, true);
    dst = mem->get();
    put_uint32_be(dst, MPEGVIDEO_VOS_START_CODE);
    if (3 >= buffer[vos_offset + 4])
      dst[4] = 0xf5;
    else
      dst[4] = buffer[vos_offset + 4];
    memcpy(dst + 5, buffer, vos_offset);
    memcpy(dst + 5 + vos_offset, buffer + vos_offset + 5,
           size - vos_offset - 5);
  }

  mxverb(3, "mpeg4_config_data:   found GOOD config with size %u\n", size);
  return mem;
}

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
  w.copy_bits(5, r);        // initial_cpb_removal_delay_length_minus1
  w.copy_bits(5, r);            // cpb_removal_delay_length_minus1
  w.copy_bits(5, r);            // dpb_output_delay_length_minus1
  w.copy_bits(5, r);            // time_offset_length
}

static void
slcopy(bit_cursor_c &r,
       bit_writer_c &w,
       int size) {
  int delta, next = 8, j;

  for (j = 0; j < size && next != 0; ++j) {
    delta = sgecopy(r, w);
    next = (next + delta + 256) % 256;
  }
}

void
mpeg4::p10::nalu_to_rbsp(memory_cptr &buffer) {
  int pos, size = buffer->get_size();
  mm_mem_io_c d(NULL, size, 100);
  unsigned char *b = buffer->get();

  for (pos = 0; pos < size; ++pos) {
    if (((pos + 2) < size) &&
        (0 == b[pos]) && (0 == b[pos + 1]) && (3 == b[pos + 2])) {
      d.write_uint8(0);
      d.write_uint8(0);
      pos += 2;

    } else
      d.write_uint8(b[pos]);
  }

  buffer = memory_cptr(new memory_c(d.get_and_lock_buffer(),
                                    d.getFilePointer()));
}

void
mpeg4::p10::rbsp_to_nalu(memory_cptr &buffer) {
  int pos, size = buffer->get_size();
  mm_mem_io_c d(NULL, size, 100);
  unsigned char *b = buffer->get();

  for (pos = 0; pos < size; ++pos) {
    if (((pos + 2) < size) &&
        (0 == b[pos]) && (0 == b[pos + 1]) && (3 >= b[pos + 2])) {
      d.write_uint8(0);
      d.write_uint8(0);
      d.write_uint8(3);
      ++pos;

    } else
      d.write_uint8(b[pos]);
  }

  buffer = memory_cptr(new memory_c(d.get_and_lock_buffer(),
                                    d.getFilePointer()));
}

bool
mpeg4::p10::parse_sps(memory_cptr &buffer,
                      sps_info_t &sps,
                      bool keep_ar_info) {
  int size = buffer->get_size();
  unsigned char *newsps = (unsigned char *)safemalloc(size + 100);
  memory_cptr mcptr_newsps(new memory_c(newsps, size));
  bit_cursor_c r(buffer->get(), size);
  bit_writer_c w(newsps, size);
  int i, nref, mb_width, mb_height;

  keep_ar_info |= hack_engaged(ENGAGE_KEEP_BITSTREAM_AR_INFO);

  memset(&sps, 0, sizeof(sps));

  sps.par_num = 1;
  sps.par_den = 1;
  sps.ar_found = false;

  w.copy_bits(3, r);            // forbidden_zero_bit, nal_ref_idc
  if (w.copy_bits(5, r) != 7)   // nal_unit_type
    return false;
  sps.profile_idc = w.copy_bits(8, r); // profile_idc
  if (sps.profile_idc < 0)
    return false;
  sps.profile_compat = w.copy_bits(8, r); // constraints
  sps.level_idc = w.copy_bits(8, r); // level_idc
  sps.id = gecopy(r, w);        // sps id
  if (sps.profile_idc >= 100) { // high profile
    if (gecopy(r, w) == 3)      // chroma_format_idc
      w.copy_bits(1, r);        // residue_transform_flag
    gecopy(r, w);               // bit_depth_luma_minus8
    gecopy(r, w);               // bit_depth_chroma_minus8
    w.copy_bits(1, r);         // qpprime_y_zero_transform_bypass_flag
    if (w.copy_bits(1, r) == 1) // seq_scaling_matrix_present_flag
      for (i = 0; i < 8; ++i)
        if (w.copy_bits(1, r) == 1) // seq_scaling_list_present_flag
          slcopy(r, w, i < 6 ? 16 : 64);
  }
                                // log2_max_frame_num_minus4
  sps.log2_max_frame_num = gecopy(r, w) + 4;
  sps.pic_order_cnt_type = gecopy(r, w); // pic_order_cnt_type
  switch (sps.pic_order_cnt_type) {
    case 0:
                                // log2_max_pic_order_cnt_lsb_minus4
      sps.log2_max_pic_order_cnt_lsb = gecopy(r, w) + 4;
      break;

    case 1:
                                // delta_pic_order_always_zero_flag
      sps.delta_pic_order_always_zero_flag = w.copy_bits(1, r);
                                // offset_for_non_ref_pic
      sps.offset_for_non_ref_pic = sgecopy(r, w);
                                // offset_for_top_to_bottom_field
      sps.offset_for_top_to_bottom_field = sgecopy(r, w);
      nref = gecopy(r, w);     // num_ref_frames_in_pic_order_cnt_cycle
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
  w.copy_bits(1, r);           // gaps_in_frame_num_value_allowed_flag
  mb_width = gecopy(r, w) + 1;  // pic_width_in_mbs_minus1
  mb_height = gecopy(r, w) + 1; // pic_height_in_map_units_minus1
  if (w.copy_bits(1, r) == 0) { // frame_mbs_only_flag
    sps.frame_mbs_only = false;
    w.copy_bits(1, r);          // mb_adaptive_frame_field_flag
  } else
    sps.frame_mbs_only = true;

  sps.width = mb_width * 16;
  sps.height = (2 - sps.frame_mbs_only) * mb_height * 16;

  w.copy_bits(1, r);            // direct_8x8_inference_flag
  if (w.copy_bits(1, r) == 1) {
    sps.crop_left = gecopy(r, w); // frame_crop_left_offset
    sps.crop_right = gecopy(r, w); // frame_crop_right_offset
    sps.crop_top = gecopy(r, w);   // frame_crop_top_offset
    sps.crop_bottom = gecopy(r, w); // frame_crop_bottom_offset

    sps.width = 16 * mb_width - 2 * (sps.crop_left + sps.crop_right);
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

      if (13 >= ar_type) {
        static const int par_nums[14] = {
          0, 1, 12, 10, 16, 40, 24, 20, 32, 80, 18, 15, 64, 160
        };
        static const int par_denoms[14] = {
          0, 1, 11, 11, 11, 33, 11, 11, 11, 33, 11, 11, 33, 99
        };
        sps.par_num = par_nums[ar_type];
        sps.par_den = par_denoms[ar_type];

      } else {
        sps.par_num = r.get_bits(16);
        sps.par_den = r.get_bits(16);

        if (keep_ar_info) {
          w.put_bits(16, sps.par_num);
          w.put_bits(16, sps.par_den);
        }
      }
      sps.ar_found = true;
    } else
      w.put_bit(0);             // ar_info_present

    // copy the rest
    if (w.copy_bits(1, r) == 1) // overscan_info_present
      w.copy_bits(1, r);        // overscan_appropriate
    if (w.copy_bits(1, r) == 1) { // video_signal_type_present
      w.copy_bits(4, r);        // video_format, video_full_range
      if (w.copy_bits(1, r) == 1) // color_desc_present
        w.copy_bits(24, r);
    }
    if (w.copy_bits(1, r) == 1) { // chroma_loc_info_present
      gecopy(r, w);             // chroma_sample_loc_type_top_field
      gecopy(r, w);             // chroma_sample_loc_type_bottom_field
    }
    sps.timing_info_present = w.copy_bits(1, r);
    if (sps.timing_info_present) {
      sps.num_units_in_tick = w.copy_bits(32, r);
      sps.time_scale = w.copy_bits(32, r);
      sps.fixed_frame_rate = w.copy_bits(1, r);
    }

    bool f = false;
    if (w.copy_bits(1, r) == 1) // nal_hrd_parameters_present
      hrdcopy(r, w),  f = true;
    if (w.copy_bits(1, r) == 1) // vcl_hrd_parameters_present
      hrdcopy(r, w),  f = true;
    if (f)
      w.copy_bits(1, r);        // low_delay_hrd_flag
    w.copy_bits(1, r);          // pic_struct_present

    if (w.copy_bits(1, r) == 1) { // bitstream_restriction
      w.copy_bits(1, r);    // motion_vectors_over_pic_boundaries_flag
      gecopy(r, w);             // max_bytes_per_pic_denom
      gecopy(r, w);             // max_bits_per_pic_denom
      gecopy(r, w);             // log2_max_mv_length_h
      gecopy(r, w);             // log2_max_mv_length_v
      gecopy(r, w);             // num_reorder_frames
      gecopy(r, w);             // max_dec_frame_buffering
    }
  }

  w.put_bit(1);
  w.byte_align();

  buffer = mcptr_newsps;
  buffer->set_size(w.get_bit_position() / 8);

  return true;
}

bool
mpeg4::p10::parse_pps(memory_cptr &buffer,
                      pps_info_t &pps) {
  try {
    bit_cursor_c r(buffer->get(), buffer->get_size());

    memset(&pps, 0, sizeof(pps));

    r.skip_bits(3);             // forbidden_zero_bit, nal_ref_idc
    if (r.get_bits(5) != 8)     // nal_unit_type
      return false;
    pps.id = geread(r);
    pps.sps_id = geread(r);

    r.skip_bits(1);             // entropy_coding_mode_flag
    pps.pic_order_present = r.get_bit();

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
                        int &buffer_size,
                        uint32_t &par_num,
                        uint32_t &par_den) {
  try {
    mm_mem_io_c avcc(buffer, buffer_size), new_avcc(NULL, buffer_size, 1024);
    memory_cptr nalu(new memory_c());
    int num_sps, sps, length;
    sps_info_t sps_info;
    bool ar_found;

    par_num = 1;
    par_den = 1;
    ar_found = false;

    avcc.read(nalu, 5);
    new_avcc.write(nalu);

    num_sps = avcc.read_uint8();
    new_avcc.write_uint8(num_sps);
    num_sps &= 0x1f;
    mxverb(4, "mpeg4_p10_extract_par: num_sps %d\n", num_sps);

    for (sps = 0; sps < num_sps; sps++) {
      bool abort;

      length = avcc.read_uint16_be();
      if ((length + avcc.getFilePointer()) >= buffer_size)
        length = buffer_size - avcc.getFilePointer();
      avcc.read(nalu, length);

      abort = false;
      if ((0 < length) && ((nalu->get()[0] & 0x1f) == 7)) {
        nalu_to_rbsp(nalu);
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
    buffer = new_avcc.get_and_lock_buffer();

    return ar_found;

  } catch(...) {
    return false;
  }
}

bool
mpeg4::p10::is_avc_fourcc(const char *fourcc) {
  return
    !strncasecmp(fourcc, "avc", 3) ||
    !strncasecmp(fourcc, "h264", 4) ||
    !strncasecmp(fourcc, "x264", 4);
}

/** \brief Extract the FPS from a MPEG video sequence header

   This function looks for a MPEG sequence header in a buffer containing
   a MPEG1 or MPEG2 video frame. If such a header is found its
   FPS index is extracted and returned. This index can be mapped to the
   actual number of frames per second with the function
   ::mpeg_video_get_fps

   \param buffer The buffer to search for the header.
   \param buffer_size The buffer size.

   \return The index or \c -1 if no MPEG sequence header was found or
     if the buffer was too small.
*/
int
mpeg1_2::extract_fps_idx(const unsigned char *buffer,
                         int buffer_size) {
  uint32_t marker;
  int idx;

  mxverb(3, "mpeg_video_fps: start search in %d bytes\n", buffer_size);
  if (buffer_size < 8) {
    mxverb(3, "mpeg_video_fps: sequence header too small\n");
    return -1;
  }
  marker = get_uint32_be(buffer);
  idx = 4;
  while ((idx < buffer_size) && (marker != MPEGVIDEO_SEQUENCE_START_CODE)) {
    marker <<= 8;
    marker |= buffer[idx];
    idx++;
  }
  if (idx >= buffer_size) {
    mxverb(3, "mpeg_video_fps: no sequence header start code found\n");
    return -1;
  }

  mxverb(3, "mpeg_video_fps: found sequence header start code at %d\n",
         idx - 4);
  idx += 3;                     // width and height
  if (idx >= buffer_size) {
    mxverb(3, "mpeg_video_fps: sequence header too small\n");
    return -1;
  }
  return buffer[idx] & 0x0f;
}

/** \brief Extract the aspect ratio from a MPEG video sequence header

   This function looks for a MPEG sequence header in a buffer containing
   a MPEG1 or MPEG2 video frame. If such a header is found its
   aspect ratio is extracted and returned.

   \param buffer The buffer to search for the header.
   \param buffer_size The buffer size.

   \return \c true if a MPEG sequence header was found and \c false otherwise.
*/
bool
mpeg1_2::extract_ar(const unsigned char *buffer,
                    int buffer_size,
                    float &ar) {
  uint32_t marker;
  int idx;

  mxverb(3, "mpeg_video_ar: start search in %d bytes\n", buffer_size);
  if (buffer_size < 8) {
    mxverb(3, "mpeg_video_ar: sequence header too small\n");
    return -1;
  }
  marker = get_uint32_be(buffer);
  idx = 4;
  while ((idx < buffer_size) && (marker != MPEGVIDEO_SEQUENCE_START_CODE)) {
    marker <<= 8;
    marker |= buffer[idx];
    idx++;
  }
  if (idx >= buffer_size) {
    mxverb(3, "mpeg_video_ar: no sequence header start code found\n");
    return -1;
  }

  mxverb(3, "mpeg_video_ar: found sequence header start code at %d\n",
         idx - 4);
  idx += 3;                     // width and height
  if (idx >= buffer_size) {
    mxverb(3, "mpeg_video_ar: sequence header too small\n");
    return -1;
  }

  switch (buffer[idx] & 0xf0) {
    case MPEGVIDEO_AR_1_1:
      ar = 1.0f;
      break;
    case MPEGVIDEO_AR_4_3:
      ar = 4.0f / 3.0f;
      break;
    case MPEGVIDEO_AR_16_9:
      ar = 16.0f / 9.0f;
      break;
    case MPEGVIDEO_AR_2_21:
      ar = 2.21f;
      break;
    default:
      ar = -1.0f;
  }
  return true;
}

/** \brief Get the number of frames per second

   Converts the index returned by ::mpeg_video_extract_fps_idx to a number.

   \param idx The index as to convert.

   \return The number of frames per second or \c -1.0 if the index was
     invalid.
*/
double
mpeg1_2::get_fps(int idx) {
  static const int fps[8] = {0, 24, 25, 0, 30, 50, 0, 60};

  if ((idx < 1) || (idx > 8))
    return -1.0;
  switch (idx) {
    case MPEGVIDEO_FPS_23_976:
      return (double)24000.0 / 1001.0;
    case MPEGVIDEO_FPS_29_97:
      return (double)30000.0 / 1001.0;
    case MPEGVIDEO_FPS_59_94:
      return (double)60000.0 / 1001.0;
    default:
      return fps[idx - 1];
  }
}

/** \brief Check whether or not a FourCC refers to MPEG-4 part 2 video

   \param fourcc A pointer to a string with four characters

   \return true if the FourCC refers to a MPEG-4 part 2 video codec.
*/
bool
mpeg4::p2::is_fourcc(const void *fourcc) {
  static const char *mpeg4_p2_fourccs[] = {
    "MP42", "DIV2", "DIVX", "XVID", "DX50", "FMP4", "DXGM",
    NULL
  };
  int i;

  for (i = 0; NULL != mpeg4_p2_fourccs[i]; ++i)
    if (!strncasecmp((const char *)fourcc, mpeg4_p2_fourccs[i], 4))
      return true;
  return false;
}

// -------------------------------------------------------------------

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

mpeg4::p10::avc_es_parser_c::avc_es_parser_c():
  m_nalu_size_length(4),
  m_keep_ar_info(true),
  m_avcc_ready(false), m_avcc_changed(false),
  m_default_duration(40000000), m_frame_number(0), m_num_skipped_frames(0),
  m_first_keyframe_found(false), m_recovery_point_valid(false),
  m_generate_timecodes(false),
  m_have_incomplete_frame(false), m_ignore_nalu_size_length_errors(false) {
}

void
mpeg4::p10::avc_es_parser_c::add_bytes(unsigned char *buffer,
                                       int size) {
  memory_slice_cursor_c cursor;
  uint32_t marker, marker_size = 0, previous_marker_size = 0;
  int previous_pos = -1, new_size;

  if ((NULL != m_unparsed_buffer.get()) &&
      (0 != m_unparsed_buffer->get_size()))
    cursor.add_slice(m_unparsed_buffer);
  cursor.add_slice(buffer, size);

  if (3 <= cursor.get_remaining_size()) {
    marker = 1 << 24 |
      (unsigned int)cursor.get_char() << 16 |
      (unsigned int)cursor.get_char() << 8 |
      (unsigned int)cursor.get_char();

    while (1) {
      if (0x00000001 == marker)
        marker_size = 4;
      else if (0x00000001 == (marker & 0x00ffffff))
        marker_size = 3;

      if (0 != marker_size) {
        if (-1 != previous_pos) {
          new_size = cursor.get_position() - marker_size - previous_pos -
            previous_marker_size;
          memory_cptr nalu(new memory_c(safemalloc(new_size), new_size, true));
          cursor.copy(nalu->get(), previous_pos + previous_marker_size,
                      new_size);
          handle_nalu(nalu);
        }
        previous_pos = cursor.get_position() - marker_size;
        previous_marker_size = marker_size;
        marker_size = 0;
      }

      if (!cursor.char_available())
        break;
      marker <<= 8;
      marker |= (unsigned int)cursor.get_char();
    }
  }

  if (-1 == previous_pos)
    previous_pos = 0;

  new_size = cursor.get_size() - previous_pos;
  if (0 != new_size) {
    m_unparsed_buffer = memory_cptr(new memory_c(safemalloc(new_size),
                                                 new_size, true));
    cursor.copy(m_unparsed_buffer->get(), previous_pos, new_size);
  } else
    m_unparsed_buffer = memory_cptr(NULL);
}

void
mpeg4::p10::avc_es_parser_c::flush() {
  if ((NULL != m_unparsed_buffer.get()) &&
      (5 <= m_unparsed_buffer->get_size())) {
    int marker_size = get_uint32_be(m_unparsed_buffer->get()) == 0x0000001 ?
      4 : 3;
    handle_nalu(clone_memory(m_unparsed_buffer->get() + marker_size,
                             m_unparsed_buffer->get_size() - marker_size));
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
  deque<int64_t>::iterator i = m_timecodes.end();
  while (i != m_timecodes.begin()) {
    if (timecode >= *(i - 1))
      break;
    --i;
  }
  m_timecodes.insert(i, timecode);
}

void
mpeg4::p10::avc_es_parser_c::write_nalu_size(unsigned char *buffer,
                                             int size,
                                             int nalu_size_length) {
  int i;

  if (-1 == nalu_size_length)
    nalu_size_length = m_nalu_size_length;

  if (!m_ignore_nalu_size_length_errors &&
      (size >= ((uint64_t)1 << (nalu_size_length * 8)))) {
    int required_bytes;
    for (required_bytes = nalu_size_length + 1;
         size >= (1 << (required_bytes * 8)); ++required_bytes)
      ;
    throw nalu_size_length_error_c(required_bytes);
  }

  for (i = 0; i < nalu_size_length; i++)
    buffer[i] = (size >> (8 * (nalu_size_length - 1 - i))) & 0xff;
}

bool
mpeg4::p10::avc_es_parser_c::flush_decision(slice_info_t &si,
                                            slice_info_t &ref) {
  if (si.frame_num != ref.frame_num)
    return true;
  if (si.field_pic_flag != ref.field_pic_flag)
    return true;
  if ((si.nal_ref_idc != ref.nal_ref_idc) &&
      (!si.nal_ref_idc || !ref.nal_ref_idc))
    return true;

  if (m_sps_info_list[si.sps].pic_order_cnt_type ==
      m_sps_info_list[ref.sps].pic_order_cnt_type) {
    if (!m_sps_info_list[ref.sps].pic_order_cnt_type) {
      if (si.pic_order_cnt_lsb != ref.pic_order_cnt_lsb)
        return true;
      if (si.delta_pic_order_cnt_bottom != ref.delta_pic_order_cnt_bottom)
        return true;

    } else if (1 == m_sps_info_list[ref.sps].pic_order_cnt_type) {
      if ((si.delta_pic_order_cnt[0] != ref.delta_pic_order_cnt[0]) ||
          (si.delta_pic_order_cnt[1] != ref.delta_pic_order_cnt[1]))
        return true;
    }
  }

  if ((NALU_TYPE_IDR_SLICE == si.nalu_type) &&
      (NALU_TYPE_IDR_SLICE == ref.nalu_type) &&
      (si.idr_pic_id != ref.idr_pic_id))
    return true;

  return false;
}

void
mpeg4::p10::avc_es_parser_c::flush_incomplete_frame() {
  if (!m_have_incomplete_frame || !m_avcc_ready)
    return;

  if (1) //m_first_keyframe_found)
    m_frames.push_back(m_incomplete_frame);
  else
    ++m_num_skipped_frames;
  m_incomplete_frame.clear();
  m_have_incomplete_frame = false;
}

void
mpeg4::p10::avc_es_parser_c::flush_unhandled_nalus() {
  deque<memory_cptr>::iterator nalu = m_unhandled_nalus.begin();

  while (m_unhandled_nalus.end() != nalu) {
    handle_nalu(*nalu);
    nalu++;
  }

  m_unhandled_nalus.clear();
}

static int klaus = 0;

void
mpeg4::p10::avc_es_parser_c::handle_slice_nalu(memory_cptr &nalu) {
  slice_info_t si;

//   if (klaus >= 161)
//     mxinfo("slice size %d\n", nalu->get_size());

  if (!m_avcc_ready) {
    m_unhandled_nalus.push_back(nalu);
    return;
  }

  if (!parse_slice(nalu, si)) {
    mxwarn("Slice parser error %d.\n", klaus);
    ++klaus;
    return;
  }

//   mxinfo("frame_num %u: ", si.frame_num);

  if (m_have_incomplete_frame &&
      flush_decision(si, m_incomplete_frame.m_si))
    flush_incomplete_frame();

  if (m_have_incomplete_frame) {
    memory_c &mem = *(m_incomplete_frame.m_data.get());
    int offset = mem.get_size();
    mem.resize(offset + m_nalu_size_length + nalu->get_size());
    write_nalu_size(mem.get() + offset, nalu->get_size());
    memcpy(mem.get() + offset + m_nalu_size_length, nalu->get(),
           nalu->get_size());
    return;
  }

//   mxinfo("SEI type %d\n", (int)m_incomplete_frame.m_si.type);
  m_incomplete_frame.m_si = si;
  m_incomplete_frame.m_keyframe =
    m_recovery_point_valid ||
    ((NALU_TYPE_IDR_SLICE == m_incomplete_frame.m_si.nalu_type) &&
     ((AVC_SLICE_TYPE_I == m_incomplete_frame.m_si.type) ||
      (AVC_SLICE_TYPE2_I == m_incomplete_frame.m_si.type) ||
      (AVC_SLICE_TYPE_SI == m_incomplete_frame.m_si.type) ||
      (AVC_SLICE_TYPE2_SI == m_incomplete_frame.m_si.type)));

  m_recovery_point_valid = false;

  if (m_incomplete_frame.m_keyframe) {
    m_first_keyframe_found = true;
    cleanup();
  }

  m_incomplete_frame.m_data = create_nalu_with_size(nalu, true);
  m_have_incomplete_frame = true;

  if (1) { //m_first_keyframe_found) {
    if (m_generate_timecodes)
      add_timecode(m_frame_number * m_default_duration);
    ++m_frame_number;
  }
}

void
mpeg4::p10::avc_es_parser_c::handle_sps_nalu(memory_cptr &nalu) {
  sps_info_t sps_info;
  vector<sps_info_t>::iterator i;

  nalu_to_rbsp(nalu);
  if (!parse_sps(nalu, sps_info, m_keep_ar_info))
    return;
  rbsp_to_nalu(nalu);

  mxforeach(i, m_sps_info_list)
    if (i->id == sps_info.id)
      break;
  if (m_sps_info_list.end() == i) {
    m_sps_list.push_back(nalu);
    m_sps_info_list.push_back(sps_info);
    m_avcc_changed = true;
  }
}

void
mpeg4::p10::avc_es_parser_c::handle_pps_nalu(memory_cptr &nalu) {
  pps_info_t pps_info;
  vector<pps_info_t>::iterator i;

  nalu_to_rbsp(nalu);
  if (!parse_pps(nalu, pps_info))
    return;
  rbsp_to_nalu(nalu);

  mxforeach(i, m_pps_info_list)
    if (i->id == pps_info.id)
      break;
  if (m_pps_info_list.end() == i) {
    m_pps_list.push_back(nalu);
    m_pps_info_list.push_back(pps_info);
    m_avcc_changed = true;
  }
}

void
mpeg4::p10::avc_es_parser_c::handle_sei_nalu(memory_cptr &nalu) {
  try {
    nalu_to_rbsp(nalu);

    bit_cursor_c r(nalu->get(), nalu->get_size());
    int ptype, psize, value;

    r.skip_bits(8);

    while (1) {
      ptype = 0;
      while ((value = r.get_bits(8)) == 0xff)
        ptype += value;
      ptype += value;

      psize = 0;
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
  int type;
//   int i;

//   mxinfo("NALU! size = %d; ", nalu->get_size());

//   for (i = 0; (8 > i) && (nalu->get_size() > i); ++i)
//     mxinfo("%02x ", *(nalu->get() + i));

//   if (6 <= nalu->get_size()) {
//     mxinfo(" ... ");
//     for (i = nalu->get_size() - 6; nalu->get_size() > i; ++i)
//       mxinfo("%02x ", *(nalu->get() + i));
//   }

//   if (1 <= nalu->get_size()) {
//     type = *(nalu->get()) & 0x1f;
//     mxinfo("type %d", type);
//   }
//   mxinfo("\n");

  if (1 > nalu->get_size())
    return;

  type = *(nalu->get()) & 0x1f;

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
    bit_cursor_c r(buffer->get(), buffer->get_size());
    int sps_idx, pps_idx;

    memset(&si, 0, sizeof(si));

    si.nal_ref_idc = r.get_bits(3); // forbidden_zero_bit, nal_ref_idc
    si.nalu_type = r.get_bits(5);   // si.nalu_type
    if ((NALU_TYPE_NON_IDR_SLICE != si.nalu_type) &&
        (NALU_TYPE_DP_A_SLICE != si.nalu_type) &&
        (NALU_TYPE_IDR_SLICE != si.nalu_type))
      return false;

    geread(r);                  // first_mb_in_slice
    si.type = geread(r);        // slice_type

    if (9 < si.type) {
      mxverb(3, "slice parser error: 9 < si.type: %u\n", si.type);
      return false;
    }

    si.pps_id = geread(r);      // pps_id

    for (pps_idx = 0; m_pps_info_list.size() > pps_idx; ++pps_idx)
      if (m_pps_info_list[pps_idx].id == si.pps_id)
        break;
    if (m_pps_info_list.size() == pps_idx) {
      mxverb(3, "slice parser error: PPS not found: %u\n", si.pps_id);
      return false;
    }

    pps_info_t &pps = m_pps_info_list[pps_idx];

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

    if ((1 == sps.pic_order_cnt_type) &&
        !sps.delta_pic_order_always_zero_flag) {
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
  deque<avc_frame_t>::iterator i(m_frames.begin());
  deque<int64_t>::iterator t(m_timecodes.begin());

//   mxinfo("default cleanup\n");

  int64_t r = i->m_start = i->m_end = *t;

  ++i;
  ++t;

  while ((m_frames.end() != i) && (m_timecodes.end() != t)) {
    i->m_ref1 = r - *t;
    r = i->m_start = (i - 1)->m_end = *t;
    ++i;
    ++t;
  }

  if ((m_frames.size() >= 2) && (m_timecodes.end() != t))
    (i - 1)->m_end = *t;

  m_frames_out.insert(m_frames_out.end(), m_frames.begin(), m_frames.end());
  m_timecodes.erase(m_timecodes.begin(),
                    m_timecodes.begin() + m_frames.size());
  m_frames.clear();
}

void
mpeg4::p10::avc_es_parser_c::cleanup() {
  deque<avc_frame_t>::iterator i(m_frames.begin());
  deque<int64_t>::iterator t(m_timecodes.begin());
  unsigned j;

//   mxinfo("cleanup! dumping timecodes:\n");
//   mxforeach(t, m_timecodes) {
//     mxinfo("  " FMT_TIMECODEN "\n", ARG_TIMECODEN(*t));
//   }
//   t = m_timecodes.begin();

  if (m_frames.empty())
    return;

  // This may be wrong but is needed for mkvmerge to work correctly
  // (cluster_helper etc).
  i->m_keyframe = true;

  if (m_timecodes.size() < m_frames.size()) {
    mxverb(4, "mpeg4::p10::avc_es_parser_c::cleanup() numfr %d sti %d\n",
           (int)m_frames.size(), (int)m_timecodes.size());
    m_timecodes.erase(m_timecodes.begin(),
                      m_timecodes.begin() + m_frames.size());
    m_frames.clear();
    return;
  }

  slice_info_t &idr = i->m_si;
  sps_info_t &sps = m_sps_info_list[idr.sps];
  if (//(NALU_TYPE_IDR_SLICE != idr.nalu_type) ||
      ((AVC_SLICE_TYPE_I != idr.type) &&
       (AVC_SLICE_TYPE_SI != idr.type) &&
       (AVC_SLICE_TYPE2_I != idr.type) &&
       (AVC_SLICE_TYPE2_SI != idr.type)) ||
      (0 == idr.nal_ref_idc) ||
      (2 == sps.pic_order_cnt_type)) {
    default_cleanup();
    return;
  }

//   ++i;

  vector<poc_t> poc;

  if (0 == sps.pic_order_cnt_type) {
//     poc.push_back(poc_t(0, 0));
//     j = 1;
    j = 0;

    int prev_pic_order_cnt_msb = 0, prev_pic_order_cnt_lsb = 0;
    int pic_order_cnt_msb = -1;

    while (m_frames.end() != i) {
      slice_info_t &si = i->m_si;

      if (si.sps != idr.sps) {
//         mxinfo("WUFF!!!!!!!\n");
        default_cleanup();
        return;
      }

      if (-1 == pic_order_cnt_msb)
        pic_order_cnt_msb = 0;
      else if ((si.pic_order_cnt_lsb < prev_pic_order_cnt_lsb) &&
               ((prev_pic_order_cnt_lsb - si.pic_order_cnt_lsb) >=
                (1 << (sps.log2_max_pic_order_cnt_lsb - 1))))
        pic_order_cnt_msb = prev_pic_order_cnt_msb +
          (1 << sps.log2_max_pic_order_cnt_lsb);
      else if ((si.pic_order_cnt_lsb > prev_pic_order_cnt_lsb) &&
               ((si.pic_order_cnt_lsb - prev_pic_order_cnt_lsb) >
                (1 << (sps.log2_max_pic_order_cnt_lsb - 1))))
        pic_order_cnt_msb = prev_pic_order_cnt_msb -
          (1 << sps.log2_max_pic_order_cnt_lsb);
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

//   mxinfo("normal cleanup\n");

//   mxinfo("dumping POC\n");
//   for (j = 0; poc.size() > j; ++j)
//     mxinfo("  %d: poc %d dec %d\n", j, poc[j].poc, poc[j].dec);

  sort(poc.begin(), poc.end(), compare_poc_by_poc);

  int num_frames = m_frames.size(), num_timecodes = m_timecodes.size();

  for (j = 0; num_frames > j; ++j, ++t) {
    poc[j].timecode = *t;
    poc[j].duration =
      (j + 1) < num_frames ? *(t + 1) - poc[j].timecode :
      num_timecodes > num_frames ?
      m_timecodes[num_frames] - poc[j].timecode :
      0;
  }

  sort(poc.begin(), poc.end(), compare_poc_by_dec);

  i = m_frames.begin();
  i->m_start = poc[0].timecode;
  i->m_end = poc[0].timecode + poc[0].duration;
  ++i;

  for (j = 1; poc.size() > j; ++j) {
    i->m_ref1 = poc[j-1].timecode - poc[j].timecode;
    i->m_start = poc[j].timecode;
    i->m_end = poc[j].timecode + poc[j].duration;
    ++i;
  }

  m_frames_out.insert(m_frames_out.end(), m_frames.begin(), m_frames.end());
  m_timecodes.erase(m_timecodes.begin(),
                    m_timecodes.begin() + m_frames.size());
  m_frames.clear();
}

memory_cptr
mpeg4::p10::avc_es_parser_c::create_nalu_with_size(const memory_cptr &src,
                                                   bool add_extra_data) {
  int final_size = m_nalu_size_length + src->get_size(), offset = 0, size;
  unsigned char *buffer;

  if (add_extra_data) {
    deque<memory_cptr>::iterator it;

    mxforeach(it, m_extra_data)
      final_size += (*it)->get_size();
    buffer = (unsigned char *)safemalloc(final_size);

    mxforeach(it, m_extra_data) {
      memcpy(buffer + offset, (*it)->get(), (*it)->get_size());
      offset += (*it)->get_size();
    }

    m_extra_data.clear();
  } else
    buffer = (unsigned char *)safemalloc(final_size);

  size = src->get_size();
  write_nalu_size(buffer + offset, size);
  memcpy(buffer + offset + m_nalu_size_length, src->get(), size);

  return memory_cptr(new memory_c(buffer, final_size, true));
}

memory_cptr
mpeg4::p10::avc_es_parser_c::get_avcc() {
  deque<memory_cptr>::iterator it;
  unsigned char *buffer;
  int final_size = 6 + 1, offset = 6, size;

  mxforeach(it, m_sps_list)
    final_size += (*it)->get_size() + 2;
  mxforeach(it, m_pps_list)
    final_size += (*it)->get_size() + 2;

  buffer = (unsigned char *)safemalloc(final_size);

  assert(!m_sps_list.empty());
  sps_info_t &sps = *m_sps_info_list.begin();

  buffer[0] = 1;
  buffer[1] = sps.profile_idc;
  buffer[2] = sps.profile_compat;
  buffer[3] = sps.level_idc;
  buffer[4] = 0xfc | (m_nalu_size_length - 1);
  buffer[5] = 0xe0 | m_sps_list.size();

  mxforeach(it, m_sps_list) {
    size = (*it)->get_size();

    write_nalu_size(buffer + offset, size, 2);
    memcpy(&buffer[offset + 2], (*it)->get(), size);
    offset += 2 + size;
  }

  buffer[offset] = m_pps_list.size();
  ++offset;

  mxforeach(it, m_pps_list) {
    size = (*it)->get_size();

    write_nalu_size(buffer + offset, size, 2);
    memcpy(&buffer[offset + 2], (*it)->get(), size);
    offset += 2 + size;
  }

  return memory_cptr(new memory_c(buffer, final_size, true));
}

void
mpeg4::p10::avc_es_parser_c::dump_info() {
  mxinfo("Dumping m_frames_out:\n");
  deque<avc_frame_t>::iterator i;

  mxforeach(i, m_frames_out) {
    mxinfo("size %d key %d start " FMT_TIMECODEN " end " FMT_TIMECODEN
           " ref1 " FMT_TIMECODEN " adler32 0x%08x\n",
           i->m_data->get_size(), i->m_keyframe, ARG_TIMECODEN(i->m_start),
           ARG_TIMECODEN(i->m_end), ARG_TIMECODEN(i->m_ref1),
           calc_adler32(i->m_data->get(), i->m_data->get_size()));
  }
}
