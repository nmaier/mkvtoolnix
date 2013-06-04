/** MPEG video helper functions (MPEG 1, 2 and 4)

   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file

*/

#ifndef MTX_COMMON_HEVC_H
#define MTX_COMMON_HEVC_H

#include "common/common_pch.h"

#include "common/math.h"

#define NALU_START_CODE 0x00000001

// VCL NALs
#define HEVC_NALU_TYPE_TRAIL_N        0
#define HEVC_NALU_TYPE_TRAIL_R        1
#define HEVC_NALU_TYPE_TSA_N          2
#define HEVC_NALU_TYPE_TSA_R          3
#define HEVC_NALU_TYPE_STSA_N         4
#define HEVC_NALU_TYPE_STSA_R         5
#define HEVC_NALU_TYPE_RADL_N         6
#define HEVC_NALU_TYPE_RADL_R         7
#define HEVC_NALU_TYPE_RASL_N         8
#define HEVC_NALU_TYPE_RASL_R         9
#define HEVC_NALU_TYPE_RSV_VCL_N10    10
#define HEVC_NALU_TYPE_RSV_VCL_N12    12
#define HEVC_NALU_TYPE_RSV_VCL_N14    14
#define HEVC_NALU_TYPE_RSV_VCL_R11    11
#define HEVC_NALU_TYPE_RSV_VCL_R13    13
#define HEVC_NALU_TYPE_RSV_VCL_R15    15
#define HEVC_NALU_TYPE_BLA_W_LP       16
#define HEVC_NALU_TYPE_BLA_W_RADL     17
#define HEVC_NALU_TYPE_BLA_N_LP       18
#define HEVC_NALU_TYPE_IDR_W_RADL     19
#define HEVC_NALU_TYPE_IDR_N_LP       20
#define HEVC_NALU_TYPE_CRA_NUT        21
#define HEVC_NALU_TYPE_RSV_RAP_VCL22  22
#define HEVC_NALU_TYPE_RSV_RAP_VCL23  23
#define HEVC_NALU_TYPE_RSV_VCL24      24
#define HEVC_NALU_TYPE_RSV_VCL25      25
#define HEVC_NALU_TYPE_RSV_VCL26      26
#define HEVC_NALU_TYPE_RSV_VCL27      27
#define HEVC_NALU_TYPE_RSV_VCL28      28
#define HEVC_NALU_TYPE_RSV_VCL29      29
#define HEVC_NALU_TYPE_RSV_VCL30      30
#define HEVC_NALU_TYPE_RSV_VCL31      31

//Non-VCL NALs
#define HEVC_NALU_TYPE_VIDEO_PARAM    32
#define HEVC_NALU_TYPE_SEQ_PARAM      33
#define HEVC_NALU_TYPE_PIC_PARAM      34
#define HEVC_NALU_TYPE_ACCESS_UNIT    35
#define HEVC_NALU_TYPE_END_OF_SEQ     36
#define HEVC_NALU_TYPE_END_OF_STREAM  37
#define HEVC_NALU_TYPE_FILLER_DATA    38
#define HEVC_NALU_TYPE_PREFIX_SEI     39
#define HEVC_NALU_TYPE_SUFFIX_SEI     40
#define HEVC_NALU_TYPE_RSV_NVCL41     41
#define HEVC_NALU_TYPE_RSV_NVCL42     42
#define HEVC_NALU_TYPE_RSV_NVCL43     43
#define HEVC_NALU_TYPE_RSV_NVCL44     44
#define HEVC_NALU_TYPE_RSV_NVCL45     45
#define HEVC_NALU_TYPE_RSV_NVCL46     46
#define HEVC_NALU_TYPE_RSV_NVCL47     47
#define HEVC_NALU_TYPE_UNSPEC48       48
#define HEVC_NALU_TYPE_UNSPEC49       49
#define HEVC_NALU_TYPE_UNSPEC50       50
#define HEVC_NALU_TYPE_UNSPEC51       51
#define HEVC_NALU_TYPE_UNSPEC52       52
#define HEVC_NALU_TYPE_UNSPEC53       53
#define HEVC_NALU_TYPE_UNSPEC54       54
#define HEVC_NALU_TYPE_UNSPEC55       55
#define HEVC_NALU_TYPE_UNSPEC56       56
#define HEVC_NALU_TYPE_UNSPEC57       57
#define HEVC_NALU_TYPE_UNSPEC58       58
#define HEVC_NALU_TYPE_UNSPEC59       59
#define HEVC_NALU_TYPE_UNSPEC60       60
#define HEVC_NALU_TYPE_UNSPEC61       61
#define HEVC_NALU_TYPE_UNSPEC62       62
#define HEVC_NALU_TYPE_UNSPEC63       63

