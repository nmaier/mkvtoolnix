/** MPEG video helper functions (MPEG 1, 2 and 4)

   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file
   \version $Id$

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MPEG4_COMMON_H
#define __MPEG4_COMMON_H

#include "common_memory.h"

/** Start code for a MPEG-4 part 2 (?) video object plain */
#define MPEGVIDEO_VOP_START_CODE                  0x000001b6
/** Strt code for a MPEG-4 part 2 group of pictures */
#define MPEGVIDEO_GOP_START_CODE                  0x000001b3
/** Start code for a MPEG-1/-2 sequence header */
#define MPEGVIDEO_SEQUENCE_START_CODE             0x000001b3
/** Start code for a MPEG-1 and -2 packet */
#define MPEGVIDEO_PACKET_START_CODE               0x000001ba
#define MPEGVIDEO_SYSTEM_HEADER_START_CODE        0x000001bb
#define MPEGVIDEO_MPEG_PROGRAM_END_CODE           0x000001b9

/** Start code for a MPEG-4 part 2 visual object sequence start marker */
#define MPEGVIDEO_VOS_START_CODE                  0x000001b0
/** Start code for a MPEG-4 part 2 visual object sequence end marker */
#define MPEGVIDEO_VOS_END_CODE                    0x000001b1
/** Start code for a MPEG-4 part 2 visual object marker */
#define MPEGVIDEO_VISUAL_OBJECT_START_CODE        0x000001b5

#define MPEGVIDEO_START_CODE_PREFIX               0x000001
#define MPEGVIDEO_PROGRAM_STREAM_MAP              0xbc
#define MPEGVIDEO_PRIVATE_STREAM_1                0xbd
#define MPEGVIDEO_PADDING_STREAM                  0xbe
#define MPEGVIDEO_PRIVATE_STREAM_2                0xbf
#define MPEGVIDEO_ECM_STREAM                      0xf0
#define MPEGVIDEO_EMM_STREAM                      0xf1
#define MPEGVIDEO_PROGRAM_STREAM_DIRECTORY        0xff
#define MPEGVIDEO_DSMCC_STREAM                    0xf2
#define MPEGVIDEO_ITUTRECH222TYPEE_STREAM         0xf8

#define mpeg_is_start_code(v) \
  ((((v) >> 8) & 0xffffff) == MPEGVIDEO_START_CODE_PREFIX)

/** MPEG-1/-2 frame rate: 24000/1001 frames per second */
#define MPEGVIDEO_FPS_23_976    0x01
/** MPEG-1/-2 frame rate: 24 frames per second */
#define MPEGVIDEO_FPS_24        0x02
/** MPEG-1/-2 frame rate: 25 frames per second */
#define MPEGVIDEO_FPS_25        0x03
/** MPEG-1/-2 frame rate: 30000/1001 frames per second */
#define MPEGVIDEO_FPS_29_97     0x04
/** MPEG-1/-2 frame rate: 30 frames per second */
#define MPEGVIDEO_FPS_30        0x05
/** MPEG-1/-2 frame rate: 50 frames per second */
#define MPEGVIDEO_FPS_50        0x06
/** MPEG-1/-2 frame rate: 60000/1001 frames per second */
#define MPEGVIDEO_FPS_59_94     0x07
/** MPEG-1/-2 frame rate: 60 frames per second */
#define MPEGVIDEO_FPS_60        0x08

/** MPEG-1/-2 aspect ratio 1:1 */
#define MPEGVIDEO_AR_1_1        0x10
/** MPEG-1/-2 aspect ratio 4:3 */
#define MPEGVIDEO_AR_4_3        0x20
/** MPEG-1/-2 aspect ratio 16:9 */
#define MPEGVIDEO_AR_16_9       0x30
/** MPEG-1/-2 aspect ratio 2.21 */
#define MPEGVIDEO_AR_2_21       0x40

#define IS_MPEG4_L2_FOURCC(s) \
  (!strncasecmp((s), "DIVX", 4) || !strncasecmp((s), "XVID", 4) || \
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
#define FRAME_TYPE_TO_CHAR(t) \
  (FRAME_TYPE_I == (t) ? 'I' : FRAME_TYPE_P == (t) ? 'P' : 'B')

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

  video_frame_t():
    data(NULL), size(0), pos(0), type(FRAME_TYPE_I), priv(NULL),
    timecode(0), duration(0), bref(0), fref(0) {};
};

bool MTX_DLL_API mpeg4_p2_extract_par(const unsigned char *buffer,
                                      int buffer_size,
                                      uint32_t &par_num, uint32_t &par_den);
bool MTX_DLL_API mpeg4_p2_extract_size(const unsigned char *buffer,
                                       int buffer_size,
                                       uint32_t &width, uint32_t &height);
void MTX_DLL_API mpeg4_p2_find_frame_types(const unsigned char *buffer,
                                           int buffer_size,
                                           vector<video_frame_t> &frames);
memory_c * MTX_DLL_API
mpeg4_p2_parse_config_data(const unsigned char *buffer, int buffer_size);

bool MTX_DLL_API mpeg4_p10_extract_par(uint8_t *&buffer, int &buffer_size,
                                       uint32_t &par_num, uint32_t &par_den);

int MTX_DLL_API mpeg1_2_extract_fps_idx(const unsigned char *buffer,
                                        int buffer_size);
double MTX_DLL_API mpeg1_2_get_fps(int idx);
bool MTX_DLL_API mpeg1_2_extract_ar(const unsigned char *buffer,
                                    int buffer_size, float &ar);

bool MTX_DLL_API is_mpeg4_p2_fourcc(const void *fourcc);

#endif /* __MPEG4_COMMON_H */
