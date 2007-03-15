/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   The cluster helper groups frames into blocks groups and those
   into clusters, sets the durations, renders the clusters etc.

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <cassert>
#include <limits.h>

#include <vector>

#include <matroska/KaxBlock.h>
#include <matroska/KaxBlockData.h>
#include <matroska/KaxSeekHead.h>

#include "cluster_helper.h"
#include "common.h"
#include "commonebml.h"
#include "hacks.h"
#include "libmatroska_extensions.h"
#include "output_control.h"
#include "p_video.h"
#include "p_vorbis.h"

cluster_helper_c::cluster_helper_c():
  cluster(NULL),
  min_timecode_in_cluster(-1), max_timecode_in_cluster(-1),
  current_split_point(split_points.begin()) {

  cluster_content_size = 0;
  max_timecode_and_duration = 0;
  last_cluster_tc = 0;
  num_cue_elements = 0;
  header_overhead = -1;
  packet_num = 0;
  out = NULL;
  timecode_offset = 0;
  last_packets = NULL;
  first_timecode_in_file = -1;
  bytes_in_file = 0;
}

cluster_helper_c::~cluster_helper_c() {
  delete cluster;
}

void
cluster_helper_c::add_packet(packet_cptr packet) {
  int64_t timecode, timecode_delay, additional_size;
  int i;
  bool split;

  if (!cluster)
    prepare_new_cluster();

  // Normalize the timecodes according to the timecode scale.
  packet->unmodified_assigned_timecode = packet->assigned_timecode;
  packet->unmodified_duration = packet->duration;
  packet->timecode = RND_TIMECODE_SCALE(packet->timecode);
  if (packet->duration > 0)
    packet->duration = RND_TIMECODE_SCALE(packet->duration);
  packet->assigned_timecode = RND_TIMECODE_SCALE(packet->assigned_timecode);
  if (packet->bref > 0)
    packet->bref = RND_TIMECODE_SCALE(packet->bref);
  if (packet->fref > 0)
    packet->fref = RND_TIMECODE_SCALE(packet->fref);

  timecode = get_timecode();

  timecode_delay =
    (packet->assigned_timecode > max_timecode_in_cluster) ||
    (-1 == max_timecode_in_cluster) ?
    packet->assigned_timecode : max_timecode_in_cluster;
  timecode_delay -=
    (-1 == min_timecode_in_cluster) ||
    (packet->assigned_timecode < min_timecode_in_cluster) ?
    packet->assigned_timecode : min_timecode_in_cluster;
  timecode_delay = (int64_t)(timecode_delay / timecode_scale);

  mxverb(4, "cluster_helper_c::add_packet(): new packet { source " LLD "/%s "
         "timecode: " LLD " duration: " LLD " bref: " LLD " fref: " LLD " "
         "assigned_timecode: " LLD " timecode_delay: " FMT_TIMECODEN " }\n",
         packet->source->ti.id, packet->source->ti.fname.c_str(),
         packet->timecode, packet->duration, packet->bref, packet->fref,
         packet->assigned_timecode, ARG_TIMECODEN(timecode_delay));

  if ((SHRT_MAX < timecode_delay) || (SHRT_MIN > timecode_delay) ||
      (packet->gap_following && !packets.empty()) ||
      ((packet->assigned_timecode - timecode) > max_ns_per_cluster)) {
    render();
    prepare_new_cluster();
  }

  if (splitting() &&
      (split_points.end() != current_split_point) &&
      (file_num <= split_max_num_files) &&
      (packet->bref == -1) &&
      ((packet->source->get_track_type() == track_video) ||
       (video_packetizer == NULL))) {
    split = false;

    // Maybe we want to start a new file now.
    if (split_point_t::SPT_SIZE == current_split_point->m_type) {

      if (!packets.empty()) {
        // Cluster + Cluster timecode (roughly)
        additional_size = 21;
        // Add sizes for all frames.
        for (i = 0; i < packets.size(); i++) {
          packet_cptr &p = packets[i];
          additional_size += p->data->get_size();
          if (p->bref == -1)
            additional_size += 10;
          else if (p->fref == -1)
            additional_size += 13;
          else
            additional_size += 16;
        }
      } else
        additional_size = 0;
      if (num_cue_elements > 0) {
        kax_cues->UpdateSize();
        additional_size += kax_cues->ElementSize();
      }
      mxverb(3, "cluster_helper split decision: header_overhead: " LLD ", "
             "additional_size: " LLD ", bytes_in_file: " LLD ", sum: " LLD
             "\n", header_overhead, additional_size, bytes_in_file,
             header_overhead + additional_size + bytes_in_file);
      if ((header_overhead + additional_size + bytes_in_file) >=
          current_split_point->m_point)
        split = true;

    } else if ((split_point_t::SPT_DURATION == current_split_point->m_type) &&
               (0 <= first_timecode_in_file) &&
               (packet->assigned_timecode - first_timecode_in_file) >=
               current_split_point->m_point)
      split = true;

    else if ((split_point_t::SPT_TIMECODE == current_split_point->m_type) &&
             (packet->assigned_timecode >= current_split_point->m_point))
      split = true;

    if (split) {
      render();

      num_cue_elements = 0;

      mxinfo("\n");
      finish_file();
      create_next_output_file();
      if (no_linking)
        last_cluster_tc = 0;

      bytes_in_file = 0;
      first_timecode_in_file = -1;

      if (no_linking)
        timecode_offset = packet->assigned_timecode;

      if (current_split_point->m_use_once)
        ++current_split_point;

      prepare_new_cluster();
    }
  }

  packet->packet_num = packet_num;
  packet_num++;

  packets.push_back(packet);
  cluster_content_size += packet->data->get_size();

  if (packet->assigned_timecode > max_timecode_in_cluster)
    max_timecode_in_cluster = packet->assigned_timecode;
  if ((-1 == min_timecode_in_cluster) ||
      (packet->assigned_timecode < min_timecode_in_cluster))
    min_timecode_in_cluster = packet->assigned_timecode;

  // Render the cluster if it is full (according to my many criteria).
  timecode = get_timecode();
  if (((packet->assigned_timecode - timecode) > max_ns_per_cluster) ||
      (packets.size() > max_blocks_per_cluster) ||
      (get_cluster_content_size() > 1500000)) {
    render();
    prepare_new_cluster();
  }
}

