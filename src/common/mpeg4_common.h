/*
 * mkvmerge -- utility for splicing together matroska files
 * from component media subtypes
 *
 * Distributed under the GPL
 * see the file COPYING for details
 * or visit http://www.gnu.org/copyleft/gpl.html
 *
 * $Id$
 *
 * MPEG1, 2 and 4 video helper functions
 *
 * Written by Moritz Bunkus <moritz@bunkus.org>.
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

typedef struct {
  unsigned char *data;
  int size, pos;
  char type;
  unsigned char *priv;
  int64_t timecode, duration, bref, fref;
} video_frame_t;

bool MTX_DLL_API mpeg4_extract_par(const unsigned char *buffer, int size,
                                   uint32_t &par_num, uint32_t &par_den);
void MTX_DLL_API mpeg4_find_frame_types(const unsigned char *buf, int size,
                                        vector<video_frame_t> &frames);

int MTX_DLL_API mpeg1_2_extract_fps_idx(const unsigned char *buffer,
                                           int size);
double MTX_DLL_API mpeg1_2_get_fps(int idx);

#endif /* __MPEG4_COMMON_H */
