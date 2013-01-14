/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   The cluster helper groups frames into blocks groups and those
   into clusters, sets the durations, renders the clusters etc.

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "common/common_pch.h"

#include "common/ebml.h"
#include "common/hacks.h"
#include "common/math.h"
#include "common/strings/formatting.h"
#include "merge/cluster_helper.h"
#include "merge/libmatroska_extensions.h"
#include "merge/output_control.h"
#include "output/p_video.h"

#include <matroska/KaxBlock.h>
#include <matroska/KaxBlockData.h>
#include <matroska/KaxCuesData.h>
#include <matroska/KaxSeekHead.h>

std::string
split_point_t::str()
  const {
  return (boost::format("<%1% %2% once:%3% discard:%4% create_file:%5%>")
          % format_timecode(m_point)
          % (  split_point_t::SPT_DURATION          == m_type ? "duration"
             : split_point_t::SPT_SIZE              == m_type ? "size"
             : split_point_t::SPT_TIMECODE          == m_type ? "timecode"
             : split_point_t::SPT_CHAPTER           == m_type ? "chapter"
             : split_point_t::SPT_PARTS             == m_type ? "part"
             : split_point_t::SPT_PARTS_FRAME_FIELD == m_type ? "part(frame/field)"
             : split_point_t::SPT_FRAME_FIELD       == m_type ? "frame/field"
             :                                                  "unknown")
          % m_use_once % m_discard % m_create_new_file).str();
}

cluster_helper_c::cluster_helper_c()
  : m_cluster(nullptr)
  , m_cluster_content_size(0)
  , m_max_timecode_and_duration(0)
  , m_max_video_timecode_rendered(0)
  , m_previous_cluster_tc{-1}
  , m_num_cue_elements(0)
  , m_header_overhead(-1)
  , m_timecode_offset(0)
  , m_bytes_in_file(0)
  , m_first_timecode_in_file(-1)
  , m_first_discarded_timecode{-1}
  , m_last_discarded_timecode_and_duration{0}
  , m_discarded_duration{0}
  , m_min_timecode_in_cluster(-1)
  , m_max_timecode_in_cluster(-1)
  , m_frame_field_number{}
  , m_first_video_keyframe_seen{}
  , m_out(nullptr)
  , m_current_split_point(m_split_points.begin())
  , m_num_cue_points_postprocessed{}
  , m_discarding{false}
  , m_splitting_and_processed_fully{false}
  , m_no_cue_duration{hack_engaged(ENGAGE_NO_CUE_DURATION)}
  , m_no_cue_relative_position{hack_engaged(ENGAGE_NO_CUE_RELATIVE_POSITION)}
  , m_debug_splitting{debugging_requested("cluster_helper|splitting")}
  , m_debug_packets{  debugging_requested("cluster_helper|cluster_helper_packets")}
  , m_debug_duration{ debugging_requested("cluster_helper|cluster_helper_duration")}
  , m_debug_rendering{debugging_requested("cluster_helper|cluster_helper_rendering")}
  , m_debug_cue_duration{debugging_requested("cluster_helper|cluster_helper_cue_duration")}
  , m_debug_cue_relative_position{debugging_requested("cluster_helper|cluster_helper_cue_relative_position")}
{
}

cluster_helper_c::~cluster_helper_c() {
  delete m_cluster;
}