#define HEVC_SLICE_TYPE_P   0
#define HEVC_SLICE_TYPE_B   1
#define HEVC_SLICE_TYPE_I   2

#define HEVC_EXTENDED_SAR        0xff
#define HEVC_NUM_PREDEFINED_PARS   17

namespace hevc{

struct vps_info_t {
  unsigned int id;

  unsigned int profile_idc;
  unsigned int level_idc;

  uint32_t checksum;

  vps_info_t() {
    memset(this, 0, sizeof(*this));
  }
};

struct short_term_ref_pic_set_t {
  int inter_ref_pic_set_prediction_flag;
  int delta_idx;
  int delta_rps_sign;
  int abs_delta_rps;

  int delta_poc[16+1];
  int used[16+1];
  int ref_id[16+1];

  int num_ref_id;
  int num_pics;
  int num_negative_pics;
  int num_positive_pics;
};

struct sps_info_t {
  unsigned int id;
  unsigned int vps_id;

  short_term_ref_pic_set_t short_term_ref_pic_sets[64];

  unsigned int chroma_format_idc;
  unsigned int separate_colour_plane_flag;
  unsigned int log2_min_luma_coding_block_size_minus3;
  unsigned int log2_diff_max_min_luma_coding_block_size;
  unsigned int log2_max_pic_order_cnt_lsb;

  // vui:
  bool vui_present, ar_found;
  unsigned int par_num, par_den;

  // timing_info:
  bool timing_info_present;
  unsigned int num_units_in_tick, time_scale;

  unsigned int width, height;

  unsigned int vps;

  uint32_t checksum;

  sps_info_t() {
    memset(this, 0, sizeof(*this));
  }

  void dump();

  bool timing_info_valid() const;
  int64_t default_duration() const;
};

struct pps_info_t {
  unsigned id;
  unsigned sps_id;

  bool dependent_slice_segments_enabled_flag;
  bool output_flag_present_flag;
  unsigned int num_extra_slice_header_bits;

  uint32_t checksum;

  pps_info_t() {
    memset(this, 0, sizeof(*this));
  }

  void dump();
};

struct slice_info_t {
  unsigned char nalu_type;
  unsigned char type;
  unsigned char pps_id;
  bool first_slice_segment_in_pic_flag;
  unsigned int pic_order_cnt_lsb;

  unsigned int sps;
  unsigned int pps;

  slice_info_t() {
    clear();
  }

  void dump() const;
  void clear() {
    memset(this, 0, sizeof(*this));
  }
};

struct par_extraction_t {
  memory_cptr new_hevcc;
  unsigned int numerator, denominator;
  bool successful;

  bool is_valid() const;
};

void nalu_to_rbsp(memory_cptr &buffer);
void rbsp_to_nalu(memory_cptr &buffer);

bool parse_vps(memory_cptr &buffer, vps_info_t &vps);
bool parse_sps(memory_cptr &buffer, sps_info_t &sps, std::vector<vps_info_t> &m_vps_info_list, bool keep_ar_info = false);
bool parse_pps(memory_cptr &buffer, pps_info_t &pps);

par_extraction_t extract_par(memory_cptr const &buffer);
bool is_hevc_fourcc(const char *fourcc);
memory_cptr hevcc_to_nalus(const unsigned char *buffer, size_t size);

struct hevc_frame_t {
  memory_cptr m_data;
  int64_t m_start, m_end, m_ref1, m_ref2;
  bool m_keyframe, m_has_provided_timecode;
  slice_info_t m_si;
  int m_presentation_order, m_decode_order;

