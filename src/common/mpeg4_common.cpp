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

#include "common.h"
#include "mpeg4_common.h"

/** Extract the pixel aspect ratio from a MPEG4 video frame
  
   This function searches a buffer containing a MPEG4 video frame
   for the pixel aspectc ratio. If it is found then the numerator
   and the denominator are returned.
  
   \param buffer The buffer containing the MPEG4 video frame.
   \param size The size of the buffer in bytes.
   \param par_num The numerator, if found, is stored in this variable.
   \param par_den The denominator, if found, is stored in this variable.
  
   \return \c true if the pixel aspect ratio was found and \c false
     otherwise.
*/
bool
mpeg4_extract_par(const unsigned char *buffer,
                  int size,
                  uint32_t &par_num,
                  uint32_t &par_den) {
  const uint32_t ar_nums[16] = {0, 1, 12, 10, 16, 40, 0, 0,
                                0, 0,  0,  0,  0,  0, 0, 0};
  const uint32_t ar_dens[16] = {1, 1, 11, 11, 11, 33, 1, 1,
                                1, 1,  1,  1,  1,  1, 1, 1};
  uint32_t marker, aspect_ratio_info, num, den;
  bool b;
  bit_cursor_c bits(buffer, size);

  while (!bits.eof()) {
    if (!bits.peek_bits(32, marker))
      break;

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
    bits.get_bit(b);
    if (b != 0) {               // is_old_id
      bits.skip_bits(4);        // vo_ver_id
      bits.skip_bits(3);        // vo_priority
    }

    if (bits.get_bits(4, aspect_ratio_info)) {
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
    }
    return false;
  }
  return false;
}

/** Find frame boundaries and frame types in a packed video frame
  
   This function searches a buffer containing one or more MPEG4 video frames
   for the frame boundaries and their types. This may be the case for B frames
   if they're glued to another frame like they are in AVI files.
  
   \param buffer The buffer containing the MPEG4 video frame(s).
   \param size The size of the buffer in bytes.
   \param frames The data for each frame that is found is put into this
     variable. See ::video_frame_t
  
   \return Nothing. If no frames were found (e.g. only the dummy header for
     a dummy frame) then \a frames will contain no elements.
*/
void
mpeg4_find_frame_types(const unsigned char *buffer,
                       int size,
                       vector<video_frame_t> &frames) {
  bit_cursor_c bits(buffer, size);
  uint32_t marker, frame_type;
  bool first_frame;
  video_frame_t frame;
  vector<video_frame_t>::iterator fit;

  frame.pos = 0;
  frames.clear();
  first_frame = true;
  mxverb(3, "\nmpeg4_frames: start search in %d bytes\n", size);
  while (!bits.eof()) {
    if (!bits.peek_bits(32, marker))
      break;

    if ((marker & 0xffffff00) != 0x00000100) {
      bits.skip_bits(8);
      continue;
    }

    mxverb(3, "mpeg4_frames:   found start code at %d\n",
           bits.get_bit_position() / 8);
    bits.skip_bits(32);
    if (marker == MPEGVIDEO_OBJECT_PLAIN_START_CODE) {
      if (!bits.get_bits(2, frame_type))
        break;
      if (!first_frame) {
        frame.size = (bits.get_bit_position() / 8) - 4 - frame.pos;
        frames.push_back(frame);
        frame.pos = (bits.get_bit_position() / 8) - 4;
      } else
        first_frame = false;
      frame.type = frame_type == 0 ? 'I' : frame_type == 1 ? 'P' :
        frame_type == 2 ? 'B' : 'S';
      bits.byte_align();
    }
  }

  if (!first_frame) {
    frame.size = size - frame.pos;
    frames.push_back(frame);
  }
  mxverb(2, "mpeg4_frames:   summary: found %d frames ", frames.size());
  for (fit = frames.begin(); fit < frames.end(); fit++)
    mxverb(2, "'%c' (%d at %d) ",
           fit->type, fit->size, fit->pos);
  mxverb(2, "\n");

  fit = frames.begin();
  while (fit < frames.end()) {
    if (fit->size < 10)      // dummy frame
      frames.erase(fit);
    else
      fit++;
  }
}