void
cluster_helper_c::render_before_adding_if_necessary(packet_cptr &packet) {
  int64_t timecode        = get_timecode();
  int64_t timecode_delay  = (   (packet->assigned_timecode > m_max_timecode_in_cluster)
                             || (-1 == m_max_timecode_in_cluster))                       ? packet->assigned_timecode : m_max_timecode_in_cluster;
  timecode_delay         -= (   (-1 == m_min_timecode_in_cluster)
                             || (packet->assigned_timecode < m_min_timecode_in_cluster)) ? packet->assigned_timecode : m_min_timecode_in_cluster;
  timecode_delay          = (int64_t)(timecode_delay / g_timecode_scale);

  mxdebug_if(m_debug_packets,
             boost::format("cluster_helper_c::add_packet(): new packet { source %1%/%2% "
                           "timecode: %3% duration: %4% bref: %5% fref: %6% assigned_timecode: %7% timecode_delay: %8% }\n")
             % packet->source->m_ti.m_id % packet->source->m_ti.m_fname % packet->timecode          % packet->duration
             % packet->bref              % packet->fref                 % packet->assigned_timecode % format_timecode(timecode_delay));

  bool is_video_keyframe = (packet->source == g_video_packetizer) && packet->is_key_frame();
  bool do_render         = (std::numeric_limits<int16_t>::max() < timecode_delay)
                        || (std::numeric_limits<int16_t>::min() > timecode_delay)
                        || (   (std::max<int64_t>(0, m_min_timecode_in_cluster) > m_previous_cluster_tc)
                            && (packet->assigned_timecode                       > m_min_timecode_in_cluster)
                            && (!g_video_packetizer || !is_video_keyframe || m_first_video_keyframe_seen)
                            && (   (packet->gap_following && !m_packets.empty())
                                || ((packet->assigned_timecode - timecode) > g_max_ns_per_cluster)
                                || is_video_keyframe));

  if (is_video_keyframe)
    m_first_video_keyframe_seen = true;

  mxdebug_if(m_debug_rendering,
             boost::format("render check cur_tc %9% min_tc_ic %1% prev_cl_tc %2% test %3% is_vid_and_key %4% tc_delay %5% gap_following_and_not_empty %6% cur_tc>min_tc_ic %8% first_video_key_seen %10% do_render %7%\n")
             % m_min_timecode_in_cluster % m_previous_cluster_tc % (std::max<int64_t>(0, m_min_timecode_in_cluster) > m_previous_cluster_tc) % is_video_keyframe
             % timecode_delay % (packet->gap_following && !m_packets.empty()) % do_render % (packet->assigned_timecode > m_min_timecode_in_cluster) % packet->assigned_timecode % m_first_video_keyframe_seen);

  if (!do_render)
    return;

  render();
  prepare_new_cluster();
}

void
cluster_helper_c::render_after_adding_if_necessary(packet_cptr &packet) {
  // Render the cluster if it is full (according to my many criteria).
  auto timecode = get_timecode();
  if (   ((packet->assigned_timecode - timecode) > g_max_ns_per_cluster)
      || (m_packets.size()                       > static_cast<size_t>(g_max_blocks_per_cluster))
      || (get_cluster_content_size()             > 1500000)) {
    render();
    prepare_new_cluster();
  }
}

void
cluster_helper_c::split_if_necessary(packet_cptr &packet) {
  if (   !splitting()
      || (m_split_points.end() == m_current_split_point)
      || (g_file_num > g_split_max_num_files)
      || !packet->is_key_frame()
      || (   (packet->source->get_track_type() != track_video)
          && g_video_packetizer))
    return;

  bool split_now = false;

  // Maybe we want to start a new file now.
  if (split_point_t::SPT_SIZE == m_current_split_point->m_type) {
    int64_t additional_size = 0;

    if (!m_packets.empty())
      // Cluster + Cluster timecode: roughly 21 bytes. Add all frame sizes & their overheaders, too.
      additional_size = 21 + boost::accumulate(m_packets, 0, [](size_t size, const packet_cptr &p) { return size + p->data->get_size() + (p->is_key_frame() ? 10 : p->is_p_frame() ? 13 : 16); });

    additional_size += 18 * m_num_cue_elements;

    mxdebug_if(m_debug_splitting,
               boost::format("cluster_helper split decision: header_overhead: %1%, additional_size: %2%, bytes_in_file: %3%, sum: %4%\n")
               % m_header_overhead % additional_size % m_bytes_in_file % (m_header_overhead + additional_size + m_bytes_in_file));
    if ((m_header_overhead + additional_size + m_bytes_in_file) >= m_current_split_point->m_point)
      split_now = true;

  } else if (   (split_point_t::SPT_DURATION == m_current_split_point->m_type)
             && (0 <= m_first_timecode_in_file)
             && (packet->assigned_timecode - m_first_timecode_in_file) >= m_current_split_point->m_point)
    split_now = true;

  else if (   (   (split_point_t::SPT_TIMECODE == m_current_split_point->m_type)
               || (split_point_t::SPT_PARTS    == m_current_split_point->m_type))
           && (packet->assigned_timecode >= m_current_split_point->m_point))
    split_now = true;

  else if (   (   (split_point_t::SPT_FRAME_FIELD       == m_current_split_point->m_type)
               || (split_point_t::SPT_PARTS_FRAME_FIELD == m_current_split_point->m_type))
           && (m_frame_field_number >= m_current_split_point->m_point))
    split_now = true;

  if (!split_now)
    return;

  split(packet);
}