  hevc_frame_t() {
    clear();
  }

  hevc_frame_t(const hevc_frame_t &f) {
    *this = f;
  }

  void clear() {
    m_start                 = 0;
    m_end                   = 0;
    m_ref1                  = 0;
    m_ref2                  = 0;
    m_keyframe              = false;
    m_has_provided_timecode = false;
    m_presentation_order    = 0;
    m_decode_order          = 0;
    m_data.reset();

    m_si.clear();
  }
};

class nalu_size_length_x: public mtx::exception {
protected:
  unsigned int m_required_length;

public:
  nalu_size_length_x(unsigned int required_length)
    : m_required_length(required_length)
  {
  }
  virtual ~nalu_size_length_x() throw() { }

  virtual const char *what() const throw() {
    return "'NALU size' length too small";
  }

  virtual std::string error() const throw() {
    return (boost::format("length of the 'NALU size' field too small: need at least %1% bytes") % m_required_length).str();
  }

  virtual unsigned int get_required_length() const {
    return m_required_length;
  }
};

class hevcc_c {
public:
  unsigned int m_profile_idc, m_level_idc, m_nalu_size_length;
  std::vector<memory_cptr> m_vps_list, m_sps_list, m_pps_list;
  std::vector<vps_info_t> m_vps_info_list;
  std::vector<sps_info_t> m_sps_info_list;
  std::vector<pps_info_t> m_pps_info_list;
  memory_cptr m_trailer;

public:
  hevcc_c();
  hevcc_c(unsigned int nalu_size_len, std::vector<memory_cptr> const &vps_list, std::vector<memory_cptr> const &sps_list, std::vector<memory_cptr> const &pps_list);

  explicit operator bool() const;

  memory_cptr pack();
  bool parse_vps_list(bool ignore_errors = false);
  bool parse_sps_list(bool ignore_errors = false);
  bool parse_pps_list(bool ignore_errors = false);

  static hevcc_c unpack(memory_cptr const &mem);
};

class hevc_es_parser_c {
protected:
  int m_nalu_size_length;

  bool m_keep_ar_info;
  bool m_hevcc_ready, m_hevcc_changed;

  int64_t m_stream_default_duration, m_forced_default_duration, m_container_default_duration;
  int m_frame_number, m_num_skipped_frames;
  bool m_first_keyframe_found, m_recovery_point_valid, m_b_frames_since_keyframe;

  bool m_par_found;
  int64_rational_c m_par;

  std::deque<hevc_frame_t> m_frames, m_frames_out;
  std::deque<int64_t> m_provided_timecodes;
  std::deque<uint64_t> m_provided_stream_positions;
  int64_t m_max_timecode;
  std::map<int64_t, int64_t> m_duration_frequency;

  std::vector<memory_cptr> m_vps_list, m_sps_list, m_pps_list, m_extra_data;
  std::vector<vps_info_t> m_vps_info_list;
  std::vector<sps_info_t> m_sps_info_list;
  std::vector<pps_info_t> m_pps_info_list;

  memory_cptr m_unparsed_buffer;
  uint64_t m_stream_position, m_parsed_position;

  hevc_frame_t m_incomplete_frame;
  bool m_have_incomplete_frame;
  std::deque<memory_cptr> m_unhandled_nalus;

  bool m_ignore_nalu_size_length_errors, m_discard_actual_frames;

  bool m_debug_keyframe_detection, m_debug_nalu_types, m_debug_timecode_statistics, m_debug_timecodes, m_debug_sps_info;
  std::map<int, std::string> m_nalu_names_by_type;

