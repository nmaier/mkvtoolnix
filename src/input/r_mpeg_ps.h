/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definitions for the MPEG PS demultiplexer module

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_INPUT_R_MPEG_PS_H
#define MTX_INPUT_R_MPEG_PS_H

#include "common/common_pch.h"

#include "common/bit_cursor.h"
#include "common/dts.h"
#include "common/mm_multi_file_io.h"
#include "common/mpeg1_2.h"
#include "merge/packet_extensions.h"
#include "merge/pr_generic.h"

struct mpeg_ps_id_t {
  int id;
  int sub_id;

  mpeg_ps_id_t(int n_id = 0, int n_sub_id = 0)
    : id(n_id)
    , sub_id(n_sub_id) {
  }

  inline unsigned int idx() const {
    return ((id & 0xff) << 8) | (sub_id & 0xff);
  }
};

inline std::ostream &
operator <<(std::ostream &out,
            mpeg_ps_id_t const &id) {
  out << (boost::format("%|1$02x|.%|2$02x|") % id.id % id.sub_id);

  return out;
}

class mpeg_ps_packet_c {
public:
  mpeg_ps_id_t m_id;
  int64_t m_pts, m_dts;
  unsigned int m_length, m_full_length;
  bool m_valid;
  memory_cptr m_buffer;

public:
  mpeg_ps_packet_c(mpeg_ps_id_t id = mpeg_ps_id_t{})
    : m_id{id}
    , m_pts{-1}
    , m_dts{-1}
    , m_length{0}
    , m_full_length{0}
    , m_valid{false}
  {
  }

  bool has_pts() const {
    return -1 != m_pts;
  }

  bool has_dts() const {
    return -1 != m_dts;
  }

  int64_t pts() const {
    return has_pts() ? m_pts : std::numeric_limits<int64_t>::max();
  }

  int64_t dts() const {
    return has_dts() ? m_dts : pts();
  }

  operator bool() const {
    return m_valid;
  }

  bool has_been_read() const {
    return m_valid && m_buffer;
  }
};

inline std::ostream &
operator <<(std::ostream &out,
            mpeg_ps_packet_c const &p) {
  out << (boost::format("[ID %1% PTS %2% DTS %3% length %4% full_length %5% valid? %6% read? %7%]")
          % p.m_id
          % (p.has_pts() ? format_timecode(p.pts()) : std::string{"none"})
          % (p.has_dts() ? format_timecode(p.dts()) : std::string{"none"})
          % p.m_length % p.m_full_length % p.m_valid
          % (p.has_been_read() ? "yes, fully" : p.m_buffer ? "only partially" : "no"));

  return out;
}

struct mpeg_ps_track_t {
  int ptzr;

  char type;                    // 'v' for video, 'a' for audio, 's' for subs
  mpeg_ps_id_t id;
  int sort_key;
  uint32_t fourcc;

  bool provide_timecodes;
  int64_t timecode_offset, timecode_b_frame_offset;

  bool v_interlaced;
  int v_version, v_width, v_height, v_dwidth, v_dheight;
  double v_frame_rate, v_aspect_ratio;
  memory_cptr v_avcc;
  unsigned char *raw_seq_hdr;
  int raw_seq_hdr_size;

  int a_channels, a_sample_rate, a_bits_per_sample, a_bsid;
  dts_header_t dts_header;

  unsigned char *buffer;
  unsigned int buffer_usage, buffer_size;

  multiple_timecodes_packet_extension_c *multiple_timecodes_packet_extension;

  mpeg_ps_track_t():
    ptzr(-1),
    type(0),
    sort_key(0),
    fourcc(0),
    provide_timecodes(false),
    timecode_offset(std::numeric_limits<int64_t>::max()),
    timecode_b_frame_offset{0},
    v_interlaced(false),
    v_version(0),
    v_width(0),
    v_height(0),
    v_dwidth(0),
    v_dheight(0),
    v_frame_rate(0),
    v_aspect_ratio(0),
    raw_seq_hdr(nullptr),
    raw_seq_hdr_size(0),
    a_channels(0),
    a_sample_rate(0),
    a_bits_per_sample(0),
    a_bsid(0),
    buffer(nullptr),
    buffer_usage(0),
    buffer_size(0),
    multiple_timecodes_packet_extension(new multiple_timecodes_packet_extension_c)
  {
  };

