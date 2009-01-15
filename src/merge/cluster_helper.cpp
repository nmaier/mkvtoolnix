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

#include "os.h"

#include <algorithm>
#include <cassert>
#include <limits.h>
#include <vector>

#include "cluster_helper.h"
#include "common.h"
#include "commonebml.h"
#include "hacks.h"
#include "libmatroska_extensions.h"
#include "output_control.h"
#include "p_video.h"
#include "p_vorbis.h"

#include <matroska/KaxBlock.h>
#include <matroska/KaxBlockData.h>
#include <matroska/KaxSeekHead.h>

cluster_helper_c::cluster_helper_c()
  : m_cluster(NULL)
  , m_cluster_content_size(0)
  , m_max_timecode_and_duration(0)
  , m_max_video_timecode_rendered(0)
  , m_previous_cluster_tc(0)
  , m_num_cue_elements(0)
  , m_header_overhead(-1)
  , m_packet_num(0)
  , m_timecode_offset(0)
  , m_previous_packets(NULL)
  , m_bytes_in_file(0)
  , m_first_timecode_in_file(-1)
  , m_min_timecode_in_cluster(-1)
  , m_max_timecode_in_cluster(-1)
  , m_out(NULL)
  , m_current_split_point(m_split_points.begin())
{
}

cluster_helper_c::~cluster_helper_c() {
  delete m_cluster;
}

