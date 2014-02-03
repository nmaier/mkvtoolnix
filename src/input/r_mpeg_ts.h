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

#ifndef MTX_R_MPEG_TS_H
#define MTX_R_MPEG_TS_H

#include "common/common_pch.h"

#include "common/aac.h"
#include "common/byte_buffer.h"
#include "common/codec.h"
#include "common/endian.h"
#include "common/dts.h"
#include "common/mm_io.h"
#include "common/mpeg4_p10.h"
#include "common/truehd.h"
#include "merge/pr_generic.h"
#include "mpegparser/M2VParser.h"

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
  STREAM_AUDIO_AC3_PLUS     = 0x84, // Audio AC3 - Dolby Digital Plus (EAC3)
  STREAM_AUDIO_DTS_HD       = 0x85, // Audio DTS HD
  STREAM_AUDIO_DTS_HD_MA    = 0x86, // Audio DTS HD Master Audio
  STREAM_VIDEO_VC1          = 0xEA, // Video VC-1
  STREAM_SUBTITLES_HDMV_PGS = 0x90, // HDMV PGS subtitles
};

#if defined(COMP_MSC)
#pragma pack(push,1)
#endif

// TS packet header
struct PACKED_STRUCTURE mpeg_ts_packet_header_t {
  unsigned char sync_byte;
  unsigned char pid_msb_flags1; // msb:5 transport_priority:1 payload_unit_start_indicator:1 transport_error_indicator:1
  unsigned char pid_lsb;
  unsigned char flags2;         // continuity_counter:4 adaptation_field_control:2 transport_scrambling_control:2

  uint16_t get_pid() {
    return ((static_cast<uint16_t>(pid_msb_flags1) & 0x1f) << 8) | static_cast<uint16_t>(pid_lsb);
  }

  unsigned char get_payload_unit_start_indicator() {
    return (pid_msb_flags1 & 0x40) >> 6;
  }

  unsigned char get_transport_error_indicator() {
    return (pid_msb_flags1 & 0x80) >> 7;
  }

  unsigned char get_continuity_counter() {
    return flags2 & 0x0f;
  }

  unsigned char get_adaptation_field_control() {
    return (flags2 & 0x30) >> 4;
  }
};

// Adaptation field
struct PACKED_STRUCTURE mpeg_ts_adaptation_field_t {
  unsigned char length;
  unsigned char flags;  // extension:1 transport_private_data:1 splicing_point:1 opcr:1 pcr:1 elementary_stream_priority_indicator:1 random_access_indicator:1 discontinuity_indicator:1

  unsigned char get_discontinuity_indicator() {
    return (flags & 80) >> 7;
  }
};

// PAT header
struct PACKED_STRUCTURE mpeg_ts_pat_t {
  unsigned char table_id;
  unsigned char section_length_msb_flags1; // msb:4 reserved:2 zero:1 section_syntax_indicator:1
  unsigned char section_length_lsb;
  unsigned char transport_stream_id_msb;
  unsigned char transport_stream_id_lsb;
  unsigned char flags2; // current_next_indicator:1 version_number:5 reserved2:2
  unsigned char section_number;
  unsigned char last_section_number;

  uint16_t get_section_length() {
    return ((static_cast<uint16_t>(section_length_msb_flags1) & 0x0f) << 8) | static_cast<uint16_t>(section_length_lsb);
  }

  unsigned char get_section_syntax_indicator() {
    return (section_length_msb_flags1 & 0x80) >> 7;
  }

  unsigned char get_current_next_indicator() {
    return flags2 & 0x01;
  }
};

// PAT section
struct PACKED_STRUCTURE mpeg_ts_pat_section_t {
  uint16_t program_number;
  unsigned char pid_msb_flags;
  unsigned char pid_lsb;

  uint16_t get_pid() {
    return ((static_cast<uint16_t>(pid_msb_flags) & 0x1f) << 8) | static_cast<uint16_t>(pid_lsb);
  }

  uint16_t get_program_number() {
    return get_uint16_be(&program_number);
  }
};

// PMT header
struct PACKED_STRUCTURE mpeg_ts_pmt_t {
  unsigned char table_id;
  unsigned char section_length_msb_flags1; // msb:4 reserved:2 zero:1 section_syntax_indicator:1
  unsigned char section_length_lsb;
  uint16_t program_number;
  unsigned char flags2; // current_next_indicator:1 version_number:5 reserved2:2
  unsigned char section_number;
  unsigned char last_section_number;
  unsigned char pcr_pid_msb_flags;
  unsigned char pcr_pid_lsb;
  unsigned char program_info_length_msb_reserved; // msb:4 reserved4:4
  unsigned char program_info_length_lsb;

