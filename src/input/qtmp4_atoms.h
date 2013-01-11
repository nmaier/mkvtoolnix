/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   structs for various Quicktime and MP4 atoms

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_INPUT_QTMP4_ATOMS_H
#define MTX_INPUT_QTMP4_ATOMS_H

// 'Movie header' atom
#if defined(COMP_MSC)
#pragma pack(push,1)
#endif
typedef struct PACKED_STRUCTURE {
  uint8_t  version;
  uint8_t  flags[3];
  uint32_t creation_time;
  uint32_t modification_time;
  uint32_t time_scale;
  uint32_t duration;
  uint32_t preferred_rate;
  uint16_t preferred_volume;
  uint8_t  reserved[10];
  uint8_t  matrix_structure[36];
  uint32_t preview_time;
  uint32_t preview_duration;
  uint32_t poster_time;
  uint32_t selection_time;
  uint32_t selection_duration;
  uint32_t current_time;
  uint32_t next_track_id;
} mvhd_atom_t;

// 'Track header' atom
typedef struct PACKED_STRUCTURE {
  uint8_t  version;
  uint8_t  flags[3];
  uint32_t creation_time;
  uint32_t modification_time;
  uint32_t track_id;
  uint8_t  reserved1[4];
  uint32_t duration;
  uint8_t  reserved2[8];
  uint16_t layer;
  uint16_t alternate_group;
  uint16_t volume;
  uint8_t  reserved3[2];
  uint8_t  matrix_structure[36];
  uint32_t track_width;
  uint32_t track_height;
} tkhd_atom_t;

// 'Media header' atom
typedef struct PACKED_STRUCTURE {
  uint8_t  version;              // == 0
  uint8_t  flags[3];
  uint32_t creation_time;
  uint32_t modification_time;
  uint32_t time_scale;
  uint32_t duration;
  uint16_t language;
  uint16_t quality;
} mdhd_atom_t;

// 'Media header' atom, 64bit version
typedef struct PACKED_STRUCTURE {
  uint8_t  version;              // == 1
  uint8_t  flags[3];
  uint64_t creation_time;
  uint64_t modification_time;
  uint32_t time_scale;
  uint64_t duration;
  uint16_t language;
  uint16_t quality;
} mdhd64_atom_t;

// 'Handler reference' atom
typedef struct PACKED_STRUCTURE {
  uint8_t  version;
  uint8_t  flags[3];
  uint32_t type;
  uint32_t subtype;
  uint32_t manufacturer;
  uint32_t flags2;
  uint32_t flags_mask;
} hdlr_atom_t;

// Base for all 'sample data description' atoms
typedef struct PACKED_STRUCTURE {
  uint32_t size;
  char     fourcc[4];
  uint8_t  reserved[6];
  uint16_t data_reference_index;
} base_stsd_atom_t;

// 'sound sample description' atom
typedef struct PACKED_STRUCTURE {
  base_stsd_atom_t base;
  uint16_t         version;
  uint16_t         revision;
  uint32_t         vendor;
  uint16_t         channels;
  uint16_t         sample_size;
  int16_t          compression_id;
  uint16_t         packet_size;
  uint32_t         sample_rate;         // 32bit fixed-point number
} sound_v0_stsd_atom_t;

// 'sound sample description' atom v1
typedef struct PACKED_STRUCTURE {
  sound_v0_stsd_atom_t v0;
  struct PACKED_STRUCTURE {
    uint32_t samples_per_packet;
    uint32_t bytes_per_packet;
    uint32_t bytes_per_frame;
    uint32_t bytes_per_sample;
  } v1;
} sound_v1_stsd_atom_t;

