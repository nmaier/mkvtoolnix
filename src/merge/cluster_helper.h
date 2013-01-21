/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the cluster helper

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_CLUSTER_HELPER_C
#define MTX_CLUSTER_HELPER_C

#include "common/common_pch.h"

#include <matroska/KaxBlock.h>
#include <matroska/KaxCluster.h>

#include "common/split_point.h"
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

typedef std::pair<int64_t, int64_t> id_timecode_t;

class cluster_helper_c {
private:
  kax_cluster_c *m_cluster;
  std::vector<packet_cptr> m_packets;
  int m_cluster_content_size;
  int64_t m_max_timecode_and_duration, m_max_video_timecode_rendered;
  int64_t m_previous_cluster_tc, m_num_cue_elements, m_header_overhead;
  int64_t m_timecode_offset;
  int64_t m_bytes_in_file, m_first_timecode_in_file, m_first_discarded_timecode, m_last_discarded_timecode_and_duration, m_discarded_duration;
  int64_t m_min_timecode_in_cluster, m_max_timecode_in_cluster, m_frame_field_number;
  bool m_first_video_keyframe_seen;
  mm_io_c *m_out;

  std::vector<split_point_c> m_split_points;
  std::vector<split_point_c>::iterator m_current_split_point;

  std::map<id_timecode_t, int64_t> m_id_timecode_duration_map;
  size_t m_num_cue_points_postprocessed;

  bool m_discarding, m_splitting_and_processed_fully;

  bool m_no_cue_duration, m_no_cue_relative_position;

  bool m_debug_splitting, m_debug_packets, m_debug_duration, m_debug_rendering, m_debug_cue_duration, m_debug_cue_relative_position;

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
  int64_t get_discarded_duration() const;
  void handle_discarded_duration(bool create_new_file, bool previously_discarding);

  void add_split_point(split_point_c const &split_point);
  void dump_split_points() const;
  bool splitting() const {
    return !m_split_points.empty();
  }
  bool split_mode_produces_many_files() const;

  bool discarding() const {
    return splitting() && m_discarding;
  }

  int get_packet_count() const {
    return m_packets.size();
  }

  void discard_queued_packets();
  bool is_splitting_and_processed_fully() const {
    return m_splitting_and_processed_fully;
  }

private:
  void set_duration(render_groups_c *rg);
  bool must_duration_be_set(render_groups_c *rg, packet_cptr &new_packet);

  void render_before_adding_if_necessary(packet_cptr &packet);
  void render_after_adding_if_necessary(packet_cptr &packet);
  void split_if_necessary(packet_cptr &packet);
  void split(packet_cptr &packet);

  bool add_to_cues_maybe(packet_cptr &pack, kax_block_blob_c &block_group);
  void postprocess_cues();
  std::map<id_timecode_t, int64_t> calculate_block_positions() const;
};

extern cluster_helper_c *g_cluster_helper;

#endif // MTX_CLUSTER_HELPER_C