  uint16_t get_pcr_pid() {
    return ((static_cast<uint16_t>(pcr_pid_msb_flags) & 0x1f) << 8) | static_cast<uint16_t>(pcr_pid_lsb);
  }

  uint16_t get_section_length() {
    return ((static_cast<uint16_t>(section_length_msb_flags1) & 0x0f) << 8) | static_cast<uint16_t>(section_length_lsb);
  }

  uint16_t get_program_info_length() {
    return ((static_cast<uint16_t>(program_info_length_msb_reserved) & 0x0f) << 8) | static_cast<uint16_t>(program_info_length_lsb);
  }

  unsigned char get_section_syntax_indicator() {
    return (section_length_msb_flags1 & 0x80) >> 7;
  }

  unsigned char get_current_next_indicator() {
    return flags2 & 0x01;
  }

  uint16_t get_program_number() {
    return get_uint16_be(&program_number);
  }
};

// PMT descriptor
struct PACKED_STRUCTURE mpeg_ts_pmt_descriptor_t {
  unsigned char tag;
  unsigned char length;
};

// PMT pid info
struct PACKED_STRUCTURE mpeg_ts_pmt_pid_info_t {
  unsigned char stream_type;
  unsigned char pid_msb_flags;
  unsigned char pid_lsb;
  unsigned char es_info_length_msb_flags;
  unsigned char es_info_length_lsb;

  uint16_t get_pid() {
    return ((static_cast<uint16_t>(pid_msb_flags) & 0x1f) << 8) | static_cast<uint16_t>(pid_lsb);
  }

  uint16_t get_es_info_length() {
    return ((static_cast<uint16_t>(es_info_length_msb_flags) & 0x0f) << 8) | static_cast<uint16_t>(es_info_length_lsb);
  }
};

// PES header
struct PACKED_STRUCTURE mpeg_ts_pes_header_t {
  unsigned char packet_start_code[3];
  unsigned char stream_id;
  uint16_t pes_packet_length;
  unsigned char flags1; // original_or_copy:1 copyright:1 data_alignment_indicator:1 pes_priority:1 pes_scrambling_control:2 onezero:2
  unsigned char flags2; // pes_extension:1 pes_crc:1 additional_copy_info:1 dsm_trick_mode:1 es_rate:1 escr:1 pts_dts_flags:2
  unsigned char pes_header_data_length;
  unsigned char pts_dts;

  uint16_t get_pes_packet_length() {
    return get_uint16_be(&pes_packet_length);
  }

  unsigned char get_pes_extension() {
    return flags2 & 0x01;
  }

  unsigned char get_pes_crc() {
    return (flags2 & 0x02) >> 1;
  }

  unsigned char get_additional_copy_info() {
    return (flags2 & 0x04) >> 2;
  }

  unsigned char get_dsm_trick_mode() {
    return (flags2 & 0x08) >> 3;
  }

  unsigned char get_es_rate() {
    return (flags2 & 0x10) >> 4;
  }

  unsigned char get_escr() {
    return (flags2 & 0x20) >> 5;
  }

  unsigned char get_pts_dts_flags() {
    return (flags2 & 0xc0) >> 6;
  }
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
  codec_c codec;
  uint16_t pid;
  bool data_ready;
  int pes_payload_size;             // size of the current PID payload in bytes
  byte_buffer_cptr pes_payload;     // buffer with the current PID payload
  unsigned char continuity_counter; // check for PID continuity

  bool probed_ok;
  int ptzr;                         // the actual packetizer instance

  timecode_c m_timecode, m_previous_timecode, m_previous_valid_timecode, m_timecode_wrap_add;

  // video related parameters
  bool v_interlaced;
  int v_version, v_width, v_height, v_dwidth, v_dheight;
  double v_frame_rate, v_aspect_ratio;
  memory_cptr raw_seq_hdr;

  // audio related parameters
  int a_channels, a_sample_rate, a_bits_per_sample, a_bsid;
  dts_header_t a_dts_header;
  aac_header_c m_aac_header;

  bool m_apply_dts_timecode_fix, m_use_dts, m_timecodes_wrapped;