// 'sound sample description' atom v2
typedef struct PACKED_STRUCTURE {
  sound_v0_stsd_atom_t v0;
  struct PACKED_STRUCTURE {
    uint32_t v2_struct_size;
    uint64_t sample_rate;       // 64bit float
    // uint32_t unknown1;
    uint32_t channels;

    // 16
    uint32_t const1;            // always 0x7f000000
    uint32_t bits_per_channel;  // for uncompressed audio
    uint32_t flags;
    uint32_t bytes_per_frame;   // if constant
    uint32_t samples_per_frame; // lpcm frames per audio packet if constant
  } v2;
} sound_v2_stsd_atom_t;

// 'video sample description' atom
typedef struct PACKED_STRUCTURE {
  base_stsd_atom_t base;
  uint16_t         version;
  uint16_t         revision;
  uint32_t         vendor;
  uint32_t         temporal_quality;
  uint32_t         spatial_quality;
  uint16_t         width;
  uint16_t         height;
  uint32_t         horizontal_resolution; // 32bit fixed-point number
  uint32_t         vertical_resolution;   // 32bit fixed-point number
  uint32_t         data_size;
  uint16_t         frame_count;
  char             compressor_name[32];
  uint16_t         depth;
  uint16_t         color_table_id;
} video_stsd_atom_t;

typedef struct PACKED_STRUCTURE {
  uint32_t size;
  video_stsd_atom_t id;
} qt_image_description_t;
#if defined(COMP_MSC)
#pragma pack(pop)
#endif

// one byte tag identifiers
#define MP4DT_O                 0x01
#define MP4DT_IO                0x02
#define MP4DT_ES                0x03
#define MP4DT_DEC_CONFIG        0x04
#define MP4DT_DEC_SPECIFIC      0x05
#define MP4DT_SL_CONFIG         0x06
#define MP4DT_CONTENT_ID        0x07
#define MP4DT_SUPPL_CONTENT_ID  0x08
#define MP4DT_IP_PTR            0x09
#define MP4DT_IPMP_PTR          0x0A
#define MP4DT_IPMP              0x0B
#define MP4DT_REGISTRATION      0x0D
#define MP4DT_ESID_INC          0x0E
#define MP4DT_ESID_REF          0x0F
#define MP4DT_FILE_IO           0x10
#define MP4DT_FILE_O            0x11
#define MP4DT_EXT_PROFILE_LEVEL 0x13
#define MP4DT_TAGS_START        0x80
#define MP4DT_TAGS_END          0xFE

// MPEG4 esds structure
typedef struct {
  uint8_t        version;
  uint32_t       flags;
  uint16_t       esid;
  uint8_t        stream_priority;
  uint8_t        object_type_id;
  uint8_t        stream_type;
  uint32_t       buffer_size_db;
  uint32_t       max_bitrate;
  uint32_t       avg_bitrate;
  memory_cptr    decoder_config;
  memory_cptr    sl_config;
} esds_t;

// Object type identifications.
// See http://gpac.sourceforge.net/tutorial/mediatypes.htm
#define MP4OTI_MPEG4Systems1                   0x01
#define MP4OTI_MPEG4Systems2                   0x02
#define MP4OTI_MPEG4Visual                     0x20
#define MP4OTI_MPEG4Audio                      0x40
#define MP4OTI_MPEG2VisualSimple               0x60
#define MP4OTI_MPEG2VisualMain                 0x61
#define MP4OTI_MPEG2VisualSNR                  0x62
#define MP4OTI_MPEG2VisualSpatial              0x63
#define MP4OTI_MPEG2VisualHigh                 0x64
#define MP4OTI_MPEG2Visual422                  0x65
#define MP4OTI_MPEG2AudioMain                  0x66
#define MP4OTI_MPEG2AudioLowComplexity         0x67
#define MP4OTI_MPEG2AudioScaleableSamplingRate 0x68
#define MP4OTI_MPEG2AudioPart3                 0x69
#define MP4OTI_MPEG1Visual                     0x6A
#define MP4OTI_MPEG1Audio                      0x6B
#define MP4OTI_JPEG                            0x6C
#define MP4OTI_VOBSUB                          0xE0

#endif // MTX_INPUT_QTMP4_ATOMS_H
