/** MPEG video helper functions (MPEG 1, 2 and 4)
 *
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * \file
 * \version $Id$
 *
 * \author Written by Moritz Bunkus <moritz@bunkus.org>.
 */

#ifndef __MPEG4_COMMON_H
#define __MPEG4_COMMON_H

/** Start code for a MPEG4 (?) video object plain */
#define MPEGVIDEO_OBJECT_PLAIN_START_CODE         0x000001b6
/** Start code for a MPEG-1 and -2 sequence header */
#define MPEGVIDEO_SEQUENCE_START_CODE             0x000001b3

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

/** Pointers to MPEG4 video frames and their data
 *
 * MPEG4 video can be stored in a "packed" format, e.g. in AVI. This means
 * that one AVI chunk may contain more than one video frame. This is
 * usually the case with B frames due to limitations in how AVI and
 * Windows' media frameworks work. With ::mpeg4_find_frame_types
 * such packed frames can be analyzed. The results are stored in these
 * structures: one structure for one frame in the analyzed chunk.
 */
typedef struct {
  /** The beginning of the frame data. This is a pointer into an existing
      buffer handed over to ::mpeg4_find_frame_types. */
  unsigned char *data;
  /** The size of the frame in bytes. */
  int size;
  /** The position of the frame in the original buffer. */
  int pos;
  /** The frame type: \c 'I', \c 'P' or \c 'B'. */
  char type;
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
} video_frame_t;

bool MTX_DLL_API mpeg4_extract_par(const unsigned char *buffer, int size,
                                   uint32_t &par_num, uint32_t &par_den);
void MTX_DLL_API mpeg4_find_frame_types(const unsigned char *buffer, int size,
                                        vector<video_frame_t> &frames);

int MTX_DLL_API mpeg1_2_extract_fps_idx(const unsigned char *buffer,
                                        int size);
double MTX_DLL_API mpeg1_2_get_fps(int idx);

#endif /* __MPEG4_COMMON_H */
