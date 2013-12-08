/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the Quicktime & MP4 reader

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_INPUT_R_QTMP4_H
#define MTX_INPUT_R_QTMP4_H

#include "common/common_pch.h"

#include "common/ac3.h"
#include "common/fourcc.h"
#include "common/mm_io.h"
#include "output/p_video.h"
#include "input/qtmp4_atoms.h"

struct qt_durmap_t {
  uint32_t number;
  uint32_t duration;

  qt_durmap_t()
    : number{}
    , duration{}
  {
  }
};

struct qt_chunk_t {
  uint32_t samples;
  uint32_t size;
  uint32_t desc;
  uint64_t pos;

  qt_chunk_t()
    : samples{}
    , size{}
    , desc{}
    , pos{}
  {
  }
};

struct qt_chunkmap_t {
  uint32_t first_chunk;
  uint32_t samples_per_chunk;
  uint32_t sample_description_id;

  qt_chunkmap_t()
    : first_chunk{}
    , samples_per_chunk{}
    , sample_description_id{}
  {
  }
};

struct qt_editlist_t {
  int64_t  duration;
  int64_t  pos;
  uint32_t speed;
  uint32_t frames;
  int64_t  start_sample;
  int64_t  start_frame;
  int64_t  pts_offset;

  qt_editlist_t()
    : duration{}
    , pos{}
    , speed{}
    , frames{}
    , start_sample{}
    , start_frame{}
    , pts_offset{}
  {
  }
};

struct qt_sample_t {
  int64_t  pts;
  uint32_t size;
  int64_t  pos;

  qt_sample_t()
    : pts{}
    , size{}
    , pos{}
  {
  }
};

struct qt_frame_offset_t {
  uint32_t count;
  uint32_t offset;

  qt_frame_offset_t()
    : count{}
    , offset{}
  {
  }
};

struct qt_index_t {
  int64_t file_pos, size;
  int64_t timecode, duration;
  bool    is_keyframe;

  qt_index_t()
    : file_pos{}
    , size{}
    , timecode{}
    , duration{}
    , is_keyframe{}
  {
  };

  qt_index_t(int64_t p_file_pos, int64_t p_size, int64_t p_timecode, int64_t p_duration, bool p_is_keyframe)
    : file_pos{p_file_pos}
    , size{p_size}
    , timecode{p_timecode}
    , duration{p_duration}
    , is_keyframe{p_is_keyframe}
  {
  }
};

struct qtmp4_demuxer_c {
  bool ok;

  char type;
  uint32_t id, container_id;
  fourcc_c fourcc;
  uint32_t pos;

  int64_t time_scale, duration, global_duration, constant_editlist_offset_ns;
  uint32_t sample_size;

  std::vector<qt_sample_t> sample_table;
  std::vector<qt_chunk_t> chunk_table;
  std::vector<qt_chunkmap_t> chunkmap_table;
  std::vector<qt_durmap_t> durmap_table;
  std::vector<uint32_t> keyframe_table;
  std::vector<qt_editlist_t> editlist_table;
  std::vector<qt_frame_offset_t> raw_frame_offset_table;
  std::vector<int32_t> frame_offset_table;

  std::vector<int64_t> timecodes, durations, frame_indices;

  std::vector<qt_index_t> m_index;

  double fps;

  esds_t esds;
  bool esds_parsed;

  memory_cptr stsd;
  unsigned int stsd_non_priv_struct_size;
  uint32_t v_width, v_height, v_bitdepth;
  std::deque<int64_t> references;
  bool v_is_avc, avc_use_bframes;
  uint32_t a_channels, a_bitdepth;
  float a_samplerate;
  int a_aac_profile, a_aac_output_sample_rate;
  bool a_aac_is_sbr, a_aac_config_parsed;
  ac3::frame_c m_ac3_header;

  memory_cptr priv;

  bool warning_printed;

  int ptzr;

  std::string language;

  debugging_option_c m_debug_tables, m_debug_fps, m_debug_headers, m_debug_editlists;

  qtmp4_demuxer_c()
    : ok{false}
    , type{'?'}
    , id{0}
    , container_id{0}
    , pos{0}
    , time_scale{1}
    , duration{0}
    , global_duration{0}
    , constant_editlist_offset_ns{0}
    , sample_size{0}
    , fps{0.0}
    , esds_parsed{false}
    , stsd_non_priv_struct_size{}
    , v_width{0}
    , v_height{0}
    , v_bitdepth{0}
    , v_is_avc{false}
    , avc_use_bframes{false}
    , a_channels{0}
    , a_bitdepth{0}
    , a_samplerate{0.0}
    , a_aac_profile{0}
    , a_aac_output_sample_rate{0}
    , a_aac_is_sbr{false}
    , a_aac_config_parsed{false}
    , warning_printed{false}
    , ptzr{-1}
    , m_debug_tables{         "qtmp4_full|qtmp4_tables"}
    , m_debug_fps{      "qtmp4|qtmp4_full|qtmp4_fps"}
    , m_debug_headers{  "qtmp4|qtmp4_full|qtmp4_headers"}
    , m_debug_editlists{"qtmp4|qtmp4_full|qtmp4_editlists"}
  {
    memset(&esds, 0, sizeof(esds_t));
  }