void
cluster_helper_c::split(packet_cptr &packet) {
  render();

  m_num_cue_elements = 0;

  bool create_new_file       = m_current_split_point->m_create_new_file;
  bool previously_discarding = m_discarding;

  mxdebug_if(m_debug_splitting, boost::format("Splitting: splitpoint %1% reached before timecode %2%, create new? %3%.\n") % m_current_split_point->str() % format_timecode(packet->assigned_timecode) % create_new_file);

  if (create_new_file)
    finish_file();

  if (m_current_split_point->m_use_once) {
    if (   m_current_split_point->m_discard
        && (   (split_point_t::SPT_PARTS             == m_current_split_point->m_type)
            || (split_point_t::SPT_PARTS_FRAME_FIELD == m_current_split_point->m_type))
        && (m_split_points.end() == (m_current_split_point + 1))) {
      mxdebug_if(m_debug_splitting, boost::format("Splitting: Last part in 'parts:' splitting mode finished\n"));
      m_splitting_and_processed_fully = true;
    }

    m_discarding = m_current_split_point->m_discard;
    ++m_current_split_point;
  }

  if (create_new_file) {
    create_next_output_file();
    if (g_no_linking) {
      m_previous_cluster_tc = -1;
      m_timecode_offset = g_video_packetizer ? m_max_video_timecode_rendered : packet->assigned_timecode;
    }

    m_bytes_in_file                        = 0;
    m_first_timecode_in_file               = -1;
  }

  handle_discarded_duration(create_new_file, previously_discarding);

  prepare_new_cluster();
}

void
cluster_helper_c::add_packet(packet_cptr packet) {
  if (!m_cluster)
    prepare_new_cluster();

  packet->normalize_timecodes();
  render_before_adding_if_necessary(packet);
  split_if_necessary(packet);

  m_packets.push_back(packet);
  m_cluster_content_size += packet->data->get_size();

  if (packet->assigned_timecode > m_max_timecode_in_cluster)
    m_max_timecode_in_cluster = packet->assigned_timecode;

  if ((-1 == m_min_timecode_in_cluster) || (packet->assigned_timecode < m_min_timecode_in_cluster))
    m_min_timecode_in_cluster = packet->assigned_timecode;

  render_after_adding_if_necessary(packet);

  if (g_video_packetizer == packet->source)
    ++m_frame_field_number;
}

int64_t
cluster_helper_c::get_timecode() {
  return m_packets.empty() ? 0 : m_packets.front()->assigned_timecode;
}

void
cluster_helper_c::prepare_new_cluster() {
  delete m_cluster;

  m_cluster              = new kax_cluster_c;
  m_cluster_content_size = 0;
  m_packets.clear();

  m_cluster->SetParent(*g_kax_segment);
  m_cluster->SetPreviousTimecode(std::max<int64_t>(0, m_previous_cluster_tc), (int64_t)g_timecode_scale);
}

int
cluster_helper_c::get_cluster_content_size() {
  return m_cluster_content_size;
}

void
cluster_helper_c::set_output(mm_io_c *out) {
  m_out = out;
}

void
cluster_helper_c::set_duration(render_groups_c *rg) {
  if (rg->m_durations.empty())
    return;

  kax_block_blob_c *group = rg->m_groups.back().get();
  int64_t def_duration    = rg->m_source->get_track_default_duration();
  int64_t block_duration  = 0;

  size_t i;
  for (i = 0; rg->m_durations.size() > i; ++i)
    block_duration += rg->m_durations[i];
  mxdebug_if(m_debug_duration,
             boost::format("cluster_helper::set_duration: block_duration %1% rounded duration %2% def_duration %3% use_durations %4% rg->m_duration_mandatory %5%\n")
             % block_duration % RND_TIMECODE_SCALE(block_duration) % def_duration % (g_use_durations ? 1 : 0) % (rg->m_duration_mandatory ? 1 : 0));

  if (rg->m_duration_mandatory) {
    if (   (0 == block_duration)
        || (   (0 < block_duration)
            && (block_duration != (static_cast<int64_t>(rg->m_durations.size()) * def_duration))))
      group->set_block_duration(RND_TIMECODE_SCALE(block_duration));

  } else if (   (   g_use_durations
                 || (0 < def_duration))
             && (0 < block_duration)
             && (RND_TIMECODE_SCALE(block_duration) != RND_TIMECODE_SCALE(rg->m_durations.size() * def_duration)))
    group->set_block_duration(RND_TIMECODE_SCALE(block_duration));
}

