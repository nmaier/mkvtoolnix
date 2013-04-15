/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   cues storage & helper class

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/debugging.h"
#include "common/ebml.h"
#include "common/hacks.h"
#include "common/math.h"
#include "merge/cluster_helper.h"
#include "merge/cues.h"
#include "merge/libmatroska_extensions.h"
#include "merge/output_control.h"

cues_cptr cues_c::s_cues;

cues_c::cues_c()
  : m_num_cue_points_postprocessed{}
  , m_no_cue_duration{hack_engaged(ENGAGE_NO_CUE_DURATION)}
  , m_no_cue_relative_position{hack_engaged(ENGAGE_NO_CUE_RELATIVE_POSITION)}
  , m_debug_cue_duration{debugging_requested("cues|cues_cue_duration")}
  , m_debug_cue_relative_position{debugging_requested("cues|cues_cue_relative_position")}
{
}

void
cues_c::set_duration_for_id_timecode(uint64_t id,
                                     uint64_t timecode,
                                     uint64_t duration) {
  if (!m_no_cue_duration)
    m_id_timecode_duration_map[ std::make_pair(id, timecode) ] = duration;
}

void
cues_c::add(KaxCues &cues) {
  for (auto child : cues) {
    auto point = dynamic_cast<KaxCuePoint *>(child);
    if (point)
      add(*point);
  }
}

void
cues_c::add(KaxCuePoint &point) {
  uint64_t timecode = FindChildValue<KaxCueTime>(point) * g_timecode_scale;

  for (auto point_child : point) {
    auto positions = dynamic_cast<KaxCueTrackPositions *>(point_child);
    if (!positions)
      continue;

    uint64_t track_num = FindChildValue<KaxCueTrack>(*positions);
    assert(track_num <= static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()));

    m_points.push_back({ timecode, 0, FindChildValue<KaxCueClusterPosition>(*positions), static_cast<uint32_t>(track_num), 0 });

    uint64_t codec_state_position = FindChildValue<KaxCueCodecState>(*positions);
    if (codec_state_position)
      m_codec_state_position_map[ { track_num, timecode } ] = codec_state_position;
  }
}

void
cues_c::write(mm_io_c &out,
              KaxSeekHead &seek_head) {
  if (!m_points.size() || !g_cue_writing_requested)
    return;

  // sort();

  // Need to write the (empty) cues element so that its position will
  // be set for indexing in g_kax_sh_main. Necessary because there's
  // no API function to force the position to a certain value; nor is
  // there a different API function in KaxSeekHead for adding anything
  // by ID and position manually.
  out.save_pos();
  kax_cues_position_dummy_c cues_dummy;
  cues_dummy.Render(out);
  out.restore_pos();

  // Write meta seek information if it is not disabled.
  seek_head.IndexThis(cues_dummy, *g_kax_segment);

  // Forcefully write the correct head and copy its content from the
  // temporary storage location.
  auto total_size = calculate_total_size();
  write_ebml_element_head(out, EBML_ID(KaxCues), total_size);

  for (auto &point : m_points) {
    KaxCuePoint kc_point;

    GetChild<KaxCueTime>(kc_point).SetValue(point.timecode / g_timecode_scale);

    auto &positions = GetChild<KaxCueTrackPositions>(kc_point);
    GetChild<KaxCueTrack>(positions).SetValue(point.track_num);
    GetChild<KaxCueClusterPosition>(positions).SetValue(point.cluster_position);

    auto codec_state_position = m_codec_state_position_map.find({ point.track_num, point.timecode });
    if (codec_state_position != m_codec_state_position_map.end())
      GetChild<KaxCueCodecState>(positions).SetValue(codec_state_position->second);

    if (point.relative_position)
      GetChild<KaxCueRelativePosition>(positions).SetValue(point.relative_position);

    if (point.duration)
      GetChild<KaxCueDuration>(positions).SetValue(RND_TIMECODE_SCALE(point.duration) / g_timecode_scale);

    kc_point.Render(out);
  }

  m_points.clear();
  m_codec_state_position_map.clear();
  m_num_cue_points_postprocessed = 0;
}

void
cues_c::sort() {
  brng::sort(m_points, [](cue_point_t const &a, cue_point_t const &b) {
      if (a.timecode < b.timecode)
        return true;
      if (a.timecode > b.timecode)
        return false;

      if (a.track_num < b.track_num)
        return true;
      if (a.track_num > b.track_num)
        return false;

      if (a.cluster_position < b.cluster_position)
        return true;

      return false;
    });
}

