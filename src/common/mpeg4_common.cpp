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
#include "mm_io.h"
#include "mpeg4_common.h"

bool
mpeg4_p2_extract_par_internal(const unsigned char *buffer,
                              int buffer_size,
                              uint32_t &par_num,
                              uint32_t &par_den) {
  const uint32_t ar_nums[16] = {0, 1, 12, 10, 16, 40, 0, 0,
                                0, 0,  0,  0,  0,  0, 0, 0};
  const uint32_t ar_dens[16] = {1, 1, 11, 11, 11, 33, 1, 1,
                                1, 1,  1,  1,  1,  1, 1, 1};
  uint32_t marker, aspect_ratio_info, num, den;
  bit_cursor_c bits(buffer, buffer_size);

  while (!bits.eof()) {
    bits.peek_bits(32, marker);

    if ((marker & 0xffffff00) != 0x00000100) {
      bits.skip_bits(8);
      continue;
    }

    marker &= 0xff;
    if ((marker < 0x20) || (marker > 0x2f)) {
      bits.skip_bits(8);
      continue;
    }

    mxverb(2, "mpeg4 AR: found VOL header at %u\n",
           bits.get_bit_position() / 8);
    bits.skip_bits(32);

    // VOL header
    bits.skip_bits(1);          // random access
    bits.skip_bits(8);          // vo_type
    if (bits.get_bit()) {       // is_old_id
      bits.skip_bits(4);        // vo_ver_id
      bits.skip_bits(3);        // vo_priority
    }

    bits.get_bits(4, aspect_ratio_info);
    mxverb(2, "mpeg4 AR: aspect_ratio_info: %u\n", aspect_ratio_info);
    if (aspect_ratio_info == 15) { // ASPECT_EXTENDED
      bits.get_bits(8, num);
      bits.get_bits(8, den);
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

  fit = frames.begin();
  while (fit < frames.end()) {
    if (fit->size < 10)      // dummy frame
      frames.erase(fit);
    else
      fit++;
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

static int64_t
read_golomb_ue(bit_cursor_c &bits) {
  int i;
  int64_t value;

  i = 0;
  while (i < 64) {
    bool bit = bits.get_bit();
    if (bit)
      break;
    i++;
  }

  bits.get_bits(i, value);

  return ((1 << i) - 1) + value;
}

static int64_t
read_golomb_se(bit_cursor_c &bits) {
  int64_t value;

  value = read_golomb_ue(bits);

  return (value & 0x01) != 0x00 ? (value + 1) / 2 : -(value / 2);
}

/** Extract the pixel aspect ratio from the MPEG4 layer 10 (AVC) codec data

   This function searches a buffer containing the MPEG4 layer 10 (AVC) codec
   initialization for the pixel aspectc ratio. If it is found then the
   numerator and the denominator are returned.

   \param buffer The buffer containing the MPEG4 layer 10 codec data.
   \param buffer_size The size of the buffer in bytes.
   \param par_num The numerator, if found, is stored in this variable.
   \param par_den The denominator, if found, is stored in this variable.

   \return \c true if the pixel aspect ratio was found and \c false
     otherwise.
*/
bool
mpeg4_p10_extract_par(const uint8_t *buffer,
                      int buffer_size,
                      uint32_t &par_num,
                      uint32_t &par_den) {
  try {
    mm_mem_io_c avcc(buffer, buffer_size);
    int num_sps, sps;

    avcc.skip(5);
    num_sps = avcc.read_uint8();
    mxverb(4, "mpeg4_p10_extract_par: num_sps %d\n", num_sps);

    for (sps = 0; sps < num_sps; sps++) {
      int length, poc_type, ar_type, nal_unit_type;

      length = avcc.read_uint16_be();
      bit_cursor_c bits(&buffer[avcc.getFilePointer()],
                        (length + avcc.getFilePointer()) >  buffer_size ?
                        buffer_size - avcc.getFilePointer() : length);
      bits.get_bits(8, nal_unit_type);
      nal_unit_type &= 0x1f;
      mxverb(4, "mpeg4_p10_extract_par: nal_unit_type %d\n", nal_unit_type);
      if (nal_unit_type != 7)   // 7 = SPS
        continue;

      bits.skip_bits(16);
      read_golomb_ue(bits);
      read_golomb_ue(bits);
      poc_type = read_golomb_ue(bits);

      if (poc_type > 1) {
        mxverb(4, "mpeg4_p10_extract_par: poc_type %d\n", poc_type);
        throw false;
      }

      if (poc_type == 0)
        read_golomb_ue(bits);
      else {
        int cycles, i;

        bits.skip_bits(1);
        read_golomb_se(bits);
        read_golomb_se(bits);
        cycles = read_golomb_ue(bits);
        for (i = 0; i < cycles; ++i)
          read_golomb_se(bits);
      }

      read_golomb_ue(bits);
      bits.skip_bits(1);
      read_golomb_ue(bits);
      read_golomb_ue(bits);
      if (!bits.get_bit())      // MB only
        bits.skip_bits(1);
      bits.skip_bits(1);
      if (bits.get_bit()) {     // cropping
        read_golomb_ue(bits);
        read_golomb_ue(bits);
        read_golomb_ue(bits);
        read_golomb_ue(bits);
      }
      if (!bits.get_bit()) {    // VUI
        mxverb(4, "mpeg4_p10_extract_par: !VUI\n");
        throw false;
      }
      if (!bits.get_bit()) {    // AR
        mxverb(4, "mpeg4_p10_extract_par: !AR\n");
        throw false;
      }

      bits.get_bits(8, ar_type);
      if ((ar_type != 0xff) &&  // custom AR
          (ar_type > 13)) {
        mxverb(4, "mpeg4_p10_extract_par: wrong ar_type %d\n", ar_type);
        throw false;
      }

      if (ar_type <= 13) {
        static const int par_nums[14] = {
          0, 1, 12, 10, 16, 40, 24, 20, 32, 80, 18, 15, 64, 160
        };
        static const int par_denoms[14] = {
          0, 1, 11, 11, 11, 33, 11, 11, 11, 33, 11, 11, 33, 99
        };

        par_num = par_nums[ar_type];
        par_den = par_denoms[ar_type];
        mxverb(4, "mpeg4_p10_extract_par: ar_type %d num %d den %d\n",
               ar_type, par_num, par_den);
        return true;
      }

      bits.get_bits(16, par_num);
      bits.get_bits(16, par_den);
      mxverb(4, "mpeg4_p10_extract_par: ar_type %d num %d den %d\n",
             ar_type, par_num, par_den);
      return true;
    } // for
  } catch(...) {
  }

  return false;
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
