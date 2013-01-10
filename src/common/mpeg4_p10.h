/** MPEG video helper functions (MPEG 1, 2 and 4)

   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_COMMON_MPEG4_P10_H
#define MTX_COMMON_MPEG4_P10_H

#include "common/common_pch.h"

#include "common/math.h"

#define NALU_START_CODE 0x00000001

#define NALU_TYPE_NON_IDR_SLICE  0x01
#define NALU_TYPE_DP_A_SLICE     0x02
#define NALU_TYPE_DP_B_SLICE     0x03
#define NALU_TYPE_DP_C_SLICE     0x04
#define NALU_TYPE_IDR_SLICE      0x05
#define NALU_TYPE_SEI            0x06
#define NALU_TYPE_SEQ_PARAM      0x07
#define NALU_TYPE_PIC_PARAM      0x08
#define NALU_TYPE_ACCESS_UNIT    0x09
#define NALU_TYPE_END_OF_SEQ     0x0a
#define NALU_TYPE_END_OF_STREAM  0x0b
#define NALU_TYPE_FILLER_DATA    0x0c

#define AVC_SLICE_TYPE_P   0
#define AVC_SLICE_TYPE_B   1
#define AVC_SLICE_TYPE_I   2
#define AVC_SLICE_TYPE_SP  3
#define AVC_SLICE_TYPE_SI  4
#define AVC_SLICE_TYPE2_P  5
#define AVC_SLICE_TYPE2_B  6
#define AVC_SLICE_TYPE2_I  7
#define AVC_SLICE_TYPE2_SP 8
#define AVC_SLICE_TYPE2_SI 9

#define AVC_EXTENDED_SAR        0xff
#define AVC_NUM_PREDEFINED_PARS   17

namespace mpeg4 {
namespace p10 {

struct sps_info_t {
  unsigned int id;

  unsigned int profile_idc;
  unsigned int profile_compat;
  unsigned int level_idc;
  unsigned int chroma_format_idc;
  unsigned int log2_max_frame_num;
  unsigned int pic_order_cnt_type;
  unsigned int log2_max_pic_order_cnt_lsb;
  unsigned int offset_for_non_ref_pic;
  unsigned int offset_for_top_to_bottom_field;
  unsigned int num_ref_frames_in_pic_order_cnt_cycle;
  bool delta_pic_order_always_zero_flag;
  bool frame_mbs_only;

  // vui:
  bool vui_present, ar_found;
  unsigned int par_num, par_den;

  // timing_info:
  bool timing_info_present;
  unsigned int num_units_in_tick, time_scale;
  bool fixed_frame_rate;

  unsigned int crop_left, crop_top, crop_right, crop_bottom;
  unsigned int width, height;

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

  bool pic_order_present;

  uint32_t checksum;

  pps_info_t() {
    memset(this, 0, sizeof(*this));
  }

  void dump();
};

struct slice_info_t {
  unsigned char nalu_type;
  unsigned char nal_ref_idc;
  unsigned char type;
  unsigned char pps_id;
  unsigned int frame_num;
  bool field_pic_flag, bottom_field_flag;
  unsigned int idr_pic_id;
  unsigned int pic_order_cnt_lsb;
  unsigned int delta_pic_order_cnt_bottom;
  unsigned int delta_pic_order_cnt[2];
  unsigned int first_mb_in_slice;

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

void nalu_to_rbsp(memory_cptr &buffer);
void rbsp_to_nalu(memory_cptr &buffer);

bool parse_sps(memory_cptr &buffer, sps_info_t &sps, bool keep_ar_info = false);
bool parse_pps(memory_cptr &buffer, pps_info_t &pps);

bool extract_par(uint8_t *&buffer, size_t &buffer_size, uint32_t &par_num, uint32_t &par_den);
bool is_avc_fourcc(const char *fourcc);
memory_cptr avcc_to_nalus(const unsigned char *buffer, size_t size);

struct avc_frame_t {
  memory_cptr m_data;
  int64_t m_start, m_end, m_ref1, m_ref2;
  bool m_keyframe, m_has_provided_timecode;
  slice_info_t m_si;
  int m_presentation_order, m_decode_order;

  avc_frame_t() {
    clear();
  }

  avc_frame_t(const avc_frame_t &f) {
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

class avcc_c {
public:
  unsigned int m_profile_idc, m_profile_compat, m_level_idc, m_nalu_size_length;
  std::vector<memory_cptr> m_sps_list, m_pps_list;
  std::vector<sps_info_t> m_sps_info_list;
  std::vector<pps_info_t> m_pps_info_list;

public:
  avcc_c();
  avcc_c(unsigned int nalu_size_len, std::vector<memory_cptr> const &sps_list, std::vector<memory_cptr> const &pps_list);

  explicit operator bool() const;

  memory_cptr pack();
  bool parse_sps_list(bool ignore_errors = false);
  bool parse_pps_list(bool ignore_errors = false);

  static avcc_c unpack(memory_cptr const &mem);
};

class avc_es_parser_c {
protected:
  int m_nalu_size_length;

  bool m_keep_ar_info;
  bool m_avcc_ready, m_avcc_changed;

  int64_t m_stream_default_duration, m_forced_default_duration, m_container_default_duration;
  int m_frame_number, m_num_skipped_frames;
  bool m_first_keyframe_found, m_recovery_point_valid, m_b_frames_since_keyframe;

  bool m_par_found;
  int64_rational_c m_par;

  std::deque<avc_frame_t> m_frames, m_frames_out;
  std::deque<int64_t> m_provided_timecodes;
  std::deque<uint64_t> m_provided_stream_positions;
  int64_t m_max_timecode;
  std::map<int64_t, int64_t> m_duration_frequency;

  std::vector<memory_cptr> m_sps_list, m_pps_list, m_extra_data;
  std::vector<sps_info_t> m_sps_info_list;
  std::vector<pps_info_t> m_pps_info_list;

  memory_cptr m_unparsed_buffer;
  uint64_t m_stream_position, m_parsed_position;

  avc_frame_t m_incomplete_frame;
  bool m_have_incomplete_frame;
  std::deque<memory_cptr> m_unhandled_nalus;

  bool m_ignore_nalu_size_length_errors, m_discard_actual_frames;

  bool m_debug_keyframe_detection, m_debug_nalu_types, m_debug_timecode_statistics, m_debug_timecodes, m_debug_sps_info;
  std::map<int, std::string> m_nalu_names_by_type;

  struct stats_t {
    std::vector<int> num_slices_by_type;
    size_t num_frames_out, num_frames_discarded, num_timecodes_in, num_timecodes_generated, num_timecodes_discarded, num_field_slices, num_frame_slices;

    stats_t()
      : num_slices_by_type(11, 0)
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
  avc_es_parser_c();
  ~avc_es_parser_c();

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

  avc_frame_t get_frame() {
    assert(!m_frames_out.empty());

    avc_frame_t frame(*m_frames_out.begin());
    m_frames_out.erase(m_frames_out.begin(), m_frames_out.begin() + 1);

    return frame;
  }

  memory_cptr get_avcc() const;

  bool avcc_changed() const {
    return m_avcc_changed;
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
    return m_avcc_ready;
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
  void handle_sps_nalu(memory_cptr &nalu);
  void handle_pps_nalu(memory_cptr &nalu);
  void handle_sei_nalu(memory_cptr &nalu);
  void handle_slice_nalu(memory_cptr &nalu);
  void cleanup();
  bool flush_decision(slice_info_t &si, slice_info_t &ref);
  void flush_incomplete_frame();
  void flush_unhandled_nalus();
  void write_nalu_size(unsigned char *buffer, size_t size, int this_nalu_size_length = -1) const;
  memory_cptr create_nalu_with_size(const memory_cptr &src, bool add_extra_data = false);
  void init_nalu_names();
};
typedef std::shared_ptr<avc_es_parser_c> avc_es_parser_cptr;

};
};

#endif  // MTX_COMMON_MPEG4_P10_H