bool
cluster_helper_c::must_duration_be_set(render_groups_c *rg,
                                       packet_cptr &new_packet) {
  size_t i;
  int64_t block_duration = 0;
  int64_t def_duration   = rg->m_source->get_track_default_duration();

  for (i = 0; rg->m_durations.size() > i; ++i)
    block_duration += rg->m_durations[i];
  block_duration += new_packet->get_duration();

  if (rg->m_duration_mandatory || new_packet->duration_mandatory) {
    if (   (0 == block_duration)
        || (   (0 < block_duration)
            && (block_duration != ((static_cast<int64_t>(rg->m_durations.size()) + 1) * def_duration))))
      return true;

  } else if (   (   g_use_durations
                 || (0 < def_duration))
             && (0 < block_duration)
             && (RND_TIMECODE_SCALE(block_duration) != RND_TIMECODE_SCALE((rg->m_durations.size() + 1) * def_duration)))
    return true;

  return false;
}

/*
  <+Asylum> The chicken and the egg are lying in bed next to each
            other after a good hard shag, the chicken is smoking a
            cigarette, the egg says "Well that answers that bloody
            question doesn't it"
*/

int
cluster_helper_c::render() {
  std::vector<render_groups_cptr> render_groups;

  m_id_timecode_duration_map.clear();

  bool use_simpleblock    = !hack_engaged(ENGAGE_NO_SIMPLE_BLOCKS);

  LacingType lacing_type  = hack_engaged(ENGAGE_LACING_XIPH) ? LACING_XIPH : hack_engaged(ENGAGE_LACING_EBML) ? LACING_EBML : LACING_AUTO;

  int64_t min_cl_timecode = std::numeric_limits<int64_t>::max();
  int64_t max_cl_timecode = 0;

  int elements_in_cluster = 0;
  bool added_to_cues      = false;

  // Splitpoint stuff
  if ((-1 == m_header_overhead) && splitting())
    m_header_overhead = m_out->getFilePointer() + g_tags_size;

  // Make sure that we don't have negative/wrapped around timecodes in the output file.
  // Can happend when we're splitting; so adjust timecode_offset accordingly.
  m_timecode_offset       = boost::accumulate(m_packets, m_timecode_offset, [](int64_t a, const packet_cptr &p) { return std::min(a, p->assigned_timecode); });
  int64_t timecode_offset = m_timecode_offset + get_discarded_duration();

  for (auto &pack : m_packets) {
    generic_packetizer_c *source = pack->source;
    bool has_codec_state         = !!pack->codec_state;

    if (g_video_packetizer == source)
      m_max_video_timecode_rendered = std::max(pack->assigned_timecode + pack->get_duration(), m_max_video_timecode_rendered);

    if (discarding()) {
      if (-1 == m_first_discarded_timecode)
        m_first_discarded_timecode = pack->assigned_timecode;
      m_last_discarded_timecode_and_duration = std::max(m_last_discarded_timecode_and_duration, pack->assigned_timecode + pack->get_duration());
      continue;
    }

    if (source->contains_gap())
      m_cluster->SetSilentTrackUsed();

    render_groups_c *render_group = nullptr;
    for (auto &rg : render_groups)
      if (rg->m_source == source) {
        render_group = rg.get();
        break;
      }

    if (!render_group) {
      render_groups.push_back(render_groups_cptr(new render_groups_c(source)));
      render_group = render_groups.back().get();
    }

    min_cl_timecode                        = std::min(pack->assigned_timecode, min_cl_timecode);
    max_cl_timecode                        = std::max(pack->assigned_timecode, max_cl_timecode);

    DataBuffer *data_buffer                = new DataBuffer((binary *)pack->data->get_buffer(), pack->data->get_size());

    KaxTrackEntry &track_entry             = static_cast<KaxTrackEntry &>(*source->get_track_entry());

    kax_block_blob_c *previous_block_group = !render_group->m_groups.empty() ? render_group->m_groups.back().get() : nullptr;
    kax_block_blob_c *new_block_group      = previous_block_group;

    if (!pack->is_key_frame() || has_codec_state)
      render_group->m_more_data = false;

    if (!render_group->m_more_data) {
      set_duration(render_group);
      render_group->m_durations.clear();
      render_group->m_duration_mandatory = false;

      BlockBlobType this_block_blob_type
        = !use_simpleblock                         ? BLOCK_BLOB_NO_SIMPLE
        : must_duration_be_set(render_group, pack) ? BLOCK_BLOB_NO_SIMPLE
        : !pack->data_adds.empty()                 ? BLOCK_BLOB_NO_SIMPLE
        :                                            BLOCK_BLOB_ALWAYS_SIMPLE;

      if (has_codec_state)
        this_block_blob_type = BLOCK_BLOB_NO_SIMPLE;

      render_group->m_groups.push_back(kax_block_blob_cptr(new kax_block_blob_c(this_block_blob_type)));
      new_block_group = render_group->m_groups.back().get();
      m_cluster->AddBlockBlob(new_block_group);
      new_block_group->SetParent(*m_cluster);

      added_to_cues = false;
    }

    // Now put the packet into the cluster.
    render_group->m_more_data = new_block_group->add_frame_auto(track_entry, pack->assigned_timecode - timecode_offset, *data_buffer, lacing_type,
                                                                pack->has_bref() ? pack->bref - timecode_offset : -1,
                                                                pack->has_fref() ? pack->fref - timecode_offset : -1);

    if (has_codec_state) {
      KaxBlockGroup &bgroup = (KaxBlockGroup &)*new_block_group;
      KaxCodecState *cstate = new KaxCodecState;
      bgroup.PushElement(*cstate);
      cstate->CopyBuffer(pack->codec_state->get_buffer(), pack->codec_state->get_size());
    }

    if (-1 == m_first_timecode_in_file)
      m_first_timecode_in_file = pack->assigned_timecode;

    m_max_timecode_and_duration = std::max(pack->assigned_timecode + pack->get_duration(), m_max_timecode_and_duration);

    if (!pack->is_key_frame() || !track_entry.LacingEnabled())
      render_group->m_more_data = false;

    render_group->m_durations.push_back(pack->get_unmodified_duration());
    render_group->m_duration_mandatory |= pack->duration_mandatory;

    m_id_timecode_duration_map[ id_timecode_t{ source->get_track_num(), pack->assigned_timecode - timecode_offset } ] = pack->get_duration();

    if (new_block_group) {
      // Set the reference priority if it was wanted.
      if ((0 < pack->ref_priority) && new_block_group->replace_simple_by_group())
        GetChild<KaxReferencePriority>(*new_block_group).SetValue(pack->ref_priority);

      // Handle BlockAdditions if needed
      if (!pack->data_adds.empty() && new_block_group->ReplaceSimpleByGroup()) {
        KaxBlockAdditions &additions = AddEmptyChild<KaxBlockAdditions>(*new_block_group);

        size_t data_add_idx;
        for (data_add_idx = 0; pack->data_adds.size() > data_add_idx; ++data_add_idx) {
          auto &block_more = AddEmptyChild<KaxBlockMore>(additions);
          GetChild<KaxBlockAddID     >(block_more).SetValue(data_add_idx + 1);
          GetChild<KaxBlockAdditional>(block_more).CopyBuffer((binary *)pack->data_adds[data_add_idx]->get_buffer(), pack->data_adds[data_add_idx]->get_size());
        }
      }
    }

    elements_in_cluster++;

    if (!new_block_group)
      new_block_group = previous_block_group;

    else if (g_write_cues && (!added_to_cues || has_codec_state))
      added_to_cues = add_to_cues_maybe(pack, *new_block_group);

    pack->group = new_block_group;
  }

  if (!discarding()) {
    if (0 < elements_in_cluster) {
      for (auto &rg : render_groups)
        set_duration(rg.get());

      m_cluster->SetPreviousTimecode(min_cl_timecode - timecode_offset - 1, (int64_t)g_timecode_scale);
      m_cluster->set_min_timecode(min_cl_timecode - timecode_offset);
      m_cluster->set_max_timecode(max_cl_timecode - timecode_offset);

      m_cluster->Render(*m_out, *g_kax_cues);
      m_bytes_in_file += m_cluster->ElementSize();

      if (g_kax_sh_cues)
        g_kax_sh_cues->IndexThis(*m_cluster, *g_kax_segment);

      m_previous_cluster_tc = m_cluster->GlobalTimecode();

      postprocess_cues();
    } else
      m_previous_cluster_tc = -1;
  }

  m_min_timecode_in_cluster = -1;
  m_max_timecode_in_cluster = -1;

  m_cluster->delete_non_blocks();

  return 1;
}