int64_t
cluster_helper_c::get_timecode() {
  return packets.empty() ? 0 : packets.front()->assigned_timecode;
}

void
cluster_helper_c::prepare_new_cluster() {
  if (cluster)
    delete cluster;
  cluster = new kax_cluster_c;
  cluster_content_size = 0;
  cluster->SetParent(*kax_segment);
  cluster->SetPreviousTimecode(last_cluster_tc, (int64_t)timecode_scale);
  packets.clear();
}

int
cluster_helper_c::get_cluster_content_size() {
  return cluster_content_size;
}

void
cluster_helper_c::set_output(mm_io_c *nout) {
  out = nout;
}

void
cluster_helper_c::set_duration(render_groups_t *rg) {
  uint32_t i;
  int64_t block_duration, def_duration;
  kax_block_blob_c *group;

  if (rg->durations.size() == 0)
    return;

  group = rg->groups.back();
  block_duration = 0;
  for (i = 0; i < rg->durations.size(); i++)
    block_duration += rg->durations[i];
  def_duration = rg->source->get_track_default_duration();
  mxverb(3, "cluster_helper::set_duration: block_duration " LLD " "
         "rounded duration " LLD " def_duration "
         "" LLD " use_durations %d rg->duration_mandatory %d\n",
         block_duration, RND_TIMECODE_SCALE(block_duration), def_duration,
         use_durations ? 1 : 0, rg->duration_mandatory ? 1 : 0);

  if (rg->duration_mandatory) {
    if ((block_duration == 0) ||
        ((block_duration > 0) &&
         (block_duration != (rg->durations.size() * def_duration))))
      group->set_block_duration(RND_TIMECODE_SCALE(block_duration));
  } else if ((use_durations || (def_duration > 0)) &&
             (block_duration > 0) &&
             (RND_TIMECODE_SCALE(block_duration) !=
              RND_TIMECODE_SCALE(rg->durations.size() * def_duration)))
    group->set_block_duration(RND_TIMECODE_SCALE(block_duration));
}