  void use_buffer(size_t size) {
    safefree(buffer);
    buffer       = (unsigned char *)safemalloc(size);
    buffer_size  = size;
    buffer_usage = 0;
  }

  void assert_buffer_size(size_t size) {
    if (!buffer)
      use_buffer(size);
    else if (size > buffer_size) {
      buffer      = (unsigned char *)saferealloc(buffer, size);
      buffer_size = size;
    }
  }

  ~mpeg_ps_track_t() {
    safefree(raw_seq_hdr);
    safefree(buffer);
    delete multiple_timecodes_packet_extension;
  }
};
typedef std::shared_ptr<mpeg_ps_track_t> mpeg_ps_track_ptr;

inline bool
operator <(mpeg_ps_track_ptr const &a,
           mpeg_ps_track_ptr const &b) {
  return a->sort_key < b->sort_key;
}

class mpeg_ps_reader_c: public generic_reader_c {
private:
  int64_t duration, global_timecode_offset;

  std::map<int, int> id2idx;
  std::map<int, bool> blacklisted_ids;

  std::map<int, int> es_map;
  int version;
  bool file_done;

  std::vector<mpeg_ps_track_ptr> tracks;
  std::map<generic_packetizer_c *, mpeg_ps_track_ptr> m_ptzr_to_track_map;

  bool m_debug_timecodes;

public:
  mpeg_ps_reader_c(const track_info_c &ti, const mm_io_cptr &in);
  virtual ~mpeg_ps_reader_c();

  virtual const std::string get_format_name(bool translate = true) {
    return translate ? Y("MPEG program stream") : "MPEG program stream";
  }

  virtual void read_headers();
  virtual file_status_e read(generic_packetizer_c *requested_ptzr, bool force = false);
  virtual void identify();
  virtual void create_packetizer(int64_t id);
  virtual void create_packetizers();
  virtual void add_available_track_ids();

  virtual void found_new_stream(mpeg_ps_id_t id);

  virtual bool read_timestamp(bit_cursor_c &bc, int64_t &timestamp);
  virtual bool read_timestamp(int c, int64_t &timestamp);
  virtual mpeg_ps_packet_c parse_packet(mpeg_ps_id_t id, bool read_data = true);
  virtual bool find_next_packet(mpeg_ps_id_t &id, int64_t max_file_pos = -1);
  virtual bool find_next_packet_for_id(mpeg_ps_id_t id, int64_t max_file_pos = -1);

  virtual void parse_program_stream_map();

  static int probe_file(mm_io_c *in, uint64_t size);

private:
  virtual void new_stream_v_avc_or_mpeg_1_2(mpeg_ps_id_t id, unsigned char *buf, unsigned int length, mpeg_ps_track_ptr &track);
  virtual void new_stream_v_mpeg_1_2(mpeg_ps_id_t id, unsigned char *buf, unsigned int length, mpeg_ps_track_ptr &track);
  virtual void new_stream_v_avc(mpeg_ps_id_t id, unsigned char *buf, unsigned int length, mpeg_ps_track_ptr &track);
  virtual void new_stream_v_vc1(mpeg_ps_id_t id, unsigned char *buf, unsigned int length, mpeg_ps_track_ptr &track);
  virtual void new_stream_a_mpeg(mpeg_ps_id_t id, unsigned char *buf, unsigned int length, mpeg_ps_track_ptr &track);
  virtual void new_stream_a_ac3(mpeg_ps_id_t id, unsigned char *buf, unsigned int length, mpeg_ps_track_ptr &track);
  virtual void new_stream_a_dts(mpeg_ps_id_t id, unsigned char *buf, unsigned int length, mpeg_ps_track_ptr &track);
  virtual void new_stream_a_truehd(mpeg_ps_id_t id, unsigned char *buf, unsigned int length, mpeg_ps_track_ptr &track);
  virtual bool resync_stream(uint32_t &header);
  virtual file_status_e finish();
  void sort_tracks();
  void calculate_global_timecode_offset();
};

#endif // MTX_INPUT_R_MPEG_PS_H