bool
cluster_helper_c::add_to_cues_maybe(packet_cptr &pack,
                                    kax_block_blob_c &block_group) {
  auto &source  = *pack->source;
  auto strategy = source.get_cue_creation();

  // Update the cues (index table) either if cue entries for I frames were requested and this is an I frame...
  bool add = (CUE_STRATEGY_IFRAMES == strategy) && pack->is_key_frame();

  // ... or if a codec state change is present ...
  add = add || !!pack->codec_state;

  // ... or if the user requested entries for all frames ...
  add = add || (CUE_STRATEGY_ALL == strategy);

  // ... or if this is an audio track, there is no video track and the
  // last cue entry was created more than 2s ago.
  add = add || (   (CUE_STRATEGY_SPARSE == strategy)
                && (track_audio         == source.get_track_type())
                && !g_video_packetizer
                && (   (0 > source.get_last_cue_timecode())
                    || ((pack->assigned_timecode - source.get_last_cue_timecode()) >= 2000000000)));

  if (!add)
    return false;

  g_kax_cues->AddBlockBlob(block_group);
  source.set_last_cue_timecode(pack->assigned_timecode);

  ++m_num_cue_elements;
  g_cue_writing_requested = 1;

  return true;
}

std::map<id_timecode_t, int64_t>
cluster_helper_c::calculate_block_positions()
  const {

  std::map<id_timecode_t, int64_t> positions;

  for (auto child : *m_cluster) {
    auto simple_block = dynamic_cast<KaxSimpleBlock *>(child);
    if (simple_block) {
      simple_block->SetParent(*m_cluster);
      positions[ id_timecode_t{ simple_block->TrackNum(), simple_block->GlobalTimecode() } ] = simple_block->GetElementPosition();
      continue;
    }

    auto block_group = dynamic_cast<KaxBlockGroup *>(child);
    if (!block_group)
      continue;

    auto block = FindChild<KaxBlock>(block_group);
    if (!block)
      continue;

    block->SetParent(*m_cluster);
    positions[ id_timecode_t{ block->TrackNum(), block->GlobalTimecode() } ] = block->GetElementPosition();
  }

  return positions;
}

