/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the MPEG TS demultiplexer module

   Written by Massimo Callegari <massimocallegari@yahoo.it>
   and Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __R_MPEG_TS_H
#define __R_MPEG_TS_H

#include "common/common_pch.h"

#include "common/byte_buffer.h"
#include "common/dts.h"
#include "common/mm_io.h"
#include "merge/pr_generic.h"

enum mpeg_ts_input_type_e {
  INPUT_PROBE = 0,
  INPUT_READ  = 1,
};

enum mpeg_ts_pid_type_e {
  PAT_TYPE      = 0,
  PMT_TYPE      = 1,
  ES_VIDEO_TYPE = 2,
  ES_AUDIO_TYPE = 3,
  ES_SUBT_TYPE  = 4,
  ES_UNKNOWN    = 5,
};

enum mpeg_ts_stream_type_e {
  ISO_11172_VIDEO           = 0x01, // ISO/IEC 11172 Video
  ISO_13818_VIDEO           = 0x02, // ISO/IEC 13818-2 Video
  ISO_11172_AUDIO           = 0x03, // ISO/IEC 11172 Audio
  ISO_13818_AUDIO           = 0x04, // ISO/IEC 13818-3 Audio
  ISO_13818_PRIVATE         = 0x05, // ISO/IEC 13818-1 private sections
  ISO_13818_PES_PRIVATE     = 0x06, // ISO/IEC 13818-1 PES packets containing private data
  ISO_13522_MHEG            = 0x07, // ISO/IEC 13512 MHEG
  ISO_13818_DSMCC           = 0x08, // ISO/IEC 13818-1 Annex A  DSM CC
  ISO_13818_TYPE_A          = 0x0a, // ISO/IEC 13818-6 Multiprotocol encapsulation
  ISO_13818_TYPE_B          = 0x0b, // ISO/IEC 13818-6 DSM-CC U-N Messages
  ISO_13818_TYPE_C          = 0x0c, // ISO/IEC 13818-6 Stream Descriptors
  ISO_13818_TYPE_D          = 0x0d, // ISO/IEC 13818-6 Sections (any type, including private data)
  ISO_13818_AUX             = 0x0e, // ISO/IEC 13818-1 auxiliary
  ISO_13818_PART7_AUDIO     = 0x0f, // ISO/IEC 13818-7 Audio with ADTS transport sytax
  ISO_14496_PART2_VIDEO     = 0x10, // ISO/IEC 14496-2 Visual (MPEG-4)
  ISO_14496_PART3_AUDIO     = 0x11, // ISO/IEC 14496-3 Audio with LATM transport syntax
  ISO_14496_PART10_VIDEO    = 0x1b, // ISO/IEC 14496-10 Video (MPEG-4 part 10/AVC, aka H.264)
  STREAM_AUDIO_AC3          = 0x81, // Audio AC3 (A52)
  STREAM_AUDIO_DTS          = 0x82, // Audio DTS
  STREAM_AUDIO_AC3_LOSSLESS = 0x83, // Audio AC3 - Dolby lossless
  STREAM_AUDIO_AC3_PLUS     = 0x84, // Audio AC3 - Dolby Digital Plus
  STREAM_AUDIO_DTS_HD       = 0x85, // Audio DTS HD
  STREAM_VIDEO_VC1          = 0xEA, // Video VC-1
};

#if defined(COMP_MSC)
#pragma pack(push,1)
#endif

/* TS packet header */
struct PACKED_STRUCTURE mpeg_ts_packet_header_t {
  unsigned char sync_byte;
  unsigned char pid_msb:5, transport_priority:1, payload_unit_start_indicator:1, transport_error_indicator:1;
  unsigned char pid_lsb;
  unsigned char continuity_counter:4, adaptation_field_control:2, transport_scrambling_control:2;
};

/* Adaptation field */
struct PACKED_STRUCTURE mpeg_ts_adaptation_field_t {
  unsigned char adaptation_field_length;
  unsigned char adaptation_field_extension_flag:1, transport_private_data_flag:1, splicing_point_flag:1, opcr_flag:1,
                pcr_flag:1, elementary_stream_priority_indicator:1, random_access_indicator:1, discontinuity_indicator:1;
};

/* CRC */
struct PACKED_STRUCTURE mpeg_ts_crc_t {
  unsigned char crc_3msb, crc_2msb, crc_1msb, crc_lsb;
};

/* PAT header */
struct PACKED_STRUCTURE mpeg_ts_pat_t {
  unsigned char table_id;
  unsigned char section_length_msb:4, reserved:2, zero:1, section_syntax_indicator:1;
  unsigned char section_length_lsb;
  unsigned char transport_stream_id_msb;
  unsigned char transport_stream_id_lsb;
  unsigned char current_next_indicator:1, version_number:5, reserved2:2;
  unsigned char section_number;
  unsigned char last_section_number;
};

/* PAT section */
struct PACKED_STRUCTURE mpeg_ts_pat_section_t {
  unsigned char program_number_msb;
  unsigned char program_number_lsb;
  unsigned char pid_msb:5, reserved3:3;
  unsigned char pid_lsb;
};

/* PMT header */
struct PACKED_STRUCTURE mpeg_ts_pmt_t {
  unsigned char table_id;
  unsigned char section_length_msb:4, reserved:2, zero:1, section_syntax_indicator:1;
  unsigned char section_length_lsb;
  unsigned char program_number_msb;
  unsigned char program_number_lsb;
  unsigned char current_next_indicator:1, version_number:5, reserved2:2;
  unsigned char section_number;
  unsigned char last_section_number;
  unsigned char pcr_pid_msb:5, reserved3:3;
  unsigned char pcr_pid_lsb;
  unsigned char program_info_length_msb:4, reserved4:4;
  unsigned char program_info_length_lsb:8;
};

