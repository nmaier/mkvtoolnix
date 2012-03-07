/** MPEG video helper functions (MPEG 1, 2 and 4)

   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_MPEG4_P2_H
#define __MTX_COMMON_MPEG4_P2_H

#include "common/os.h"

#include "common/memory.h"

/** Start code for a MPEG-4 part 2 (?) video object plain */
#define MPEGVIDEO_VOP_START_CODE                  0x000001b6
#define MPEGVIDEO_VOL_START_CODE                  0x00000120
/** Start code for a MPEG-4 part 2 group of pictures */
#define MPEGVIDEO_GOP_START_CODE                  0x000001b3
#define MPEGVIDEO_SYSTEM_HEADER_START_CODE        0x000001bb
#define MPEGVIDEO_PROGRAM_STREAM_MAP_START_CODE   0x000001bc
#define MPEGVIDEO_MPEG_PROGRAM_END_CODE           0x000001b9
#define MPEGVIDEO_EXT_START_CODE                  0x000001b5
#define MPEGVIDEO_PICTURE_START_CODE              0x00000100
#define MPEGVIDEO_FIRST_SLICE_START_CODE          0x00000101
#define MPEGVIDEO_LAST_SLICE_START_CODE           0x000001af

/** Start code for a MPEG-4 part 2 visual object sequence start marker */
#define MPEGVIDEO_VOS_START_CODE                  0x000001b0
/** Start code for a MPEG-4 part 2 visual object sequence end marker */
#define MPEGVIDEO_VOS_END_CODE                    0x000001b1
/** Start code for a MPEG-4 part 2 visual object marker */
#define MPEGVIDEO_VISUAL_OBJECT_START_CODE        0x000001b5

#define MPEGVIDEO_START_CODE_PREFIX               0x000001

#define mpeg_is_start_code(v) ((((v) >> 8) & 0xffffff) == MPEGVIDEO_START_CODE_PREFIX)

#define IS_MPEG4_L2_FOURCC(s) \
  (!strncasecmp((s), "MP42", 4) || !strncasecmp((s), "DIV2", 4) || \
   !strncasecmp((s), "DIVX", 4) || !strncasecmp((s), "XVID", 4) || \
   !strncasecmp((s), "DX5", 3))
#define IS_MPEG4_L2_CODECID(s) \
  (((s) == MKV_V_MPEG4_SP) || ((s) == MKV_V_MPEG4_AP) || \
   ((s) == MKV_V_MPEG4_ASP))

enum mpeg_video_type_e {
  MPEG_VIDEO_NONE = 0,
  MPEG_VIDEO_V1,
  MPEG_VIDEO_V2,
  MPEG_VIDEO_V4_LAYER_2,
  MPEG_VIDEO_V4_LAYER_10
};

enum frame_type_e {
  FRAME_TYPE_I,
  FRAME_TYPE_P,
  FRAME_TYPE_B
};
#define FRAME_TYPE_TO_CHAR(t) (FRAME_TYPE_I == (t) ? 'I' : FRAME_TYPE_P == (t) ? 'P' : 'B')

/** Pointers to MPEG4 video frames and their data

   MPEG4 video can be stored in a "packed" format, e.g. in AVI. This means
   that one AVI chunk may contain more than one video frame. This is
   usually the case with B frames due to limitations in how AVI and
   Windows' media frameworks work. With ::mpeg4_find_frame_types
   such packed frames can be analyzed. The results are stored in these
   structures: one structure for one frame in the analyzed chunk.
*/
struct video_frame_t {
  /** The beginning of the frame data. This is a pointer into an existing
      buffer handed over to ::mpeg4_find_frame_types. */
  unsigned char *data;
  /** The size of the frame in bytes. */
  int size;
  /** The position of the frame in the original buffer. */
  int pos;
  /** The frame type: \c 'I', \c 'P' or \c 'B'. */
  frame_type_e type;
  /** Private data. */
  unsigned char *priv;
  /** The timecode of the frame in \c ns. */
  int64_t timecode;
  /** The duration of the frame in \c ns. */
  int64_t duration;
  /** The frame's backward reference in \c ns relative to its
      \link video_frame_t::timecode timecode \endlink.
      This value is only set for P and B frames. */
  int64_t bref;
  /** The frame's forward reference in \c ns relative to its
      \link video_frame_t::timecode timecode \endlink.
      This value is only set for B frames. */
  int64_t fref;

  /** Whether or not this frame is actually coded. */
  bool is_coded;

  video_frame_t()
    : data(nullptr)
    , size(0)
    , pos(0)
    , type(FRAME_TYPE_I)
    , priv(nullptr)
    , timecode(0)
    , duration(0)
    , bref(0)
    , fref(0)
    , is_coded(false)
  {
  }
};

namespace mpeg4 {
  namespace p2 {
    struct config_data_t {
      int m_time_increment_bits;
      int m_width, m_height;
      bool m_width_height_found;

      config_data_t();
    };

    bool is_fourcc(const void *fourcc);
    bool is_v3_fourcc(const void *fourcc);

    bool extract_par(const unsigned char *buffer, int buffer_size, uint32_t &par_num, uint32_t &par_den);
    bool extract_size(const unsigned char *buffer, int buffer_size, uint32_t &width, uint32_t &height);
    void find_frame_types(const unsigned char *buffer, int buffer_size, std::vector<video_frame_t> &frames, const config_data_t &config_data);
    memory_c * parse_config_data(const unsigned char *buffer, int buffer_size, config_data_t &config_data);
  };
};

#endif  // __MTX_COMMON_MPEG4_P2_H