void
cluster_helper_c::postprocess_cues() {
  if (!g_kax_cues || !m_cluster)
    return;

  if (m_no_cue_duration && m_no_cue_relative_position)
    return;

  auto cluster_data_start_pos = m_cluster->GetElementPosition() + m_cluster->HeadSize();
  auto block_positions        = calculate_block_positions();

  auto &children = g_kax_cues->GetElementList();
  for (auto size = children.size(); m_num_cue_points_postprocessed < size; ++m_num_cue_points_postprocessed) {
    auto point = dynamic_cast<KaxCuePoint *>(children[m_num_cue_points_postprocessed]);
    if (!point)
      continue;

    auto time = static_cast<int64_t>(GetChild<KaxCueTime>(point).GetValue() * g_timecode_scale);

    for (auto child : *point) {
      auto positions = dynamic_cast<KaxCueTrackPositions *>(child);
      if (!positions)
        continue;

      auto track_num = GetChild<KaxCueTrack>(positions).GetValue();

      // Set CueRelativePosition for all cues.
      if (!m_no_cue_relative_position) {
        auto position_itr = block_positions.find({ track_num, time });
        auto position     = block_positions.end() != position_itr ? std::max<int64_t>(position_itr->second, cluster_data_start_pos) : 0ll;
        if (position)
          GetChild<KaxCueRelativePosition>(positions).SetValue(position - cluster_data_start_pos);

        mxdebug_if(m_debug_cue_relative_position,
                   boost::format("cue_relative_position: looking for <%1%:%2%>: cluster_data_start_pos %3% position %4%\n")
                   % track_num % time % cluster_data_start_pos % position);
      }

      // Set CueDuration if the packetizer wants them.
      if (m_no_cue_duration)
        continue;

      auto duration_itr = m_id_timecode_duration_map.find({ track_num, time });
      auto ptzr         = g_packetizers_by_track_num[track_num];

      if (!ptzr || !ptzr->wants_cue_duration())
        continue;

      if ((m_id_timecode_duration_map.end() != duration_itr) && (0 < duration_itr->second))
        GetChild<KaxCueDuration>(positions).SetValue(RND_TIMECODE_SCALE(duration_itr->second) / g_timecode_scale);

      mxdebug_if(m_debug_cue_duration, boost::format("cue_duration: looking for <%1%:%2%>: %3%\n") % track_num % time % (duration_itr == m_id_timecode_duration_map.end() ? static_cast<int64_t>(-1) : duration_itr->second));
    }
  }
}

