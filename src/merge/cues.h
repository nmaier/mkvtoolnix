/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   cues storage & helper class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#ifndef MTX_MERGE_CUES_H
#define MTX_MERGE_CUES_H

#include "common/common_pch.h"

#include <matroska/KaxCues.h>
#include <matroska/KaxCuesData.h>
#include <matroska/KaxSeekHead.h>

#include "common/mm_io.h"

typedef std::pair<uint64_t, uint64_t> id_timecode_t;

struct cue_point_t {
  uint64_t timecode, duration, cluster_position;
  uint32_t track_num, relative_position;
};

class cues_c;
typedef std::shared_ptr<cues_c> cues_cptr;

class cues_c {
protected:
  std::vector<cue_point_t> m_points;
  std::map<id_timecode_t, uint64_t> m_id_timecode_duration_map, m_codec_state_position_map;

  size_t m_num_cue_points_postprocessed;
  bool m_no_cue_duration, m_no_cue_relative_position;
  debugging_option_c m_debug_cue_duration, m_debug_cue_relative_position;

protected:
  static cues_cptr s_cues;

public:
  cues_c();

  void add(KaxCues &cues);
  void add(KaxCuePoint &point);
  void write(mm_io_c &out, KaxSeekHead &seek_head);
  void postprocess_cues(KaxCues &cues, KaxCluster &cluster);
  void set_duration_for_id_timecode(uint64_t id, uint64_t timecode, uint64_t duration);

public:
  static cues_c &get();

protected:
  void sort();
  std::map<id_timecode_t, uint64_t> calculate_block_positions(KaxCluster &cluster) const;
  uint64_t calculate_total_size() const;
  uint64_t calculate_point_size(cue_point_t const &point) const;
  uint64_t calculate_bytes_for_uint(uint64_t value) const;
};

#endif  // MTX_MERGE_CUES_H