bool
cluster_helper_c::must_duration_be_set(render_groups_t *rg,
                                       packet_cptr &new_packet) {
  uint32_t i;
  int64_t block_duration, def_duration;

  block_duration = 0;
  for (i = 0; i < rg->durations.size(); i++)
    block_duration += rg->durations[i];
  block_duration += new_packet->duration;
  def_duration = rg->source->get_track_default_duration();

  if (rg->duration_mandatory || new_packet->duration_mandatory) {
    if ((block_duration == 0) ||
        ((block_duration > 0) &&
         (block_duration != ((rg->durations.size() + 1) * def_duration))))
      return true;
  } else if ((use_durations || (def_duration > 0)) &&
             (block_duration > 0) &&
             (RND_TIMECODE_SCALE(block_duration) !=
              RND_TIMECODE_SCALE((rg->durations.size() + 1) * def_duration)))
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
  kax_block_blob_c *new_block_group, *last_block_group;
  DataBuffer *data_buffer;
  int i, k, elements_in_cluster;
  packet_cptr pack;
  int64_t max_cl_timecode;
  generic_packetizer_c *source;
  vector<render_groups_t *> render_groups;
  render_groups_t *render_group;
  bool added_to_cues, has_codec_state;
  LacingType lacing_type;
  bool use_simpleblock = hack_engaged(ENGAGE_USE_SIMPLE_BLOCK);
  BlockBlobType this_block_blob_type, std_block_blob_type = use_simpleblock ?
    BLOCK_BLOB_ALWAYS_SIMPLE : BLOCK_BLOB_NO_SIMPLE;

  max_cl_timecode = 0;

  // Splitpoint stuff
  if ((header_overhead == -1) && splitting())
    header_overhead = out->getFilePointer() + tags_size;

  elements_in_cluster = 0;
  last_block_group = NULL;
  added_to_cues = false;

  if (hack_engaged(ENGAGE_LACING_XIPH))
    lacing_type = LACING_XIPH;
  else if (hack_engaged(ENGAGE_LACING_EBML))
    lacing_type = LACING_EBML;
  else
    lacing_type = LACING_AUTO;

  for (i = 0; i < packets.size(); i++) {
    pack = packets[i];
    source = pack->source;

    has_codec_state = NULL != pack->codec_state.get();

    if (source->contains_gap())
      cluster->SetSilentTrackUsed();

    render_group = NULL;
    for (k = 0; k < render_groups.size(); k++)
      if (render_groups[k]->source == source) {
        render_group = render_groups[k];
        break;
      }
    if (NULL == render_group) {
      render_group = new render_groups_t;
      render_group->source = source;
      render_group->more_data = false;
      render_group->duration_mandatory = false;
      render_groups.push_back(render_group);
    }

    if (i == 0)
      cluster->set_min_timecode(pack->assigned_timecode - timecode_offset);
    max_cl_timecode = pack->assigned_timecode;

    data_buffer = new DataBuffer((binary *)pack->data->get(),
                                 pack->data->get_size());

    KaxTrackEntry &track_entry =
      static_cast<KaxTrackEntry &>(*source->get_track_entry());

    if (render_group->groups.size() > 0)
      last_block_group = render_group->groups.back();
    else
      last_block_group = NULL;

    if ((pack->bref != -1) || has_codec_state)
      render_group->more_data = false;

    if (!render_group->more_data) {
      set_duration(render_group);
      render_group->durations.clear();
      render_group->duration_mandatory = false;

      this_block_blob_type = !use_simpleblock ? std_block_blob_type :
        must_duration_be_set(render_group, pack) ?
        BLOCK_BLOB_NO_SIMPLE : BLOCK_BLOB_ALWAYS_SIMPLE;

      if (has_codec_state)
        this_block_blob_type = BLOCK_BLOB_NO_SIMPLE;

      new_block_group = new kax_block_blob_c(this_block_blob_type);
      cluster->AddBlockBlob(new_block_group);
      new_block_group->SetParent(*cluster);
      render_group->groups.push_back(new_block_group);
      added_to_cues = false;
    } else
      new_block_group = last_block_group;

    // Now put the packet into the cluster.
    render_group->more_data =
      new_block_group->add_frame_auto(track_entry,
                                      pack->assigned_timecode -
                                      timecode_offset,
                                      *data_buffer, lacing_type,
                                      pack->bref - timecode_offset,
                                      pack->fref - timecode_offset);

    if (has_codec_state) {
      KaxBlockGroup &bgroup((KaxBlockGroup &)*new_block_group);
      KaxCodecState *cstate = new KaxCodecState;
      bgroup.PushElement(*cstate);
      cstate->CopyBuffer(pack->codec_state->get(),
                         pack->codec_state->get_size());
    }

    if (first_timecode_in_file == -1)
      first_timecode_in_file = pack->assigned_timecode;

    if ((pack->assigned_timecode + pack->duration) > max_timecode_and_duration)
      max_timecode_and_duration = pack->assigned_timecode + pack->duration;

    if ((pack->bref != -1) || (pack->fref != -1) ||
        !track_entry.LacingEnabled())
      render_group->more_data = false;

    render_group->durations.push_back(pack->unmodified_duration);
    render_group->duration_mandatory |= pack->duration_mandatory;

    if (new_block_group != NULL) {
      // Set the reference priority if it was wanted.
      if ((pack->ref_priority > 0) &&
          new_block_group->replace_simple_by_group())
        *static_cast<EbmlUInteger *>
          (&GetChild<KaxReferencePriority>(*new_block_group)) =
          pack->ref_priority;

      // Handle BlockAdditions if needed
      if (!pack->data_adds.empty() &&
          new_block_group->ReplaceSimpleByGroup()) {
        KaxBlockAdditions &additions =
          AddEmptyChild<KaxBlockAdditions>(*new_block_group);
        for (k = 0; k < pack->data_adds.size(); k++) {
          KaxBlockMore &block_more = AddEmptyChild<KaxBlockMore>(additions);
          *static_cast<EbmlUInteger *>(&GetChild<KaxBlockAddID>(block_more)) =
            k + 1;
          GetChild<KaxBlockAdditional>(block_more).
            CopyBuffer((binary *)pack->data_adds[k]->get(),
                       pack->data_adds[k]->get_size());
        }
      }
    }

    elements_in_cluster++;

    if (new_block_group == NULL)
      new_block_group = last_block_group;
    else if (write_cues && (!added_to_cues || has_codec_state)) {
      // Update the cues (index table) either if cue entries for
      // I frames were requested and this is an I frame...
      if (((source->get_cue_creation() == CUE_STRATEGY_IFRAMES) &&
           (pack->bref == -1)) ||
          // ... or if a codec state change is present ...
          has_codec_state ||
          // ... or if the user requested entries for all frames ...
          (source->get_cue_creation() == CUE_STRATEGY_ALL) ||
          // ... or if this is an audio track, there is no video track and the
          // last cue entry was created more than 2s ago.
          ((source->get_cue_creation() == CUE_STRATEGY_SPARSE) &&
           (source->get_track_type() == track_audio) &&
           (video_packetizer == NULL) &&
           ((source->get_last_cue_timecode() < 0) ||
            ((pack->assigned_timecode - source->get_last_cue_timecode()) >=
             2000000000)))) {
        kax_cues->AddBlockBlob(*new_block_group);
        num_cue_elements++;
        cue_writing_requested = 1;
        source->set_last_cue_timecode(pack->assigned_timecode);
        added_to_cues = true;

        if (has_codec_state) {
        }
      }
    }

    pack->group = new_block_group;
    last_block_group = new_block_group;

  }

  if (elements_in_cluster > 0) {
    for (i = 0; i < render_groups.size(); i++)
      set_duration(render_groups[i]);
    cluster->set_max_timecode(max_cl_timecode - timecode_offset);
    cluster->Render(*out, *kax_cues);
    bytes_in_file += cluster->ElementSize();

    if (kax_sh_cues != NULL)
      kax_sh_cues->IndexThis(*cluster, *kax_segment);

    last_cluster_tc = cluster->GlobalTimecode();
  } else
    last_cluster_tc = 0;

  for (i = 0; i < render_groups.size(); i++)
    delete render_groups[i];

  min_timecode_in_cluster = -1;
  max_timecode_in_cluster = -1;

  return 1;
}

int64_t
cluster_helper_c::get_duration() {
  mxverb(3, "cluster_helper_c::get_duration(): " LLD " - " LLD " = " LLD "\n",
         max_timecode_and_duration, first_timecode_in_file,
         max_timecode_and_duration - first_timecode_in_file);
  return max_timecode_and_duration - first_timecode_in_file;
}

void
cluster_helper_c::add_split_point(const split_point_t &split_point) {
  split_points.push_back(split_point);
  current_split_point = split_points.begin();
}