int64_t
cluster_helper_c::get_duration() {
  mxdebug_if(m_debug_duration,
             boost::format("cluster_helper_c::get_duration(): %1% - %2% - %4% = %3%\n")
             % m_max_timecode_and_duration % m_first_timecode_in_file % (m_max_timecode_and_duration - m_first_timecode_in_file) % get_discarded_duration());

  return m_max_timecode_and_duration - m_first_timecode_in_file - get_discarded_duration();
}

int64_t
cluster_helper_c::get_discarded_duration()
  const {
  return m_discarded_duration;
}

void
cluster_helper_c::handle_discarded_duration(bool create_new_file,
                                            bool previously_discarding) {
  if (create_new_file) { // || (!previously_discarding && m_discarding)) {
    mxdebug_if(m_debug_splitting,
               boost::format("RESETTING discarded duration of %1%, create_new_file %2% previously_discarding %3% m_discarding %4%\n")
               % format_timecode(m_discarded_duration) % create_new_file % previously_discarding % m_discarding);
    m_discarded_duration = 0;

  } else if (previously_discarding && !m_discarding) {
    auto diff             = m_last_discarded_timecode_and_duration - std::max<int64_t>(m_first_discarded_timecode, 0);
    m_discarded_duration += diff;

    mxdebug_if(m_debug_splitting,
               boost::format("ADDING to discarded duration TC at %1% / %2% diff %3% new total %4% create_new_file %5% previously_discarding %6% m_discarding %7%\n")
               % format_timecode(m_first_discarded_timecode) % format_timecode(m_last_discarded_timecode_and_duration) % format_timecode(diff) % format_timecode(m_discarded_duration)
               % create_new_file % previously_discarding % m_discarding);
  } else
    mxdebug_if(m_debug_splitting,
               boost::format("KEEPING discarded duration at %1%, create_new_file %2% previously_discarding %3% m_discarding %4%\n")
               % format_timecode(m_discarded_duration) % create_new_file % previously_discarding % m_discarding);

  m_first_discarded_timecode             = -1;
  m_last_discarded_timecode_and_duration =  0;
}

void
cluster_helper_c::add_split_point(const split_point_t &split_point) {
  m_split_points.push_back(split_point);
  m_current_split_point = m_split_points.begin();
  m_discarding          = m_current_split_point->m_discard;

  if (0 == m_current_split_point->m_point)
    ++m_current_split_point;
}

bool
cluster_helper_c::split_mode_produces_many_files()
  const {
  if (!splitting())
    return false;

  if (   (split_point_t::SPT_PARTS             != m_split_points.front().m_type)
      && (split_point_t::SPT_PARTS_FRAME_FIELD != m_split_points.front().m_type))
    return true;

  bool first = true;
  for (auto &split_point : m_split_points)
    if (!split_point.m_discard && split_point.m_create_new_file) {
      if (!first)
        return true;
      first = false;
    }

  return false;
}

void
cluster_helper_c::discard_queued_packets() {
  m_packets.clear();
}

void
cluster_helper_c::dump_split_points()
  const {
  mxdebug_if(m_debug_splitting,
             boost::format("Split points:%1%\n")
             % boost::accumulate(m_split_points, std::string(""), [](std::string const &accu, split_point_t const &point) { return accu + " " + point.str(); }));
}

cluster_helper_c *g_cluster_helper = nullptr;
