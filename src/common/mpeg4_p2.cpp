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
#include "common/endian.h"
#include "common/math.h"
#include "common/mm_io.h"
#include "common/mpeg4_p2.h"

namespace mpeg4 {
  namespace p2 {
    static bool find_vol_header(bit_cursor_c &bits);
    static bool parse_vol_header(const unsigned char *buffer, int buffer_size, config_data_t &config_data);
    static bool extract_par_internal(const unsigned char *buffer, int buffer_size, uint32_t &par_num, uint32_t &par_den);
    static void parse_frame(video_frame_t &frame, const unsigned char *buffer, const mpeg4::p2::config_data_t &config_data);
  };
};

mpeg4::p2::config_data_t::config_data_t()
  : m_time_increment_bits(0)
  , m_width(0)
  , m_height(0)
  , m_width_height_found(false)
{
}

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
mpeg4::p2::parse_vol_header(const unsigned char *buffer,
                            int buffer_size,
                            mpeg4::p2::config_data_t &config_data) {
  bit_cursor_c bits(buffer, buffer_size);

  if (!find_vol_header(bits))
    return false;

  mxverb(2, boost::format("mpeg4 size: found VOL header at %1%\n") % (bits.get_bit_position() / 8));
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

  int shape = bits.get_bits(2);
  if (3 == shape)               // GRAY_SHAPE
    bits.skip_bits(4);          // video object layer shape extension

  bits.skip_bits(1);            // marker

  int time_base_den                 = bits.get_bits(16); // time base den
  config_data.m_time_increment_bits = int_log2(time_base_den - 1) + 1;

  bits.skip_bits(1);            // marker
  if (1 == bits.get_bit())      // fixed vop rate
    bits.skip_bits(config_data.m_time_increment_bits); // time base num

  if (0 == shape) {             // RECT_SHAPE
    uint32_t tmp_width, tmp_height;

    bits.skip_bits(1);
    tmp_width = bits.get_bits(13);
    bits.skip_bits(1);
    tmp_height = bits.get_bits(13);

    if ((0 != tmp_width) && (0 != tmp_height)) {
      config_data.m_width              = tmp_width;
      config_data.m_height             = tmp_height;
      config_data.m_width_height_found = true;
    }
  }

  return true;
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
    mpeg4::p2::config_data_t config_data;
    if (!parse_vol_header(buffer, buffer_size, config_data) || !config_data.m_width_height_found)
      return false;

    width  = config_data.m_width;
    height = config_data.m_height;

    return true;
  } catch (...) {
    return false;
  }
}