  ~qtmp4_demuxer_c() {
  }

  void calculate_fps();
  int64_t to_nsecs(int64_t value);
  void calculate_timecodes();
  void adjust_timecodes(int64_t delta);

  bool update_tables(int64_t global_time_scale);
  void update_editlist_table(int64_t global_time_scale);

  void build_index();

  bool read_first_bytes(memory_cptr &buf, int num_bytes, mm_io_cptr in);

  bool is_audio() const;
  bool is_video() const;
  bool is_subtitles() const;
  bool is_chapters() const;
  bool is_unknown() const;

  void handle_stsd_atom(uint64_t atom_size, int level);
  void handle_audio_stsd_atom(uint64_t atom_size, int level);
  void handle_video_stsd_atom(uint64_t atom_size, int level);
  void handle_subtitles_stsd_atom(uint64_t atom_size, int level);

  void parse_audio_header_priv_atoms(uint64_t atom_size, int level);
  void parse_video_header_priv_atoms(uint64_t atom_size, int level);
  void parse_subtitles_header_priv_atoms(uint64_t atom_size, int level);

  bool verify_audio_parameters();
  bool verify_alac_audio_parameters();
  bool verify_mp4a_audio_parameters();

  bool verify_video_parameters();
  bool verify_avc_video_parameters();
  bool verify_mp4v_video_parameters();

  bool verify_subtitles_parameters();
  bool verify_vobsub_subtitles_parameters();

  int64_t min_timecode() const;

private:
  void build_index_chunk_mode();
  void build_index_constant_sample_size_mode();

  void calculate_timecodes_constant_sample_size();
  void calculate_timecodes_variable_sample_size();

  bool parse_esds_atom(mm_mem_io_c &memio, int level);
  uint32_t read_esds_descr_len(mm_mem_io_c &memio);
};
typedef std::shared_ptr<qtmp4_demuxer_c> qtmp4_demuxer_cptr;

struct qt_atom_t {
  fourcc_c fourcc;
  uint64_t size;
  uint64_t pos;
  uint32_t hsize;

  qt_atom_t()
    : fourcc{}
    , size{}
    , pos{}
    , hsize{}
  {
  }

  qt_atom_t to_parent() {
    qt_atom_t parent;

    parent.fourcc = fourcc;
    parent.size   = size - hsize;
    parent.pos    = pos  + hsize;
    return parent;
  }
};

inline std::ostream &
operator <<(std::ostream &out,
            qt_atom_t const &atom) {
  out << (boost::format("<%1% at %2% size %3% hsize %4%>") % atom.fourcc % atom.pos % atom.size % atom.hsize);
  return out;
}

struct qtmp4_chapter_entry_t {
  std::string m_name;
  int64_t m_timecode;

  qtmp4_chapter_entry_t()
    : m_timecode{}
  {
  }

  qtmp4_chapter_entry_t(const std::string &name,
                        int64_t timecode)
    : m_name{name}
    , m_timecode{timecode}
  {
  }

  bool operator <(const qtmp4_chapter_entry_t &cmp) const {
    return m_timecode < cmp.m_timecode;
  }
};

class qtmp4_reader_c: public generic_reader_c {
private:
  std::vector<qtmp4_demuxer_cptr> m_demuxers;
  std::unordered_map<unsigned int, bool> m_chapter_track_ids;
  int64_t m_mdat_pos, m_mdat_size;
  uint32_t m_time_scale;
  fourcc_c m_compression_algorithm;
  int m_main_dmx;

  unsigned int m_audio_encoder_delay_samples;

  debugging_option_c m_debug_chapters, m_debug_headers, m_debug_tables, m_debug_interleaving, m_debug_resync;

public:
  qtmp4_reader_c(const track_info_c &ti, const mm_io_cptr &in);
  virtual ~qtmp4_reader_c();

  virtual translatable_string_c get_format_name() const {
    return YT("QuickTime/MP4");
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false);
  virtual int get_progress();
  virtual void identify();
  virtual void create_packetizers();
  virtual void create_packetizer(int64_t tid);
  virtual void add_available_track_ids();

  static int probe_file(mm_io_c *in, uint64_t size);

protected:
  virtual void parse_headers();
  virtual void calculate_timecodes();
  virtual qt_atom_t read_atom(mm_io_c *read_from = nullptr, bool exit_on_error = true);
  virtual bool resync_to_top_level_atom(uint64_t start_pos);
  virtual void parse_itunsmpb(std::string data);

