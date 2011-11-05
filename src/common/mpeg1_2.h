/** MPEG video helper functions (MPEG 1, 2 and 4)

   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __MTX_COMMON_MPEG1_2_H
#define __MTX_COMMON_MPEG1_2_H

#include "common/os.h"

#include "common/memory.h"

/** Start code for a MPEG-1/-2 group of pictures */
#define MPEGVIDEO_GOP12_START_CODE                0x000001b8
/** Start code for a MPEG-1/-2 sequence header */
#define MPEGVIDEO_SEQUENCE_START_CODE             0x000001b3
#define MPEGVIDEO_EXT_START_CODE                  0x000001b5
/** Start code for a MPEG-1 and -2 packet */
#define MPEGVIDEO_PACKET_START_CODE               0x000001ba
#define MPEGVIDEO_PROGRAM_STREAM_MAP              0xbc
#define MPEGVIDEO_PRIVATE_STREAM_1                0xbd
#define MPEGVIDEO_PADDING_STREAM                  0xbe
#define MPEGVIDEO_PRIVATE_STREAM_2                0xbf
#define MPEGVIDEO_ECM_STREAM                      0xf0
#define MPEGVIDEO_EMM_STREAM                      0xf1
#define MPEGVIDEO_PROGRAM_STREAM_DIRECTORY        0xff
#define MPEGVIDEO_DSMCC_STREAM                    0xf2
#define MPEGVIDEO_ITUTRECH222TYPEE_STREAM         0xf8

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

#define MPEGVIDEO_FOURCC_MPEG1  0x10000001
#define MPEGVIDEO_FOURCC_MPEG2  0x10000002

namespace mpeg1_2 {
  int extract_fps_idx(const unsigned char *buffer, int buffer_size);
  double get_fps(int idx);
  bool extract_ar(const unsigned char *buffer, int buffer_size, float &ar);
  bool is_fourcc(uint32_t fourcc);
  bool version_from_fourcc(uint32_t fourcc);
};

#endif  // __MTX_COMMON_MPEG1_2_H
