/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the cluster helper

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef __CLUSTER_HELPER_C
#define __CLUSTER_HELPER_C

#include "common/common_pch.h"

#include <matroska/KaxBlock.h>
#include <matroska/KaxCluster.h>

#include "merge/libmatroska_extensions.h"
#include "merge/pr_generic.h"


#define RND_TIMECODE_SCALE(a) (irnd((double)(a) / (double)((int64_t)g_timecode_scale)) * (int64_t)g_timecode_scale)

class render_groups_c {
public:
  std::vector<kax_block_blob_cptr> m_groups;
  std::vector<int64_t> m_durations;
  generic_packetizer_c *m_source;
  bool m_more_data, m_duration_mandatory;

  render_groups_c(generic_packetizer_c *source)
    : m_source(source)
    , m_more_data(false)
    , m_duration_mandatory(false)
  {
  }
};
typedef std::shared_ptr<render_groups_c> render_groups_cptr;

struct split_point_t {
  enum split_point_type_e {
    SPT_DURATION,
    SPT_SIZE,
    SPT_TIMECODE,
    SPT_CHAPTER,
    SPT_PARTS,
  };

  int64_t m_point;
  split_point_type_e m_type;
  bool m_use_once, m_discard, m_create_new_file;

  split_point_t(int64_t point,
                split_point_type_e type,
                bool use_once,
                bool discard = false,
                bool create_new_file = true)
    : m_point(point)
    , m_type(type)
    , m_use_once(use_once)
    , m_discard(discard)
    , m_create_new_file(create_new_file)
  {
  }

  bool
  operator <(split_point_t const &rhs)
    const {
    return m_point < rhs.m_point;
  }

  std::string str() const;
};

class cluster_helper_c {
private:
  kax_cluster_c *m_cluster;
  std::vector<packet_cptr> m_packets;
  int m_cluster_content_size;
  int64_t m_max_timecode_and_duration, m_max_video_timecode_rendered;
  int64_t m_previous_cluster_tc, m_num_cue_elements, m_header_overhead;
  int64_t m_packet_num, m_timecode_offset, *m_previous_packets;
  int64_t m_bytes_in_file, m_first_timecode_in_file, m_first_discarded_timecode, m_last_discarded_timecode;
  int64_t m_min_timecode_in_cluster, m_max_timecode_in_cluster;
  int64_t m_attachments_size;
  mm_io_c *m_out;

  std::vector<split_point_t> m_split_points;
  std::vector<split_point_t>::iterator m_current_split_point;

  bool m_discarding, m_debug_splitting, m_debug_packets, m_debug_duration;

public:
  cluster_helper_c();
  virtual ~cluster_helper_c();

  void set_output(mm_io_c *out);
  mm_io_c *get_output() {
    return m_out;
  }
  void prepare_new_cluster();
  KaxCluster *get_cluster() {
    return m_cluster;
  }
  void add_packet(packet_cptr packet);
  int64_t get_timecode();
  int render();
  int get_cluster_content_size();
  int64_t get_duration();
  int64_t get_first_timecode_in_file() {
    return m_first_timecode_in_file;
  }

  void add_split_point(const split_point_t &split_point);
  void dump_split_points() const;
  bool splitting() const {
    return !m_split_points.empty();
  }

  bool discarding() const {
    return splitting() && m_discarding;
  }

  int get_packet_count() const {
    return m_packets.size();
  }

private:
  void set_duration(render_groups_c *rg);
  bool must_duration_be_set(render_groups_c *rg, packet_cptr &new_packet);

  void render_before_adding_if_necessary(packet_cptr &packet);
  void render_after_adding_if_necessary(packet_cptr &packet);
  void split_if_necessary(packet_cptr &packet);
  void split(packet_cptr &packet);
};

extern cluster_helper_c *g_cluster_helper;

#endif // __CLUSTER_HELPER_C