#define NAL_SPS			7

/** \brief identifies the exact end of the bitstream
 * @return the length of the trailing, or 0 if damaged
 */
static int
decode_rbsp_trailing(const uint8_t *src) {
  int v;
  int r;

  v = *src;
  for (r = 1; r < 9; r++) {
    if (v & 1)
      return r;
    v >>= 1;
  }
  return 0;
}

/** \brief Decodes a network abstraction layer unit.
 * @param consumed is the number of bytes used as input
 * @param length is the length of the array
 * @param dst_length is the number of decoded bytes
 * @returns decoded bytes, might be src+1 if no escapes 
 */
static uint8_t *
decode_nal(const uint8_t *src,
           int length,
           int &dst_length,
           int &consumed,
           int &nal_unit_type) {
  int i, si, di;
  uint8_t *dst;

  nal_unit_type = src[0] & 0x1F;

  src++;
  length--;
  for (i = 0; (i + 1) < length; i += 2) {
    if (src[i] != 0)
      continue;
    if ((i > 0) && (src[i - 1] == 0))
      i--;
    if (((i + 2) < length) && (src[i + 1] == 0) && (src[i + 2] <= 3)) {
      if (src[i + 2] != 3) {
        /* startcode, so we must be past the end */
        length = i;
      }
      break;
    }
  }

  if (i >= (length - 1)) { //no escaped 0
    dst_length = length;
    consumed = length + 1; //+1 for the header
    return (uint8_t *)safememdup(src, length);
  }

  dst = (uint8_t *)safemalloc(length);

  si = 0;
  di = 0;
  while (si < length) { 
    //remove escapes (very rare 1:2^22)
    if (((si + 2) < length) && (src[si] == 0) && (src[si + 1] == 0) &&
        (src[si + 2] <= 3)) {
      if (src[si + 2] == 3) { //escape
        dst[di++] = 0;
        dst[di++] = 0;
        si += 3;
        continue;
      } else //next start code
        break;
    }

    dst[di++] = src[si++];
  }

  dst_length = di;
  consumed = si + 1; //+1 for the header
  return dst;
}

static const uint8_t golomb_vlc_len[512] = {
  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 
  9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 7, 
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
  5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 5, 
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 
  3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 
  1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1, 1
};

static int
ilog2(uint32_t value) {
  int n;

  n = 0;
  if (value & 0xffff0000) {
    value >>= 16;
    n += 16;
  }
  if (value & 0xff00) {
    value >>= 8;
    n += 8;
  }
  if (value >= 0x80)
    return n + 7;
  else if (value >= 0x40)
    return n + 6;
  else if (value >= 0x20)
    return n + 5;
  else if (value >= 0x10)
    return n + 4;
  else if (value >= 0x08)
    return n + 3;
  else if (value >= 0x04)
    return n + 2;
  else if (value >= 0x02)
    return n + 1;

  return n;
}

static bool
skip_golomb_ue(bit_cursor_c &bits) {
  uint32_t buf;
  bool ok;

  ok = bits.get_bits(32, buf);
  if (buf >= (1 << 27)) {
    buf >>= 23;
    ok &= bits.skip_bits(golomb_vlc_len[buf]);
  } else {
    int log;

    log = 2 * ilog2(buf) - 31;
    ok &= bits.skip_bits(32 - log);
  }

  return ok;
}

static bool
decode_seq_parameter_set(bit_cursor_c &bits,
                         uint32_t &par_num,
                         uint32_t &par_den) {
  bool ok;

  ok = true;
  ok &= bits.skip_bits(8);      // profile_idc
  ok &= bits.skip_bits(4);      // constraint_setX_flag
  ok &= bits.skip_bits(4);      // reserved
  ok &= bits.skip_bits(8);      // level_idc
  ok &= skip_golomb_ue(bits);   // sps_id
  ok &= skip_golomb_ue(bits);   // sps: log2_max_frame_num
  ok &= skip_golomb_ue(bits);   // sps: poc_type

  return false;
}

