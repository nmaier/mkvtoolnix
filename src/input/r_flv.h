/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the Flash Video (FLV) demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_R_FLV_H
#define MTX_R_FLV_H

#include "common/common_pch.h"

#include <ostream>

#include "common/byte_buffer.h"
#include "common/mm_io.h"
#include "merge/pr_generic.h"

#if defined(COMP_MSC)
#pragma pack(push,1)
#endif

struct PACKED_STRUCTURE flv_header_t {
  char signature[3];
  uint8_t version, type_flags;
  uint32_t data_offset;

  flv_header_t();

  bool read(mm_io_c *in);
  bool read(mm_io_cptr const &in);

  bool has_video() const;
  bool has_audio() const;
  bool is_valid() const;
};

inline std::ostream &
operator <<(std::ostream &out,
            flv_header_t const &h) {
  // "Cannot bind packed field to unsigned int &" if "data_offset" is used directly.
  auto local_data_offset = h.data_offset;
  out << (boost::format("[file version: %1% data offset: %2% video track present: %3% audio track present: %4%]")
          % static_cast<unsigned int>(h.version) % local_data_offset % h.has_video() % h.has_audio()).str();
  return out;
}

#if defined(COMP_MSC)
#pragma pack(pop)
#endif

class flv_tag_c {
public:
  typedef enum {
      CODEC_SORENSON_H263 = 2
    , CODEC_SCREEN_VIDEO
    , CODEC_VP6
    , CODEC_VP6_WITH_ALPHA
    , CODEC_SCREEN_VIDEO_V2
    , CODEC_H264
  } codec_type_e;

public:
  uint32_t m_previous_tag_size;
  uint8_t m_flags;
  uint64_t m_data_size, m_timecode, m_timecode_extended, m_next_position;
  bool m_ok, m_debug;

public:
  flv_tag_c();

  bool read(const mm_io_cptr &in);

  bool is_encrypted() const;
  bool is_audio() const;
  bool is_video() const;
  bool is_script_data() const;
};

inline std::ostream &
operator <<(std::ostream &out,
            flv_tag_c const &t) {
  out << (boost::format("[prev size: %1% flags: %2% data size: %3% timecode+ex: %4%/%5% next pos: %6% ok: %7%]")
          % t.m_previous_tag_size % static_cast<unsigned int>(t.m_flags) % t.m_data_size % t.m_timecode % t.m_timecode_extended % t.m_next_position % t.m_ok).str();
  return out;
}

class flv_track_c {
public:
  char m_type;                  // 'v' for video, 'a' for audio
  uint32_t m_fourcc;
  bool m_headers_read;

  memory_cptr m_payload, m_private_data, m_extra_data;

  int m_ptzr;                   // the actual packetizer instance

  int64_t m_timecode;

  // video related parameters
  unsigned int m_v_version, m_v_width, m_v_height, m_v_dwidth, m_v_dheight;
  double m_v_frame_rate, m_v_aspect_ratio;
  int64_t m_v_cts_offset;
  char m_v_frame_type;

  // audio related parameters
  unsigned int m_a_channels, m_a_sample_rate, m_a_bits_per_sample, m_a_profile;

  flv_track_c(char type);

  bool is_audio() const;
  bool is_video() const;
  bool is_valid() const;
  bool is_ptzr_set() const;
};

typedef std::shared_ptr<flv_track_c> flv_track_cptr;

class flv_reader_c: public generic_reader_c {
private:
  int m_audio_track_idx, m_video_track_idx, m_selected_track_idx;
  double m_video_frame_rate;

  flv_tag_c m_tag;

  bool m_file_done;

  std::vector<flv_track_cptr> m_tracks;

  bool m_debug;

public:
  flv_reader_c(const track_info_c &ti, const mm_io_cptr &in);
  virtual ~flv_reader_c();

  virtual bool new_stream_v_avc(flv_track_cptr &track, memory_cptr const &data);

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t tid);
  virtual void create_packetizers();

  static int probe_file(mm_io_c *io, uint64_t size);

  virtual translatable_string_c get_format_name() const;

protected:
  bool process_tag(bool skip_payload = false);
  bool process_script_tag();
  bool process_audio_tag(flv_track_cptr &track);
  bool process_audio_tag_sound_format(flv_track_cptr &track, uint8_t sound_format);
  bool process_video_tag(flv_track_cptr &track);
  bool process_video_tag_avc(flv_track_cptr &track);
  bool process_video_tag_generic(flv_track_cptr &track, flv_tag_c::codec_type_e codec_id);

  void create_a_aac_packetizer(flv_track_cptr &track);
  void create_a_mp3_packetizer(flv_track_cptr &track);
  void create_v_avc_packetizer(flv_track_cptr &track);
  void create_v_generic_packetizer(flv_track_cptr &track);

  unsigned int add_track(char type);
};

#endif // MTX_R_FLV_H