void
cluster_helper_c::add_packet(packet_cptr packet) {
  if (!m_cluster)
    prepare_new_cluster();

  // Normalize the timecodes according to the timecode scale.
  packet->unmodified_assigned_timecode = packet->assigned_timecode;
  packet->unmodified_duration          = packet->duration;
  packet->timecode                     = RND_TIMECODE_SCALE(packet->timecode);
  packet->assigned_timecode            = RND_TIMECODE_SCALE(packet->assigned_timecode);
  if (0 < packet->duration)
    packet->duration                   = RND_TIMECODE_SCALE(packet->duration);
  if (0 < packet->bref)
    packet->bref                       = RND_TIMECODE_SCALE(packet->bref);
  if (0 < packet->fref)
    packet->fref                       = RND_TIMECODE_SCALE(packet->fref);

  int64_t timecode        = get_timecode();
  int64_t timecode_delay  = (   (packet->assigned_timecode > m_max_timecode_in_cluster)
                             || (-1 == m_max_timecode_in_cluster))                       ? packet->assigned_timecode : m_max_timecode_in_cluster;
  timecode_delay         -= (   (-1 == m_min_timecode_in_cluster)
                             || (packet->assigned_timecode < m_min_timecode_in_cluster)) ? packet->assigned_timecode : m_min_timecode_in_cluster;
  timecode_delay          = (int64_t)(timecode_delay / g_timecode_scale);

  mxverb(4,
         boost::format("cluster_helper_c::add_packet(): new packet { source %1%/%2% "
                       "timecode: %3% duration: %4% bref: %5% fref: %6% assigned_timecode: %7% timecode_delay: %8% }\n")
         % packet->source->ti.id     % packet->source->ti.fname % packet->timecode          % packet->duration
         % packet->bref              % packet->fref             % packet->assigned_timecode % format_timecode(timecode_delay));

  if (   (SHRT_MAX < timecode_delay)
      || (SHRT_MIN > timecode_delay)
      || (packet->gap_following && !m_packets.empty())
      || ((packet->assigned_timecode - timecode) > g_max_ns_per_cluster)) {
    render();
    prepare_new_cluster();
  }

  if (   splitting()
      && (m_split_points.end() != m_current_split_point)
      && (g_file_num <= g_split_max_num_files)
      && (packet->bref == -1)
      && (   (packet->source->get_track_type() == track_video)
          || (NULL == g_video_packetizer))) {
    bool split_now = false;

    // Maybe we want to start a new file now.
    if (split_point_t::SPT_SIZE == m_current_split_point->m_type) {
      int64_t additional_size = 0;

      if (!m_packets.empty()) {
        // Cluster + Cluster timecode (roughly)
        additional_size = 21;

        // Add sizes for all frames.
        vector<packet_cptr>::iterator p_it;
        mxforeach(p_it, m_packets) {
          packet_cptr &p   = *p_it;
          additional_size += p->data->get_size() + ((-1 == p->bref) ? 10 : (-1 == p->fref) ? 13 : 16);
        }

      } else
        additional_size = 0;

      if (0 < m_num_cue_elements) {
        g_kax_cues->UpdateSize();
        additional_size += g_kax_cues->ElementSize();
      }

      mxverb(3,
             boost::format("cluster_helper split decision: header_overhead: %1%, additional_size: %2%, bytes_in_file: %3%, sum: %4%\n")
             % m_header_overhead % additional_size % m_bytes_in_file % (m_header_overhead + additional_size + m_bytes_in_file));
      if ((m_header_overhead + additional_size + m_bytes_in_file) >= m_current_split_point->m_point)
        split_now = true;

    } else if (   (split_point_t::SPT_DURATION == m_current_split_point->m_type)
               && (0 <= m_first_timecode_in_file)
               && (packet->assigned_timecode - m_first_timecode_in_file) >= m_current_split_point->m_point)
      split_now = true;

    else if (   (split_point_t::SPT_TIMECODE == m_current_split_point->m_type)
             && (packet->assigned_timecode >= m_current_split_point->m_point))
      split_now = true;

    if (split_now) {
      render();

      m_num_cue_elements = 0;

      mxinfo("\n");
      finish_file();
      create_next_output_file();
      if (g_no_linking)
        m_previous_cluster_tc = 0;

      m_bytes_in_file          = 0;
      m_first_timecode_in_file = -1;

      if (g_no_linking)
        m_timecode_offset = g_video_packetizer ? m_max_video_timecode_rendered : packet->assigned_timecode;

      if (m_current_split_point->m_use_once)
        ++m_current_split_point;

      prepare_new_cluster();
    }
  }

  packet->packet_num = m_packet_num;
  m_packet_num++;

  m_packets.push_back(packet);
  m_cluster_content_size += packet->data->get_size();

  if (packet->assigned_timecode > m_max_timecode_in_cluster)
    m_max_timecode_in_cluster = packet->assigned_timecode;

  if ((-1 == m_min_timecode_in_cluster) || (packet->assigned_timecode < m_min_timecode_in_cluster))
    m_min_timecode_in_cluster = packet->assigned_timecode;

  // Render the cluster if it is full (according to my many criteria).
  timecode = get_timecode();
  if (((packet->assigned_timecode - timecode) > g_max_ns_per_cluster) || (m_packets.size() > g_max_blocks_per_cluster) || (get_cluster_content_size() > 1500000)) {
    render();
    prepare_new_cluster();
  }
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
  m_cluster->SetPreviousTimecode(m_previous_cluster_tc, (int64_t)g_timecode_scale);
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
cluster_helper_c::set_duration(render_groups_t *rg) {
  if (rg->durations.empty())
    return;

  kax_block_blob_c *group = rg->groups.back();
  int64_t def_duration    = rg->source->get_track_default_duration();
  int64_t block_duration  = 0;

  int i;
  for (i = 0; rg->durations.size() > i; ++i)
    block_duration += rg->durations[i];
  mxverb(3,
         boost::format("cluster_helper::set_duration: block_duration %1% rounded duration %2% def_duration %3% use_durations %4% rg->duration_mandatory %5%\n")
         % block_duration % RND_TIMECODE_SCALE(block_duration) % def_duration % (g_use_durations ? 1 : 0) % (rg->duration_mandatory ? 1 : 0));

  if (rg->duration_mandatory) {
    if (   (0 == block_duration)
        || (   (0 < block_duration)
            && (block_duration != (rg->durations.size() * def_duration))))
      group->set_block_duration(RND_TIMECODE_SCALE(block_duration));

  } else if (   (   g_use_durations
                 || (0 < def_duration))
             && (0 < block_duration)
             && (RND_TIMECODE_SCALE(block_duration) != RND_TIMECODE_SCALE(rg->durations.size() * def_duration)))
    group->set_block_duration(RND_TIMECODE_SCALE(block_duration));
}

bool
cluster_helper_c::must_duration_be_set(render_groups_t *rg,
                                       packet_cptr &new_packet) {
  int i;
  int64_t block_duration = 0;
  int64_t def_duration   = rg->source->get_track_default_duration();

  for (i = 0; rg->durations.size() > i; ++i)
    block_duration += rg->durations[i];
  block_duration += new_packet->duration;

  if (rg->duration_mandatory || new_packet->duration_mandatory) {
    if (   (0 == block_duration)
        || (   (0 < block_duration)
            && (block_duration != ((rg->durations.size() + 1) * def_duration))))
      return true;

  } else if (   (   g_use_durations
                 || (0 < def_duration))
             && (0 < block_duration)
             && (RND_TIMECODE_SCALE(block_duration) != RND_TIMECODE_SCALE((rg->durations.size() + 1) * def_duration)))
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
  vector<render_groups_t *> render_groups;
  vector<render_groups_t *>::iterator rg_it;

  bool use_simpleblock              = hack_engaged(ENGAGE_USE_SIMPLE_BLOCK);

  BlockBlobType std_block_blob_type = use_simpleblock ? BLOCK_BLOB_ALWAYS_SIMPLE : BLOCK_BLOB_NO_SIMPLE;

  LacingType lacing_type            = hack_engaged(ENGAGE_LACING_XIPH) ? LACING_XIPH : hack_engaged(ENGAGE_LACING_EBML) ? LACING_EBML : LACING_AUTO;

  int64_t min_cl_timecode           = 9223372036854775807LL;
  int64_t max_cl_timecode           = 0;

  int elements_in_cluster           = 0;
  bool added_to_cues                = false;

  // Splitpoint stuff
  if ((-1 == m_header_overhead) && splitting())
    m_header_overhead = m_out->getFilePointer() + g_tags_size;

  // Make sure that we don't have negative/wrapped around timecodes in the output file.
  // Can happend when we're splitting; so adjust timecode_offset accordingly.
  vector<packet_cptr>::iterator pack_it;
  mxforeach(pack_it, m_packets)
    m_timecode_offset = std::min(m_timecode_offset, (*pack_it)->assigned_timecode);

  mxforeach(pack_it, m_packets) {
    packet_cptr &pack            = *pack_it;
    generic_packetizer_c *source = pack->source;
    bool has_codec_state         = NULL != pack->codec_state.get();

    if (source->contains_gap())
      m_cluster->SetSilentTrackUsed();

    render_groups_t *render_group = NULL;
    mxforeach(rg_it, render_groups)
      if ((*rg_it)->source == source) {
        render_group = *rg_it;
        break;
      }

    if (NULL == render_group) {
      render_group                     = new render_groups_t;
      render_group->source             = source;
      render_group->more_data          = false;
      render_group->duration_mandatory = false;

      render_groups.push_back(render_group);
    }

    min_cl_timecode                        = std::min(pack->assigned_timecode, min_cl_timecode);
    max_cl_timecode                        = std::max(pack->assigned_timecode, max_cl_timecode);

    DataBuffer *data_buffer                = new DataBuffer((binary *)pack->data->get(), pack->data->get_size());

    KaxTrackEntry &track_entry             = static_cast<KaxTrackEntry &>(*source->get_track_entry());

    kax_block_blob_c *previous_block_group = !render_group->groups.empty() ? render_group->groups.back() : NULL;
    kax_block_blob_c *new_block_group      = previous_block_group;

    if ((-1 != pack->bref) || has_codec_state)
      render_group->more_data = false;

    if (!render_group->more_data) {
      set_duration(render_group);
      render_group->durations.clear();
      render_group->duration_mandatory = false;

      BlockBlobType this_block_blob_type
        = !use_simpleblock                         ? std_block_blob_type
        : must_duration_be_set(render_group, pack) ? BLOCK_BLOB_NO_SIMPLE
        :                                            BLOCK_BLOB_ALWAYS_SIMPLE;

      if (has_codec_state)
        this_block_blob_type = BLOCK_BLOB_NO_SIMPLE;

      new_block_group = new kax_block_blob_c(this_block_blob_type);
      m_cluster->AddBlockBlob(new_block_group);
      new_block_group->SetParent(*m_cluster);
      render_group->groups.push_back(new_block_group);

      added_to_cues = false;
    }

    // Now put the packet into the cluster.
    render_group->more_data = new_block_group->add_frame_auto(track_entry, pack->assigned_timecode - m_timecode_offset, *data_buffer, lacing_type,
                                                              pack->bref - m_timecode_offset, pack->fref - m_timecode_offset);

    if (has_codec_state) {
      KaxBlockGroup &bgroup = (KaxBlockGroup &)*new_block_group;
      KaxCodecState *cstate = new KaxCodecState;
      bgroup.PushElement(*cstate);
      cstate->CopyBuffer(pack->codec_state->get(), pack->codec_state->get_size());
    }

    if (-1 == m_first_timecode_in_file)
      m_first_timecode_in_file = pack->assigned_timecode;

    if ((pack->assigned_timecode + pack->duration) > m_max_timecode_and_duration)
      m_max_timecode_and_duration = pack->assigned_timecode + pack->duration;

    if ((-1 != pack->bref) || (-1 != pack->fref) || !track_entry.LacingEnabled())
      render_group->more_data = false;

    render_group->durations.push_back(pack->unmodified_duration);
    render_group->duration_mandatory |= pack->duration_mandatory;

    if (NULL != new_block_group) {
      // Set the reference priority if it was wanted.
      if ((0 < pack->ref_priority) && new_block_group->replace_simple_by_group())
        *static_cast<EbmlUInteger *>(&GetChild<KaxReferencePriority>(*new_block_group)) = pack->ref_priority;

      // Handle BlockAdditions if needed
      if (!pack->data_adds.empty() && new_block_group->ReplaceSimpleByGroup()) {
        KaxBlockAdditions &additions = AddEmptyChild<KaxBlockAdditions>(*new_block_group);

        int data_add_idx;
        for (data_add_idx = 0; pack->data_adds.size() > data_add_idx; ++data_add_idx) {
          KaxBlockMore &block_more                                           = AddEmptyChild<KaxBlockMore>(additions);
          *static_cast<EbmlUInteger *>(&GetChild<KaxBlockAddID>(block_more)) = data_add_idx + 1;

          GetChild<KaxBlockAdditional>(block_more).CopyBuffer((binary *)pack->data_adds[data_add_idx]->get(), pack->data_adds[data_add_idx]->get_size());
        }
      }
    }

    elements_in_cluster++;

    if (NULL == new_block_group)
      new_block_group = previous_block_group;

    else if (g_write_cues && (!added_to_cues || has_codec_state)) {
      // Update the cues (index table) either if cue entries for
      // I frames were requested and this is an I frame...
      if ((   (CUE_STRATEGY_IFRAMES == source->get_cue_creation())
           && (-1 == pack->bref))
          // ... or if a codec state change is present ...
          || has_codec_state
          // ... or if the user requested entries for all frames ...
          || (CUE_STRATEGY_ALL == source->get_cue_creation())
          // ... or if this is an audio track, there is no video track and the
          // last cue entry was created more than 2s ago.
          || (   (CUE_STRATEGY_SPARSE == source->get_cue_creation())
              && (track_audio         == source->get_track_type())
              && (NULL                == g_video_packetizer)
              && (   (0 > source->get_last_cue_timecode())
                  || ((pack->assigned_timecode - source->get_last_cue_timecode()) >= 2000000000)))) {

        g_kax_cues->AddBlockBlob(*new_block_group);
        source->set_last_cue_timecode(pack->assigned_timecode);

        m_num_cue_elements++;
        g_cue_writing_requested = 1;
        added_to_cues           = true;
      }
    }

    pack->group = new_block_group;

    if (   (g_video_packetizer == source)
        && ((pack->assigned_timecode + pack->duration) > m_max_video_timecode_rendered))
      m_max_video_timecode_rendered = pack->assigned_timecode + pack->duration;
  }

  if (0 < elements_in_cluster) {
    mxforeach(rg_it, render_groups)
      set_duration(*rg_it);

    m_cluster->SetPreviousTimecode(min_cl_timecode - m_timecode_offset - 1, (int64_t)g_timecode_scale);
    m_cluster->set_min_timecode(min_cl_timecode - m_timecode_offset);
    m_cluster->set_max_timecode(max_cl_timecode - m_timecode_offset);

    m_cluster->Render(*m_out, *g_kax_cues);
    m_bytes_in_file += m_cluster->ElementSize();

    if (NULL != g_kax_sh_cues)
      g_kax_sh_cues->IndexThis(*m_cluster, *g_kax_segment);

    m_previous_cluster_tc = m_cluster->GlobalTimecode();
  } else
    m_previous_cluster_tc = 0;

  mxforeach(rg_it, render_groups)
    delete *rg_it;

  m_min_timecode_in_cluster = -1;
  m_max_timecode_in_cluster = -1;

  return 1;
}

int64_t
cluster_helper_c::get_duration() {
  mxverb(3,
         boost::format("cluster_helper_c::get_duration(): %1% - %2% = %3%\n")
         % m_max_timecode_and_duration % m_first_timecode_in_file % (m_max_timecode_and_duration - m_first_timecode_in_file));

  return m_max_timecode_and_duration - m_first_timecode_in_file;
}

void
cluster_helper_c::add_split_point(const split_point_t &split_point) {
  m_split_points.push_back(split_point);
  m_current_split_point = m_split_points.begin();
}

cluster_helper_c *g_cluster_helper = NULL;