/* PMT descriptor */
struct PACKED_STRUCTURE mpeg_ts_pmt_descriptor_t {
  unsigned char tag;
  unsigned char length;
};

/* PMT pid info */
struct PACKED_STRUCTURE mpeg_ts_pmt_pid_info_t {
  unsigned char stream_type;
  unsigned char pid_msb:5, reserved:3;
  unsigned char pid_lsb;
  unsigned char es_info_length_msb:4, reserved2:4;
  unsigned char es_info_length_lsb;
};

/* PES header */
struct PACKED_STRUCTURE mpeg_ts_pes_header_t {
  unsigned char packet_start_code_prefix_2msb;
  unsigned char packet_start_code_prefix_1msb;
  unsigned char packet_start_code_prefix_lsb;
  unsigned char stream_id;
  unsigned char PES_packet_length_msb;
  unsigned char PES_packet_length_lsb;
  unsigned char original_or_copy:1, copyright:1, data_alignment_indicator:1, PES_priority:1, PES_scrambling_control:2, onezero:2;
  unsigned char PES_extension:1, PES_CRC:1, additional_copy_info:1, DSM_trick_mode:1, ES_rate:1, ESCR:1, PTS_DTS_flags:2;
  unsigned char PES_header_data_length;
  unsigned char PTS_DTS;
};

#if defined(COMP_MSC)
#pragma pack(pop)
#endif

class mpeg_ts_reader_c;

class mpeg_ts_track_c {
public:
  mpeg_ts_reader_c &reader;

  bool processed;
  mpeg_ts_pid_type_e type;          //can be PAT_TYPE, PMT_TYPE, ES_VIDEO_TYPE, ES_AUDIO_TYPE, ES_SUBT_TYPE, ES_UNKNOWN
  uint32_t fourcc;
  uint16_t pid;
  bool data_ready;
  int pes_payload_size;             // size of the current PID payload in bytes
  byte_buffer_cptr pes_payload;     // buffer with the current PID payload
  unsigned char continuity_counter; // check for PID continuity

  int ptzr;                         // the actual packetizer instance

  int64_t timecode;

  // video related parameters
  bool v_interlaced;
  int v_version, v_width, v_height, v_dwidth, v_dheight;
  double v_frame_rate, v_aspect_ratio;
  memory_cptr v_avcc, raw_seq_hdr;

  // audio related parameters
  int a_channels, a_sample_rate, a_bits_per_sample, a_bsid;
  dts_header_t a_dts_header;

  // general track parameters
  std::string language;

  mpeg_ts_track_c(mpeg_ts_reader_c &p_reader)
    : reader(p_reader)
    , processed(false)
    , type(ES_UNKNOWN)
    , fourcc(0)
    , pid(0)
    , data_ready(false)
    , pes_payload_size(0)
    , pes_payload(new byte_buffer_c)
    , continuity_counter(0)
    , ptzr(-1)
    , timecode(-1)
    , v_interlaced(false)
    , v_version(0)
    , v_width(0)
    , v_height(0)
    , v_dwidth(0)
    , v_dheight(0)
    , v_frame_rate(0)
    , v_aspect_ratio(0)
    , a_channels(0)
    , a_sample_rate(0)
    , a_bits_per_sample(0)
    , a_bsid(0)
  {
  }

  void send_to_packetizer();
  int new_stream_v_mpeg_1_2();
  int new_stream_v_avc();
  int new_stream_a_mpeg();
  int new_stream_a_ac3();
  int new_stream_a_dts();
  int new_stream_a_truehd();
};

typedef counted_ptr<mpeg_ts_track_c> mpeg_ts_track_ptr;

class mpeg_ts_reader_c: public generic_reader_c {
protected:
  mm_io_c *io;
  int64_t bytes_processed, size;
  bool PAT_found, PMT_found;
  int16_t PMT_pid;
  int es_to_process;
  int64_t m_global_timecode_offset;

  mpeg_ts_input_type_e input_status; // can be INPUT_PROBE, INPUT_READ
  int track_buffer_ready;

  bool file_done;

  std::vector<mpeg_ts_track_ptr> tracks;

  bool m_dont_use_audio_pts;

  int m_detected_packet_size;

protected:
  static int potential_packet_sizes[];

public:
  mpeg_ts_reader_c(track_info_c &_ti) throw (error_c);
  virtual ~mpeg_ts_reader_c();

  static bool probe_file(mm_io_c *io, uint64_t size);
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual int get_progress();
  virtual void create_packetizer(int64_t tid);
  virtual void create_packetizers();
  virtual void add_available_track_ids();

  virtual bool parse_packet(int id, unsigned char *buf);

  static int64_t read_timestamp(unsigned char *p);
  static int detect_packet_size(mm_io_c *io, uint64_t size);

private:
  int parse_pat(unsigned char *pat);
  int parse_pmt(unsigned char *pmt);
  bool parse_start_unit_packet(mpeg_ts_track_ptr &track, mpeg_ts_packet_header_t *ts_packet_header, unsigned char *&ts_payload, unsigned char &ts_payload_size);
  void probe_packet_complete(mpeg_ts_track_ptr &track, int tidx);

  file_status_e finish();
  int send_to_packetizer(mpeg_ts_track_ptr &track);
  void create_mpeg1_2_video_packetizer(mpeg_ts_track_ptr &track);
  void create_mpeg4_p10_es_video_packetizer(mpeg_ts_track_ptr &track);
  void create_vc1_video_packetizer(mpeg_ts_track_ptr &track);

  friend class mpeg_ts_track_c;
};

#endif  // __R_MPEG_TS_H
