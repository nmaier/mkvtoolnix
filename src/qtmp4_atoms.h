/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  qtmp4_atoms.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief structs for various Quicktime and MP4 atoms
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __QTMP4_ATOMS_H
#define __QTMP4_ATOMS_H

#pragma pack(1)

// 'Movie header' atom
typedef struct {
  uint8_t version;
  uint8_t flags[3];
  uint32_t creation_time;
  uint32_t modification_time;
  uint32_t time_scale;
  uint32_t duration;
  uint32_t preferred_rate;
  uint16_t preferred_volume;
  uint8_t reserved[10];
  uint8_t matrix_structure[36];
  uint32_t preview_time;
  uint32_t preview_duration;
  uint32_t poster_time;
  uint32_t selection_time;
  uint32_t selection_duration;
  uint32_t current_time;
  uint32_t next_track_id;
} mvhd_atom_t;

// 'Track header' atom
typedef struct {
  uint8_t version;
  uint8_t flags[3];
  uint32_t creation_time;
  uint32_t modification_time;
  uint32_t track_id;
  uint8_t reserved1[4];
  uint32_t duration;
  uint8_t reserved2[8];
  uint16_t layer;
  uint16_t alternate_group;
  uint16_t volume;
  uint8_t reserved3[2];
  uint8_t matrix_structure[36];
  uint32_t track_width;
  uint32_t track_height;
} tkhd_atom_t;

// 'Media header' atom
typedef struct {
  uint8_t version;
  uint8_t flags[3];
  uint32_t creation_time;
  uint32_t modification_time;
  uint32_t time_scale;
  uint32_t duration;
  uint16_t language;
  uint16_t quality;
} mdhd_atom_t;

// 'Handler reference' atom
typedef struct {
  uint8_t version;
  uint8_t flags[3];
  uint32_t type;
  uint32_t subtype;
  uint32_t manufacturer;
  uint32_t flags2;
  uint32_t flags_mask;
} hdlr_atom_t;

// Base for all 'sample data description' atoms
typedef struct {
  char fourcc[4];
  uint8_t reserved[6];
  uint16_t data_reference_index;
} base_stsd_atom_t;

// 'sound sample description' atom
typedef struct {
  base_stsd_atom_t base;
  uint16_t version;
  uint16_t revision;
  uint32_t vendor;
  uint16_t channels;
  uint16_t sample_size;
  int16_t compression_id;
  uint16_t packet_size;
  uint32_t sample_rate;         // 32bit fixed-point number
} sound_v0_stsd_atom_t;

// 'sound sample description' atom v2
typedef struct {
  sound_v0_stsd_atom_t v0;
  struct {
    uint32_t samples_per_packet;
    uint32_t bytes_per_packet;
    uint32_t bytes_per_frame;
    uint32_t bytes_per_sample;
  } v1;
} sound_v1_stsd_atom_t;

// 'video sample description' atom
typedef struct {
  base_stsd_atom_t base;
  uint16_t version;
  uint16_t revision;
  uint32_t vendor;
  uint32_t temporal_quality;
  uint32_t spatial_quality;
  uint16_t width;
  uint16_t height;
  uint32_t horizontal_resolution; // 32bit fixed-point number
  uint32_t vertical_resolution; // 32bit fixed-point number
  uint32_t data_size;
  uint16_t frame_count;
  char compressor_name[32];
  uint16_t depth;
  uint16_t color_table_id;
} video_stsd_atom_t;

typedef struct {
  uint32_t size;
  video_stsd_atom_t id;
} qt_image_description_t;

#endif // __QTMP4_ATOMS_H
