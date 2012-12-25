/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   helper functions for the Core Audio Format

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_CAF_H
#define MTX_COMMON_CAF_H

#include "common/common_pch.h"

// See http://developer.apple.com/documentation/MusicAudio/Reference/CAFSpec/

namespace caf {

struct defs {
  static unsigned int const channel_atom_size             =   12;
  static unsigned int const max_channels                  =    8;
  static unsigned int const max_escape_header_bytes       =    8;
  static unsigned int const max_searches                  =   16;
  static unsigned int const max_coefs                     =   16;
  static unsigned int const default_frames_per_packet     = 4096;
  static unsigned int const default_frame_size            = 4096;
};

#if defined(COMP_MSC)
#pragma pack(push,1)
#endif

struct PACKED_STRUCTURE channel_layout_t {
  enum {
      mono       = (100<<16) | 1
    , stereo     = (101<<16) | 2
    , mpeg_3_0_b = (113<<16) | 3
    , mpeg_4_0_b = (116<<16) | 4
    , mpeg_5_0_d = (120<<16) | 5
    , mpeg_5_1_d = (124<<16) | 6
    , aac_6_1    = (142<<16) | 7
    , mpeg_7_1_b = (127<<16) | 8
  } layout_tag_e;

  enum {
      left                   = 1 << 0
    , right                  = 1 << 1
    , center                 = 1 << 2
    , lfe_screen             = 1 << 3
    , left_surround          = 1 << 4
    , right_surround         = 1 << 5
    , left_center            = 1 << 6
    , right_center           = 1 << 7
    , center_surround        = 1 << 8
    , left_surround_direct   = 1 << 9
    , right_surround_direct  = 1 << 10
    , top_center_surround    = 1 << 11
    , vertical_height_left   = 1 << 12
    , vertical_height_center = 1 << 13
    , vertical_height_right  = 1 << 14
    , top_back_left          = 1 << 15
    , top_back_center        = 1 << 16
    , top_back_right         = 1 << 17
  } channel_bitmap_e;

  uint32_t channel_layout_tag;
  uint32_t channel_bitmap;
  uint32_t number_channel_descriptions;
};

struct PACKED_STRUCTURE format_description_t {
  enum {
      is_float          = 0x01
    , is_big_endian     = 0x02
    , is_signed_integer = 0x04
    , is_packed         = 0x08
    , is_aligned_high   = 0x10
  };

  double   sample_rate;
  uint32_t format_id;
  uint32_t format_flags;
  uint32_t bytes_per_packet;
  uint32_t frames_per_packet;
  uint32_t bytes_per_frame;
  uint32_t channels_per_frame;
  uint32_t bits_per_channel;
  uint32_t reserved;
};

struct PACKED_STRUCTURE channel_layout_info_t {
  uint32_t channel_layout_info_size; // indicates the size of the channel layout data should be set to 24
  uint32_t channel_layout_info_id;   // identifier indicating that channel layout info is present value = 'chan'
  uint32_t version_flags;            // version flags value should be set to 0
  uint32_t channel_layout_tag;       // channel layout type from defined list
  uint32_t reserved1;                // unused; set to 0
  uint32_t reserved2;                // unused; set to 0
};

#if defined(COMP_MSC)
#pragma pack(pop)
#endif

}

#endif // MTX_COMMON_CAF_H