  struct stats_t {
    std::vector<int> num_slices_by_type;
    size_t num_frames_out, num_frames_discarded, num_timecodes_in, num_timecodes_generated, num_timecodes_discarded, num_field_slices, num_frame_slices;

    stats_t()
      : num_slices_by_type(3, 0)
      , num_frames_out(0)
      , num_frames_discarded(0)
      , num_timecodes_in(0)
      , num_timecodes_generated(0)
      , num_timecodes_discarded(0)
      , num_field_slices(0)
      , num_frame_slices(0)
    {
    }
  } m_stats;

public:
  hevc_es_parser_c();
  ~hevc_es_parser_c();

  void force_default_duration(int64_t default_duration) {
    m_forced_default_duration = default_duration;
  }

  void set_container_default_duration(int64_t default_duration) {
    m_container_default_duration = default_duration;
  }

  void set_keep_ar_info(bool keep) {
    m_keep_ar_info = keep;
  }

  void add_bytes(unsigned char *buf, size_t size);
  void add_bytes(memory_cptr &buf) {
    add_bytes(buf->get_buffer(), buf->get_size());
  }

  void flush();

  bool frame_available() {
    return !m_frames_out.empty();
  }

  hevc_frame_t get_frame() {
    assert(!m_frames_out.empty());

    hevc_frame_t frame(*m_frames_out.begin());
    m_frames_out.erase(m_frames_out.begin(), m_frames_out.begin() + 1);

    return frame;
  }

  memory_cptr get_hevcc() const;

  bool hevcc_changed() const {
    return m_hevcc_changed;
  }

  int get_width() const {
    assert(!m_sps_info_list.empty());
    return m_sps_info_list.begin()->width;
  }

  int get_height() const {
    assert(!m_sps_info_list.empty());
    return m_sps_info_list.begin()->height;
  }

  void handle_nalu(memory_cptr nalu);

  void add_timecode(int64_t timecode);

  bool headers_parsed() const {
    return m_hevcc_ready;
  }

  void set_nalu_size_length(int nalu_size_length) {
    m_nalu_size_length = nalu_size_length;
  }

  int get_nalu_size_length() const {
    return m_nalu_size_length;
  }

  void ignore_nalu_size_length_errors() {
    m_ignore_nalu_size_length_errors = true;
  }

  void discard_actual_frames(bool discard = true);

  int get_num_skipped_frames() const {
    return m_num_skipped_frames;
  }

  void dump_info() const;

  std::string get_nalu_type_name(int type) const;

  bool has_stream_default_duration() const {
    return -1 != m_stream_default_duration;
  }
  int64_t get_stream_default_duration() const {
    assert(-1 != m_stream_default_duration);
    return m_stream_default_duration;
  }

  int64_t duration_for(slice_info_t const &si) const;
  int64_t get_most_often_used_duration() const;

  size_t get_num_field_slices() const;
  size_t get_num_frame_slices() const;

  bool has_par_been_found() const;
  int64_rational_c const &get_par() const;
  std::pair<int64_t, int64_t> const get_display_dimensions(int width = -1, int height = -1) const;

protected:
  bool parse_slice(memory_cptr &buffer, slice_info_t &si);
  void handle_vps_nalu(memory_cptr &nalu);
  void handle_sps_nalu(memory_cptr &nalu);
  void handle_pps_nalu(memory_cptr &nalu);
  void handle_sei_nalu(memory_cptr &nalu);
  void handle_slice_nalu(memory_cptr &nalu);
  void cleanup();
  void flush_incomplete_frame();
  void flush_unhandled_nalus();
  void write_nalu_size(unsigned char *buffer, size_t size, int this_nalu_size_length = -1) const;
  memory_cptr create_nalu_with_size(const memory_cptr &src, bool add_extra_data = false);
  void init_nalu_names();
};
typedef std::shared_ptr<hevc_es_parser_c> hevc_es_parser_cptr;

};

#endif  // MTX_COMMON_HEVC_H