  // general track parameters
  std::string language;

  // used for probing for stream types
  byte_buffer_cptr m_probe_data;
  mpeg4::p10::avc_es_parser_cptr m_avc_parser;
  truehd_parser_cptr m_truehd_parser;
  std::shared_ptr<M2VParser> m_m2v_parser;

  bool m_debug_delivery, m_debug_timecode_wrapping;

  mpeg_ts_track_c(mpeg_ts_reader_c &p_reader)
    : reader(p_reader)
    , processed(false)
    , type(ES_UNKNOWN)
    , pid(0)
    , data_ready(false)
    , pes_payload_size(0)
    , pes_payload(new byte_buffer_c)
    , continuity_counter(0)
    , probed_ok(false)
    , ptzr(-1)
    , m_timecode_wrap_add{timecode_c::ns(0)}
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
    , m_apply_dts_timecode_fix(false)
    , m_use_dts(false)
    , m_timecodes_wrapped{false}
    , m_debug_delivery(false)
    , m_debug_timecode_wrapping{}
  {
  }

  void send_to_packetizer();
  void add_pes_payload(unsigned char *ts_payload, size_t ts_payload_size);
  void add_pes_payload_to_probe_data();

  int new_stream_v_mpeg_1_2();
  int new_stream_v_avc();
  int new_stream_v_vc1();
  int new_stream_a_mpeg();
  int new_stream_a_aac();
  int new_stream_a_ac3();
  int new_stream_a_dts();
  int new_stream_a_truehd();

  void set_pid(uint16_t new_pid);

  void handle_timecode_wrap(timecode_c &pts, timecode_c &dts);
  bool detect_timecode_wrap(timecode_c &timecode);
};

typedef std::shared_ptr<mpeg_ts_track_c> mpeg_ts_track_ptr;

class mpeg_ts_reader_c: public generic_reader_c {
protected:
  bool PAT_found, PMT_found;
  int16_t PMT_pid;
  int es_to_process;
  timecode_c m_global_timecode_offset, m_stream_timecode;

  mpeg_ts_input_type_e input_status; // can be INPUT_PROBE, INPUT_READ
  int track_buffer_ready;

  bool file_done, m_packet_sent_to_packetizer;

  std::vector<mpeg_ts_track_ptr> tracks;
  std::map<generic_packetizer_c *, mpeg_ts_track_ptr> m_ptzr_to_track_map;

  std::vector<timecode_c> m_chapter_timecodes;

  debugging_option_c m_dont_use_audio_pts, m_debug_resync, m_debug_pat_pmt, m_debug_aac, m_debug_timecode_wrapping, m_debug_clpi;

  int m_detected_packet_size;

protected:
  static int potential_packet_sizes[];

public:
  mpeg_ts_reader_c(const track_info_c &ti, const mm_io_cptr &in);
  virtual ~mpeg_ts_reader_c();

  static bool probe_file(mm_io_c *in, uint64_t size);

  virtual translatable_string_c get_format_name() const {
    return YT("MPEG transport stream");
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *requested_ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t tid);
  virtual void create_packetizers();
  virtual void add_available_track_ids();

  virtual bool parse_packet(unsigned char *buf);

  static timecode_c read_timecode(unsigned char *p);
  static int detect_packet_size(mm_io_c *in, uint64_t size);

private:
  int parse_pat(unsigned char *pat);
  int parse_pmt(unsigned char *pmt);
  bool parse_start_unit_packet(mpeg_ts_track_ptr &track, mpeg_ts_packet_header_t *ts_packet_header, unsigned char *&ts_payload, unsigned char &ts_payload_size);
  void probe_packet_complete(mpeg_ts_track_ptr &track);

  file_status_e finish();
  int send_to_packetizer(mpeg_ts_track_ptr &track);
  void create_mpeg1_2_video_packetizer(mpeg_ts_track_ptr &track);
  void create_mpeg4_p10_es_video_packetizer(mpeg_ts_track_ptr &track);
  void create_vc1_video_packetizer(mpeg_ts_track_ptr &track);
  void create_hdmv_pgs_subtitles_packetizer(mpeg_ts_track_ptr &track);

  bfs::path find_clip_info_file();
  void parse_clip_info_file();

  void process_chapter_entries();

  bool resync(int64_t start_at);

  friend class mpeg_ts_track_c;
};

#endif  // MTX_R_MPEG_TS_H