static bool
decode_nal_units(const uint8_t *buffer,
                 int buf_size,
                 uint32_t &par_num,
                 uint32_t &par_den) {
  int nal_length_size;
  int buf_index;

  nal_length_size = 2;
  buf_index = 0;

  while (true) {
    int consumed;
    int dst_length;
    int bit_length;
    uint8_t *ptr;
    int i, nalsize = 0;
    int nal_unit_type;

    if (buf_index >= buf_size)
      break;
    nalsize = 0;
    for (i = 0; i < nal_length_size; i++)
      nalsize = (nalsize << 8) | buffer[buf_index++];

    nal_unit_type = -1;
    ptr = decode_nal(buffer + buf_index, buf_size - buf_index,
                     dst_length, consumed, nal_unit_type);

    if (ptr[dst_length - 1] == 0)
      dst_length--;
    bit_length = 8 * dst_length - decode_rbsp_trailing(ptr + dst_length - 1);

    if (nal_unit_type == NAL_SPS) {
      bit_cursor_c bc(ptr, (bit_length + 7) / 8);
      bool result;

      result = decode_seq_parameter_set(bc, par_num, par_den);
      safefree(ptr);
      return result;
    }

    buf_index += consumed;
    safefree(ptr);
  }

  return false;
}

/** Extract the pixel aspect ratio from the MPEG4 layer 10 (AVC) codec data
  
   This function searches a buffer containing the MPEG4 layer 10 (AVC) codec
   initialization for the pixel aspectc ratio. If it is found then the
   numerator and the denominator are returned.
  
   \param buffer The buffer containing the MPEG4 layer 10 codec data.
   \param size The size of the buffer in bytes.
   \param par_num The numerator, if found, is stored in this variable.
   \param par_den The denominator, if found, is stored in this variable.
  
   \return \c true if the pixel aspect ratio was found and \c false
     otherwise.
*/
bool
mpeg4_l10_extract_par(const uint8_t *buffer,
                      int buf_size,
                      uint32_t &par_num,
                      uint32_t &par_den) {
  const uint8_t *p;
  int cnt, i, nalsize;

  p = buffer;
  // Decode sps from avcC
  cnt = *(p + 5) & 0x1f; // Number of sps
  p += 6;
  for (i = 0; i < cnt; i++) {
    nalsize = get_uint16_be(p) + 2;
    if (decode_nal_units(p, nalsize, par_num, par_den))
      return true;
    p += nalsize;
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
   \param size The buffer size.
  
   \return The index or \c -1 if no MPEG sequence header was found or
     if the buffer was too small.
*/
int
mpeg1_2_extract_fps_idx(const unsigned char *buffer,
                        int size) {
  uint32_t marker;
  int idx;

  mxverb(3, "mpeg_video_fps: start search in %d bytes\n", size);
  if (size < 8) {
    mxverb(3, "mpeg_video_fps: sequence header too small\n");
    return -1;
  }
  marker = get_uint32_be(buffer);
  idx = 4;
  while ((idx < size) && (marker != MPEGVIDEO_SEQUENCE_START_CODE)) {
    marker <<= 8;
    marker |= buffer[idx];
    idx++;
  }
  if (idx >= size) {
    mxverb(3, "mpeg_video_fps: no sequence header start code found\n");
    return -1;
  }

  mxverb(3, "mpeg_video_fps: found sequence header start code at %d\n",
         idx - 4);
  idx += 3;                     // width and height
  if (idx >= size) {
    mxverb(3, "mpeg_video_fps: sequence header too small\n");
    return -1;
  }
  return buffer[idx] & 0x0f;
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
