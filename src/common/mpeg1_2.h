/** MPEG video helper functions (MPEG 1, 2 and 4)

   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_MPEG1_2_H
#define MTX_COMMON_MPEG1_2_H

#include "common/common_pch.h"

// MPEG-1/-2 video start codes
#define MPEGVIDEO_PICTURE_START_CODE           0x00000100
#define MPEGVIDEO_SLICE_START_CODE_LOWER       0x00000101
#define MPEGVIDEO_SLICE_START_CODE_UPPER       0x000001af
#define MPEGVIDEO_USER_DATA_START_CODE         0x000001b2
#define MPEGVIDEO_SEQUENCE_HEADER_START_CODE   0x000001b3
#define MPEGVIDEO_SEQUENCE_ERROR_START_CODE    0x000001b4
#define MPEGVIDEO_EXT_START_CODE               0x000001b5
#define MPEGVIDEO_SEQUENCE_END_CODE            0x000001b7
#define MPEGVIDEO_GROUP_OF_PICTURES_START_CODE 0x000001b8
#define MPEGVIDEO_PACKET_START_CODE            0x000001ba

// MPEG transport stream stram IDs
#define MPEGVIDEO_PROGRAM_STREAM_MAP           0xbc
#define MPEGVIDEO_PRIVATE_STREAM_1             0xbd
#define MPEGVIDEO_PADDING_STREAM               0xbe
#define MPEGVIDEO_PRIVATE_STREAM_2             0xbf
#define MPEGVIDEO_ECM_STREAM                   0xf0
#define MPEGVIDEO_EMM_STREAM                   0xf1
#define MPEGVIDEO_PROGRAM_STREAM_DIRECTORY     0xff
#define MPEGVIDEO_DSMCC_STREAM                 0xf2
#define MPEGVIDEO_ITUTRECH222TYPEE_STREAM      0xf8

// MPEG-1/-2 video frame rate indices
#define MPEGVIDEO_FPS_23_976    0x01 // 24000/1001
#define MPEGVIDEO_FPS_24        0x02 //    24
#define MPEGVIDEO_FPS_25        0x03 //    25
#define MPEGVIDEO_FPS_29_97     0x04 // 30000/1001
#define MPEGVIDEO_FPS_30        0x05 //    30
#define MPEGVIDEO_FPS_50        0x06 //    50
#define MPEGVIDEO_FPS_59_94     0x07 // 60000/1001
#define MPEGVIDEO_FPS_60        0x08 //    60

//MPEG-1/-2 video aspect ratio indices
#define MPEGVIDEO_AR_1_1        0x10 //  1:1
#define MPEGVIDEO_AR_4_3        0x20 //  4:3
#define MPEGVIDEO_AR_16_9       0x30 // 16:9
#define MPEGVIDEO_AR_2_21       0x40 //  2.21

#define MPEGVIDEO_FOURCC_MPEG1  0x10000001
#define MPEGVIDEO_FOURCC_MPEG2  0x10000002

namespace mpeg1_2 {

int extract_fps_idx(const unsigned char *buffer, int buffer_size);
double get_fps(int idx);
bool extract_ar(const unsigned char *buffer, int buffer_size, float &ar);
bool is_fourcc(uint32_t fourcc);
bool version_from_fourcc(uint32_t fourcc);

};

#endif  // MTX_COMMON_MPEG1_2_H
