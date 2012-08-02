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

#include "common/endian.h"
#include "common/mpeg1_2.h"

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
  mxverb(3, boost::format("mpeg_video_fps: start search in %1% bytes\n") % buffer_size);
  if (buffer_size < 8) {
    mxverb(3, "mpeg_video_fps: sequence header too small\n");
    return -1;
  }
  auto marker = get_uint32_be(buffer);
  int idx     = 4;
  while ((idx < buffer_size) && (marker != MPEGVIDEO_SEQUENCE_HEADER_START_CODE)) {
    marker <<= 8;
    marker |= buffer[idx];
    idx++;
  }

  if ((idx + 3) >= buffer_size) {
    mxverb(3, "mpeg_video_fps: no full sequence header start code found\n");
    return -1;
  }

  mxverb(3, boost::format("mpeg_video_fps: found sequence header start code at %1%\n") % (idx - 4));

  return buffer[idx + 3] & 0x0f;
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

  mxverb(3, boost::format("mpeg_video_ar: start search in %1% bytes\n") % buffer_size);
  if (buffer_size < 8) {
    mxverb(3, "mpeg_video_ar: sequence header too small\n");
    return false;
  }
  marker = get_uint32_be(buffer);
  idx = 4;
  while ((idx < buffer_size) && (marker != MPEGVIDEO_SEQUENCE_HEADER_START_CODE)) {
    marker <<= 8;
    marker |= buffer[idx];
    idx++;
  }
  if (idx >= buffer_size) {
    mxverb(3, "mpeg_video_ar: no sequence header start code found\n");
    return false;
  }

  mxverb(3, boost::format("mpeg_video_ar: found sequence header start code at %1%\n") % (idx - 4));
  idx += 3;                     // width and height
  if (idx >= buffer_size) {
    mxverb(3, "mpeg_video_ar: sequence header too small\n");
    return false;
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
  static const int s_fps[8] = {0, 24, 25, 0, 30, 50, 0, 60};

  return ((idx < 1) || (idx > 8)) ?    -1.0
    : MPEGVIDEO_FPS_23_976 == idx ? 24000.0 / 1001.0
    : MPEGVIDEO_FPS_29_97  == idx ? 30000.0 / 1001.0
    : MPEGVIDEO_FPS_59_94  == idx ? 60000.0 / 1001.0
    :                               s_fps[idx - 1];
}

bool
mpeg1_2::is_fourcc(uint32_t fourcc) {
  return (MPEGVIDEO_FOURCC_MPEG1 == fourcc) || (MPEGVIDEO_FOURCC_MPEG2 == fourcc);
}

bool
mpeg1_2::version_from_fourcc(uint32_t fourcc) {
  return (MPEGVIDEO_FOURCC_MPEG1 == fourcc) ? 1 : 2;
}