static bool
mpeg4::p2::extract_par_internal(const unsigned char *buffer,
                                int buffer_size,
                                uint32_t &par_num,
                                uint32_t &par_den) {
  const uint32_t ar_nums[16] = {0, 1, 12, 10, 16, 40, 0, 0, 0, 0,  0,  0,  0,  0, 0, 0};
  const uint32_t ar_dens[16] = {1, 1, 11, 11, 11, 33, 1, 1, 1, 1,  1,  1,  1,  1, 1, 1};
  uint32_t aspect_ratio_info, num, den;
  bit_cursor_c bits(buffer, buffer_size);

  if (!find_vol_header(bits))
    return false;

  mxverb(2, boost::format("mpeg4 AR: found VOL header at %1%\n") % (bits.get_bit_position() / 8));
  bits.skip_bits(32);

  // VOL header
  bits.skip_bits(1);            // random access
  bits.skip_bits(8);            // vo_type
  if (bits.get_bit()) {         // is_old_id
    bits.skip_bits(4);          // vo_ver_id
    bits.skip_bits(3);          // vo_priority
  }

  aspect_ratio_info = bits.get_bits(4);
  mxverb(2, boost::format("mpeg4 AR: aspect_ratio_info: %1%\n") % aspect_ratio_info);
  if (aspect_ratio_info == 15) { // ASPECT_EXTENDED
    num = bits.get_bits(8);
    den = bits.get_bits(8);
  } else {
    num = ar_nums[aspect_ratio_info];
    den = ar_dens[aspect_ratio_info];
  }
  mxverb(2, boost::format("mpeg4 AR: %1% den: %2%\n") % num % den);

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

static void
mpeg4::p2::parse_frame(video_frame_t &frame,
                       const unsigned char *buffer,
                       const mpeg4::p2::config_data_t &config_data) {
  static const frame_type_e s_frame_type_map[4] = { FRAME_TYPE_I, FRAME_TYPE_P, FRAME_TYPE_B, FRAME_TYPE_P };

  bit_cursor_c bc(&buffer[ frame.pos + 4 ], frame.size);

  frame.type = s_frame_type_map[ bc.get_bits(2) ];

  while (bc.get_bit())
    ;                           // modulo time base
  bc.skip_bits(1 + config_data.m_time_increment_bits + 1); // marker, vop time increment, marker

  frame.is_coded = bc.get_bit();
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
                            std::vector<video_frame_t> &frames,
                            const mpeg4::p2::config_data_t &config_data) {
  frames.clear();
  mxverb(3, boost::format("\nmpeg4_frames: start search in %1% bytes\n") % buffer_size);

  if (4 > buffer_size)
    return;

  try {
    mm_mem_io_c bytes(buffer, buffer_size);
    uint32_t marker  = bytes.read_uint32_be();
    bool frame_found = false;
    video_frame_t frame;

    while (!bytes.eof()) {
      if ((marker & 0xffffff00) != 0x00000100) {
        marker <<= 8;
        marker |= bytes.read_uint8();
        continue;
      }

      mxverb(3, boost::format("mpeg4_frames:   found start code at %1%: 0x%|2$02x|\n") % (bytes.getFilePointer() - 4) % (marker & 0xff));
      if (marker == MPEGVIDEO_VOP_START_CODE) {
        if (frame_found) {
          frame.size = bytes.getFilePointer() - 4 - frame.pos;
          parse_frame(frame, buffer, config_data);
          frames.push_back(frame);
        } else
          frame_found = true;

        frame.pos = bytes.getFilePointer() - 4;
      }

      if (4 > (buffer_size - bytes.getFilePointer()))
        break;

      marker = bytes.read_uint32_be();
    }

    if (frame_found) {
      frame.size = buffer_size - frame.pos;
      parse_frame(frame, buffer, config_data);
      frames.push_back(frame);
    }

  } catch(...) {
  }

  if (2 <= verbose) {
    mxverb(2, boost::format("mpeg4_frames:   summary: found %1% frames ") % frames.size());
    std::vector<video_frame_t>::iterator fit;
    for (fit = frames.begin(); fit < frames.end(); fit++)
      mxverb(2, boost::format("'%1%' (size %2% coded %4% at %3%) ") % FRAME_TYPE_TO_CHAR(fit->type) %  fit->size % fit->pos % fit->is_coded);
    mxverb(2, "\n");
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

   \return \c nullptr if no configuration data was found and a pointer to
     a memory_c object otherwise. This object has to be deleted manually.
*/
memory_c *
mpeg4::p2::parse_config_data(const unsigned char *buffer,
                             int buffer_size,
                             mpeg4::p2::config_data_t &config_data) {
  if (5 > buffer_size)
    return nullptr;

  mxverb(3, boost::format("\nmpeg4_config_data: start search in %1% bytes\n") % buffer_size);

  uint32_t marker          = get_uint32_be(buffer) >> 8;
  const unsigned char *p   = buffer + 3;
  const unsigned char *end = buffer + buffer_size;
  int vos_offset           = -1;
  int vol_offset           = -1;
  unsigned int size        = 0;

  while (p < end) {
    marker = (marker << 8) | *p;
    ++p;
    if (!mpeg_is_start_code(marker))
      continue;

    mxverb(3, boost::format("mpeg4_config_data:   found start code at %1%: 0x%|2$02x|\n") % (unsigned int)(p - buffer - 4) % (marker & 0xff));
    if (MPEGVIDEO_VOS_START_CODE == marker)
      vos_offset = p - 4 - buffer;

    else if (MPEGVIDEO_VOL_START_CODE == marker)
      vol_offset = p - 4 - buffer;

    else if ((MPEGVIDEO_VOP_START_CODE == marker) || (MPEGVIDEO_GOP_START_CODE == marker)) {
      size = p - 4 - buffer;
      break;
    }
  }

  if (0 == size)
    return nullptr;

  if (-1 != vol_offset)
    try {
      mpeg4::p2::config_data_t cfg_data;
      if (parse_vol_header(&buffer[ vol_offset ], buffer_size - vol_offset, cfg_data))
        config_data.m_time_increment_bits = cfg_data.m_time_increment_bits;

    } catch (...) {
    }

  memory_c *mem;
  if (-1 == vos_offset) {
    mem                = new memory_c((unsigned char *)safemalloc(size + 5), size + 5, true);
    unsigned char *dst = mem->get_buffer();
    put_uint32_be(dst, MPEGVIDEO_VOS_START_CODE);
    dst[4] = 0xf5;
    memcpy(dst + 5, buffer, size);

  } else {
    mem                = new memory_c((unsigned char *)safemalloc(size), size, true);
    unsigned char *dst = mem->get_buffer();
    put_uint32_be(dst, MPEGVIDEO_VOS_START_CODE);
    if (3 >= buffer[vos_offset + 4])
      dst[4] = 0xf5;
    else
      dst[4] = buffer[vos_offset + 4];

    memcpy(dst + 5, buffer, vos_offset);
    memcpy(dst + 5 + vos_offset, buffer + vos_offset + 5, size - vos_offset - 5);
  }

  mxverb(3, boost::format("mpeg4_config_data:   found GOOD config with size %1%\n") % size);
  return mem;
}

/** \brief Check whether or not a FourCC refers to MPEG-4 part 2 video

   \param fourcc A pointer to a string with four characters

   \return true if the FourCC refers to a MPEG-4 part 2 video codec.
*/
bool
mpeg4::p2::is_fourcc(const void *fourcc) {
  static const char *mpeg4_p2_fourccs[] = {
    "MP42", "DIV2", "DIVX", "XVID", "DX50", "FMP4", "DXGM",
    nullptr
  };
  int i;

  for (i = 0; mpeg4_p2_fourccs[i]; ++i)
    if (!strncasecmp((const char *)fourcc, mpeg4_p2_fourccs[i], 4))
      return true;
  return false;
}

/** \brief Check whether or not a FourCC refers to MPEG-4 part 2
    version 3 video

   \param fourcc A pointer to a string with four characters

   \return true if the FourCC refers to a MPEG-4 part 2 video codec.
*/
bool
mpeg4::p2::is_v3_fourcc(const void *fourcc) {
  static const char *mpeg4_p2_v3_fourccs[] = {
    "DIV3", "MPG3", "MP43",
    "AP41", // Angel Potion
    nullptr
  };
  int i;

  for (i = 0; mpeg4_p2_v3_fourccs[i]; ++i)
    if (!strncasecmp((const char *)fourcc, mpeg4_p2_v3_fourccs[i], 4))
      return true;
  return false;
}
