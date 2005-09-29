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
#include "common.h"
#include "hacks.h"
#include "mm_io.h"
#include "mpeg4_common.h"

static bool
mpeg4_p2_find_vol_header(bit_cursor_c &bits) {
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
mpeg4_p2_extract_size_internal(const unsigned char *buffer,
                               int buffer_size,
                               uint32_t &width,
                               uint32_t &height) {
  bit_cursor_c bits(buffer, buffer_size);
  int shape, time_base_den;

  if (!mpeg4_p2_find_vol_header(bits))
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
mpeg4_p2_extract_size(const unsigned char *buffer,
                      int buffer_size,
                      uint32_t &width,
                      uint32_t &height) {
  try {
    return mpeg4_p2_extract_size_internal(buffer, buffer_size, width, height);
  } catch (...) {
    return false;
  }
}

static bool
mpeg4_p2_extract_par_internal(const unsigned char *buffer,
                              int buffer_size,
                              uint32_t &par_num,
                              uint32_t &par_den) {
  const uint32_t ar_nums[16] = {0, 1, 12, 10, 16, 40, 0, 0,
                                0, 0,  0,  0,  0,  0, 0, 0};
  const uint32_t ar_dens[16] = {1, 1, 11, 11, 11, 33, 1, 1,
                                1, 1,  1,  1,  1,  1, 1, 1};
  uint32_t aspect_ratio_info, num, den;
  bit_cursor_c bits(buffer, buffer_size);

  if (!mpeg4_p2_find_vol_header(bits))
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
mpeg4_p2_extract_par(const unsigned char *buffer,
                     int buffer_size,
                     uint32_t &par_num,
                     uint32_t &par_den) {
  try {
    return mpeg4_p2_extract_par_internal(buffer, buffer_size, par_num,
                                         par_den);
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
mpeg4_p2_find_frame_types(const unsigned char *buffer,
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

      mxverb(3, "mpeg4_frames:   found start code at %lld: 0x%02x\n",
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
mpeg4_p2_parse_config_data(const unsigned char *buffer,
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
    dst = mem->data;
    put_uint32_be(dst, MPEGVIDEO_VOS_START_CODE);
    dst[4] = 0xf5;
    memcpy(dst + 5, buffer, size);

  } else {
    mem = new memory_c((unsigned char *)safemalloc(size), size, true);
    dst = mem->data;
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


struct bit_reader_t {
  const vector<unsigned char> &v;
  unsigned char mask, val;
  unsigned int vp;

  bit_reader_t(const vector<unsigned char> &nv):
    v(nv), mask(0), val(0), vp(0) { }

  int bit() {
    if (0 == mask) {
      if (vp >= v.size())
        return -1;
      val = v[vp++];
      mask = 0x80;
    }
    int	ret = !!(val & mask);
    mask >>= 1;
    return ret;
  }

  int num(int l) {
    int v = 0, b = 0;
    while (l-- && (b = bit()) >= 0)
      v = (v << 1) | b;
    return b < 0 ? b : v;
  }
};

struct bit_writer_t {
  vector<unsigned char> &v;
  unsigned char mask, val;
  unsigned int vp;

  bit_writer_t(vector<unsigned char> &nv):
    v(nv), mask(0x80), val(0), vp(0) { }

  void bit(int bit) {
    if (bit)
      val |= mask;
    if ((mask >>= 1) == 0) {
      v.push_back(val);
      val = 0;
      mask = 0x80;
      ++vp;
    }
  }

  void flush() {
    while (mask != 0x80)
      bit(0);
  }

  void num(unsigned v, int l) {
    while (l--)
      bit(v & (1 << l));
  }
};

static int
bitcopy(bit_reader_t &r,
        bit_writer_t &w,
        int count) {
  int	bit = -1, v = 0;

  while (count-- && (bit = r.bit()) >= 0)
    w.bit(bit), v = (v << 1) | bit;

  return bit < 0 ? bit : v;
}

static int
gecopy(bit_reader_t &r,
       bit_writer_t &w) {
  int	n = 0, bit;

  while ((bit = r.bit()) == 0)
    w.bit(0), ++n;

  if (bit < 0)
    return bit;

  w.bit(1);

  if ((bit = r.num(n)) >= 0)
    w.num(bit, n);

  return bit < 0 ? bit : (1 << n) - 1 + bit;
}

static int
sgecopy(bit_reader_t &r,
        bit_writer_t &w) {
  int v = gecopy(r, w);
  return v & 1 ? (v + 1)/2 : -(v/2);
}

static void
hrdcopy(bit_reader_t &r,
        bit_writer_t &w) {
  int ncpb = gecopy(r,w);       // cpb_cnt_minus1
  bitcopy(r, w, 4);             // bit_rate_scale
  bitcopy(r, w, 4);             // cpb_size_scale
  for (int i = 0; i <= ncpb; ++i) {
    gecopy(r,w);                // bit_rate_value_minus1
    gecopy(r,w);                // cpb_size_value_minus1
    bitcopy(r, w, 1);           // cbr_flag
  }
  bitcopy(r, w, 5);           // initial_cpb_removal_delay_length_minus1
  bitcopy(r, w, 5);             // cpb_removal_delay_length_minus1
  bitcopy(r, w, 5);             // dpb_output_delay_length_minus1
  bitcopy(r, w, 5);             // time_offset_length
}

static void
slcopy(bit_reader_t &r,
       bit_writer_t &w,
       int size) {
  int delta, next = 8, j;

  for (j = 0; j < size && next != 0; ++j) {
    delta = sgecopy(r, w);
    next = (next + delta + 256) % 256;
  }
}

static void
nalu_to_rbsp(vector<unsigned char> &b) {
  for (size_t p = 1; p + 2 < b.size(); ++p)
    if (b[p] == 0 && b[p+1] == 0 && b[p+2] == 3) {
      b.erase(b.begin()+p+2);
      ++p;
    }
}

static void
rbsp_to_nalu(vector<unsigned char> &b) {
  for (size_t p = 1; p + 2 < b.size(); ++p)
    if (b[p] == 0 && b[p+1] == 0 && b[p+2] <= 3) {
      b.insert(b.begin() + p + 2, 3);
      p += 2;
    }
}

static bool
handle_sps(vector<unsigned char> &sps,
           uint32_t &par_num,
           uint32_t &par_den) {
  vector<unsigned char> newsps;
  bit_reader_t r(sps);
  bit_writer_t w(newsps);
  int profile, i, nref, vuipresent, ar_type;
  bool ar_found;

  par_num = 1;
  par_den = 1;
  ar_found = false;

  bitcopy(r, w, 3);             // forbidden_zero_bit, nal_ref_idc
  if (bitcopy(r, w, 5) != 7)    // nal_unit_type
    return false;
  profile = bitcopy(r, w, 8);   // profile_idc
  if (profile < 0)
    return false;
  bitcopy(r, w, 16);            // constraints, reserved, level
  gecopy(r, w);                 // sps id
  if (profile >= 100) {         // high profile
    if (gecopy(r, w) == 3)      // chroma_format_idc
      bitcopy(r, w, 1);         // residue_transform_flag
    gecopy(r, w);               // bit_depth_luma_minus8
    gecopy(r, w);               // bit_depth_chroma_minus8
    bitcopy(r, w, 1);          // qpprime_y_zero_transform_bypass_flag
    if (bitcopy(r, w, 1) == 1)  // seq_scaling_matrix_present_flag
      for (i = 0; i < 8; ++i)
        if (bitcopy(r, w, 1) == 1) // seq_scaling_list_present_flag
          slcopy(r, w, i < 6 ? 16 : 64);
  }
  gecopy(r, w);                 // log2_max_frame_num
  switch (gecopy(r, w)) {       // pic_order_cnt_type
    default:
      return false;
    case 2:
      break;
    case 0:
      gecopy(r, w);             // log2_max_pic_order_cnt_lsb_minus4
      break;
    case 1:
      bitcopy(r, w, 1);         // delta_pic_order_always_zero_flag
      sgecopy(r, w);            // offset_for_non_ref_pic
      sgecopy(r, w);            // offset_for_top_to_bottom_field
      nref = gecopy(r, w);     // num_ref_frams_in_pic_order_cnt_cycle
      for (i = 0; i < nref; ++i)
        gecopy(r, w);           // offset_for_ref_frame
      break;
  }
  gecopy(r, w);                 // num_ref_frames
  bitcopy(r, w, 1);            // gaps_in_frame_num_value_allowed_flag
  gecopy(r, w);                 // pic_width_in_mbs_minus1
  gecopy(r, w);                 // pic_height_in_map_units_minus1
  if (bitcopy(r, w, 1) == 0)    // frame_mbs_only_flag
    bitcopy(r, w, 1);           // mb_adaptive_frame_field_flag
  bitcopy(r, w, 1);             // direct_8x8_inference_flag
  if (bitcopy(r, w, 1) == 1) {
    gecopy(r, w);               // frame_crop_left_offset
    gecopy(r, w);               // frame_crop_right_offset
    gecopy(r, w);               // frame_crop_top_offset
    gecopy(r, w);               // frame_crop_bottom_offset
  }
  vuipresent = bitcopy(r, w, 1);
  if (vuipresent < 0)
    return false;
  if (vuipresent == 1) {
    if (r.bit() == 1) {         // ar_info_present
      ar_type = r.num(8);

      if (hack_engaged(ENGAGE_KEEP_BITSTREAM_AR_INFO)) {
        w.bit(1);
        w.num(ar_type, 8);
      }
      if (13 >= ar_type) {
        static const int par_nums[14] = {
          0, 1, 12, 10, 16, 40, 24, 20, 32, 80, 18, 15, 64, 160
        };
        static const int par_denoms[14] = {
          0, 1, 11, 11, 11, 33, 11, 11, 11, 33, 11, 11, 33, 99
        };
        par_num = par_nums[ar_type];
        par_den = par_denoms[ar_type];

      } else {
        int tx = r.num(16);
        int ty = r.num(16);
        if (tx < 0 || ty < 0)
          return false;
        par_num = tx;
        par_den = ty;

        if (hack_engaged(ENGAGE_KEEP_BITSTREAM_AR_INFO)) {
          w.num(par_num, 16);
          w.num(par_den, 16);
        }
      }
      ar_found = true;
    }
    if (!hack_engaged(ENGAGE_KEEP_BITSTREAM_AR_INFO))
      w.bit(0);                 // ar_info_present

    // copy the rest
    if (bitcopy(r, w, 1) == 1)  // overscan_info_present
      bitcopy(r, w, 1);         // overscan_appropriate
    if (bitcopy(r, w, 1) == 1) { // video_signal_type_present
      bitcopy(r, w, 4);         // video_format, video_full_range
      if (bitcopy(r, w, 1) == 1) // color_desc_present
        bitcopy(r, w, 24);
    }
    if (bitcopy(r, w, 1) == 1) { // chroma_loc_info_present
      gecopy(r, w);             // chroma_sample_loc_type_top_field
      gecopy(r, w);             // chroma_sample_loc_type_bottom_field
    }
    if (bitcopy(r, w, 1) == 1) { // timing_info_present
      bitcopy(r, w, 32);        // num_units_in_tick
      bitcopy(r, w, 32);        // time_scale
      bitcopy(r, w, 1);         // fixed_frame_rate
    }
    bool f = false;
    if (bitcopy(r, w, 1) == 1)  // nal_hrd_parameters_present
      hrdcopy(r, w),  f = true;
    if (bitcopy(r, w, 1) == 1)  // vcl_hrd_parameters_present
      hrdcopy(r, w),  f = true;
    if (f)
      bitcopy(r, w, 1);         // low_delay_hrd_flag
    bitcopy(r, w, 1);           // pic_struct_present
    if (bitcopy(r, w, 1) == 1) { // bitstream_restriction
      bitcopy(r, w, 1);     // motion_vectors_over_pic_boundaries_flag
      gecopy(r, w);             // max_bytes_per_pic_denom
      gecopy(r, w);             // max_bits_per_pic_denom
      gecopy(r, w);             // log2_max_mv_length_h
      gecopy(r, w);             // log2_max_mv_length_v
      gecopy(r, w);             // num_reorder_frames
      gecopy(r, w);             // max_dec_frame_buffering
    }
  }

  w.bit(1);
  w.flush();

  sps = newsps;

  return ar_found;
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
mpeg4_p10_extract_par(uint8_t *&buffer,
                      int &buffer_size,
                      uint32_t &par_num,
                      uint32_t &par_den) {
  try {
    mm_mem_io_c avcc(buffer, buffer_size), new_avcc(NULL, buffer_size, 1024);
    vector<unsigned char> nalu;
    int num_sps, sps, length;
    bool ar_found;

    par_num = 1;
    par_den = 1;
    ar_found = false;

    avcc.read(nalu, 5);
    new_avcc.write(nalu);
    nalu.clear();
    num_sps = avcc.read_uint8();
    new_avcc.write_uint8(num_sps);
    num_sps &= 0x1f;
    mxverb(4, "mpeg4_p10_extract_par: num_sps %d\n", num_sps);

    for (sps = 0; sps < num_sps; sps++) {
      bool abort;

      length = avcc.read_uint16_be();
      if ((length + avcc.getFilePointer()) >= buffer_size)
        length = buffer_size - avcc.getFilePointer();
      nalu.clear();
      avcc.read(nalu, length);

      abort = false;
      if ((0 < length) && ((nalu[0] & 0x1f) == 7)) {
        nalu_to_rbsp(nalu);
        ar_found = handle_sps(nalu, par_num, par_den);
        rbsp_to_nalu(nalu);
        abort = true;
      }

      new_avcc.write_uint16_be(nalu.size());
      new_avcc.write(nalu);

      if (abort) {
        nalu.clear();
        avcc.read(nalu, buffer_size - avcc.getFilePointer());
        new_avcc.write(nalu);
        break;
      }
    }

    buffer_size = new_avcc.getFilePointer();
    buffer = (unsigned char *)safemalloc(buffer_size);
    new_avcc.setFilePointer(0);
    new_avcc.read(buffer, buffer_size);

    return ar_found;

  } catch(...) {
    return false;
  }
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
mpeg1_2_extract_fps_idx(const unsigned char *buffer,
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
mpeg1_2_extract_ar(const unsigned char *buffer,
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
mpeg1_2_get_fps(int idx) {
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
is_mpeg4_p2_fourcc(const void *fourcc) {
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