std::map<id_timecode_t, uint64_t>
cues_c::calculate_block_positions(KaxCluster &cluster)
  const {

  std::map<id_timecode_t, uint64_t> positions;

  for (auto child : cluster) {
    auto simple_block = dynamic_cast<KaxSimpleBlock *>(child);
    if (simple_block) {
      simple_block->SetParent(cluster);
      positions[ id_timecode_t{ simple_block->TrackNum(), simple_block->GlobalTimecode() } ] = simple_block->GetElementPosition();
      continue;
    }

    auto block_group = dynamic_cast<KaxBlockGroup *>(child);
    if (!block_group)
      continue;

    auto block = FindChild<KaxBlock>(block_group);
    if (!block)
      continue;

    block->SetParent(cluster);
    positions[ id_timecode_t{ block->TrackNum(), block->GlobalTimecode() } ] = block->GetElementPosition();
  }

  return positions;
}

void
cues_c::postprocess_cues(KaxCues &cues,
                         KaxCluster &cluster) {
  add(cues);

  if (m_no_cue_duration && m_no_cue_relative_position)
    return;

  auto cluster_data_start_pos = cluster.GetElementPosition() + cluster.HeadSize();
  auto block_positions        = calculate_block_positions(cluster);

  for (auto point = m_points.begin() + m_num_cue_points_postprocessed, end = m_points.end(); point != end; ++point) {
    // Set CueRelativePosition for all cues.
    if (!m_no_cue_relative_position) {
      auto position_itr      = block_positions.find({ point->track_num, point->timecode });
      auto relative_position = block_positions.end() != position_itr ? std::max(position_itr->second, cluster_data_start_pos) - cluster_data_start_pos : 0ull;

      assert(relative_position <= static_cast<uint64_t>(std::numeric_limits<uint32_t>::max()));

      point->relative_position = relative_position;

      mxdebug_if(m_debug_cue_relative_position,
                 boost::format("cue_relative_position: looking for <%1%:%2%>: cluster_data_start_pos %3% position %4%\n")
                 % point->track_num % point->timecode % cluster_data_start_pos % relative_position);
    }

    // Set CueDuration if the packetizer wants them.
    if (m_no_cue_duration)
      continue;

    auto duration_itr = m_id_timecode_duration_map.find({ point->track_num, point->timecode });
    auto ptzr         = g_packetizers_by_track_num[point->track_num];

    if (!ptzr || !ptzr->wants_cue_duration())
      continue;

    if (m_id_timecode_duration_map.end() != duration_itr)
      point->duration = duration_itr->second;

    mxdebug_if(m_debug_cue_duration,
               boost::format("cue_duration: looking for <%1%:%2%>: %3%\n")
               % point->track_num % point->timecode % (duration_itr == m_id_timecode_duration_map.end() ? static_cast<int64_t>(-1) : duration_itr->second));
  }

  m_num_cue_points_postprocessed = m_points.size();

  m_id_timecode_duration_map.clear();
}

uint64_t
cues_c::calculate_total_size()
  const {
  return boost::accumulate(m_points, 0ull, [this](uint64_t sum, cue_point_t const &point) { return sum + calculate_point_size(point); });
}

uint64_t
cues_c::calculate_bytes_for_uint(uint64_t value)
  const {
  for (int idx = 1; 7 >= idx; ++idx)
    if (value < (1ull << (idx * 8)))
      return idx;
  return 8;
}

uint64_t
cues_c::calculate_point_size(cue_point_t const &point)
  const {
  uint64_t point_size = EBML_ID_LENGTH(EBML_ID(KaxCuePoint))           + 1
                      + EBML_ID_LENGTH(EBML_ID(KaxCuePoint))           + 1 + calculate_bytes_for_uint(point.timecode / g_timecode_scale)
                      + EBML_ID_LENGTH(EBML_ID(KaxCueTrackPositions))  + 1
                      + EBML_ID_LENGTH(EBML_ID(KaxCueTrack))           + 1 + calculate_bytes_for_uint(point.track_num)
                      + EBML_ID_LENGTH(EBML_ID(KaxCueClusterPosition)) + 1 + calculate_bytes_for_uint(point.cluster_position);

  auto codec_state_position = m_codec_state_position_map.find({ point.track_num, point.timecode });
  if (codec_state_position != m_codec_state_position_map.end())
    point_size += EBML_ID_LENGTH(EBML_ID(KaxCueCodecState)) + 1 + calculate_bytes_for_uint(codec_state_position->second);

  if (point.relative_position)
    point_size += EBML_ID_LENGTH(EBML_ID(KaxCueRelativePosition)) + 1 + calculate_bytes_for_uint(point.relative_position);

  if (point.duration)
    point_size += EBML_ID_LENGTH(EBML_ID(KaxCueDuration)) + 1 + calculate_bytes_for_uint(RND_TIMECODE_SCALE(point.duration) / g_timecode_scale);

  return point_size;
}

cues_c &
cues_c::get() {
  if (!s_cues)
    s_cues = std::make_shared<cues_c>();
  return *s_cues;
}