  virtual void handle_cmov_atom(qt_atom_t parent, int level);
  virtual void handle_cmvd_atom(qt_atom_t parent, int level);
  virtual void handle_ctts_atom(qtmp4_demuxer_cptr &new_dmx, qt_atom_t parent, int level);
  virtual void handle_dcom_atom(qt_atom_t parent, int level);
  virtual void handle_hdlr_atom(qtmp4_demuxer_cptr &new_dmx, qt_atom_t parent, int level);
  virtual void handle_mdhd_atom(qtmp4_demuxer_cptr &new_dmx, qt_atom_t parent, int level);
  virtual void handle_mdia_atom(qtmp4_demuxer_cptr &new_dmx, qt_atom_t parent, int level);
  virtual void handle_minf_atom(qtmp4_demuxer_cptr &new_dmx, qt_atom_t parent, int level);
  virtual void handle_moov_atom(qt_atom_t parent, int level);
  virtual void handle_mvhd_atom(qt_atom_t parent, int level);
  virtual void handle_udta_atom(qt_atom_t parent, int level);
  virtual void handle_chpl_atom(qt_atom_t parent, int level);
  virtual void handle_meta_atom(qt_atom_t parent, int level);
  virtual void handle_ilst_atom(qt_atom_t parent, int level);
  virtual void handle_4dashes_atom(qt_atom_t parent, int level);
  virtual void handle_stbl_atom(qtmp4_demuxer_cptr &new_dmx, qt_atom_t parent, int level);
  virtual void handle_stco_atom(qtmp4_demuxer_cptr &new_dmx, qt_atom_t parent, int level);
  virtual void handle_co64_atom(qtmp4_demuxer_cptr &new_dmx, qt_atom_t parent, int level);
  virtual void handle_stsc_atom(qtmp4_demuxer_cptr &new_dmx, qt_atom_t parent, int level);
  virtual void handle_stsd_atom(qtmp4_demuxer_cptr &new_dmx, qt_atom_t parent, int level);
  virtual void handle_stss_atom(qtmp4_demuxer_cptr &new_dmx, qt_atom_t parent, int level);
  virtual void handle_stsz_atom(qtmp4_demuxer_cptr &new_dmx, qt_atom_t parent, int level);
  virtual void handle_sttd_atom(qtmp4_demuxer_cptr &new_dmx, qt_atom_t parent, int level);
  virtual void handle_stts_atom(qtmp4_demuxer_cptr &new_dmx, qt_atom_t parent, int level);
  virtual void handle_tkhd_atom(qtmp4_demuxer_cptr &new_dmx, qt_atom_t parent, int level);
  virtual void handle_trak_atom(qtmp4_demuxer_cptr &new_dmx, qt_atom_t parent, int level);
  virtual void handle_edts_atom(qtmp4_demuxer_cptr &new_dmx, qt_atom_t parent, int level);
  virtual void handle_elst_atom(qtmp4_demuxer_cptr &new_dmx, qt_atom_t parent, int level);
  virtual void handle_tref_atom(qtmp4_demuxer_cptr &new_dmx, qt_atom_t parent, int level);

  virtual memory_cptr create_bitmap_info_header(qtmp4_demuxer_cptr &dmx, const char *fourcc, size_t extra_size = 0, const void *extra_data = nullptr);

  virtual void create_audio_packetizer_aac(qtmp4_demuxer_cptr &dmx);
  virtual bool create_audio_packetizer_ac3(qtmp4_demuxer_cptr &dmx);
  virtual bool create_audio_packetizer_alac(qtmp4_demuxer_cptr &dmx);
  virtual void create_audio_packetizer_mp3(qtmp4_demuxer_cptr &dmx);
  virtual void create_audio_packetizer_passthrough(qtmp4_demuxer_cptr &dmx);
  virtual void create_audio_packetizer_pcm(qtmp4_demuxer_cptr &dmx);

  virtual void create_video_packetizer_mpeg1_2(qtmp4_demuxer_cptr &dmx);
  virtual void create_video_packetizer_mpeg4_p10(qtmp4_demuxer_cptr &dmx);
  virtual void create_video_packetizer_mpeg4_p2(qtmp4_demuxer_cptr &dmx);
  virtual void create_video_packetizer_standard(qtmp4_demuxer_cptr &dmx);
  virtual void create_video_packetizer_svq1(qtmp4_demuxer_cptr &dmx);

  virtual void create_subtitles_packetizer_vobsub(qtmp4_demuxer_cptr &dmx);

  virtual void handle_audio_encoder_delay(qtmp4_demuxer_cptr &dmx);

  virtual std::string decode_and_verify_language(uint16_t coded_language);
  virtual void read_chapter_track();
  virtual void recode_chapter_entries(std::vector<qtmp4_chapter_entry_t> &entries);
  virtual void process_chapter_entries(int level, std::vector<qtmp4_chapter_entry_t> &entries);

  virtual void detect_interleaving();

  virtual std::string read_string_atom(qt_atom_t atom, size_t num_skipped);
};

#endif  // MTX_INPUT_R_QTMP4_H
