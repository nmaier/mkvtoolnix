/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   functions common for all readers/packetizers

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Modified by Steve Lhomme <steve.lhomme@free.fr>.
*/

#include "common/os.h"

#include <algorithm>
#include <map>
#include <string>

#include <matroska/KaxContentEncoding.h>
#include <matroska/KaxTag.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>

#include "common/base64.h"
#include "common/common.h"
#include "common/compression.h"
#include "common/ebml.h"
#include "common/hacks.h"
#include "common/math.h"
#include "common/string_formatting.h"
#include "common/tag_common.h"
#include "common/tagparser.h"
#include "common/unique_numbers.h"
#include "merge/mkvmerge.h"
#include "merge/output_control.h"
#include "merge/pr_generic.h"

using namespace std;

#define TRACK_TYPE_TO_DEFTRACK_TYPE(track_type)      \
  (  track_audio == track_type ? DEFTRACK_TYPE_AUDIO \
   : track_video == track_type ? DEFTRACK_TYPE_VIDEO \
   :                             DEFTRACK_TYPE_SUBS)

int64_t packet_t::sm_packet_number_counter = 0;

packet_t::~packet_t() {
}

// ---------------------------------------------------------------------

vector<generic_packetizer_c *> ptzrs_in_header_order;

generic_packetizer_c::generic_packetizer_c(generic_reader_c *p_reader,
                                           track_info_c &p_ti)
  throw(error_c)
  : next_packet_wo_assigned_timecode(0)
  , m_free_refs(-1)
  , m_next_free_refs(-1)
  , enqueued_bytes(0)
  , safety_last_timecode(0)
  , safety_last_duration(0)
  , track_entry(NULL)
  , hserialno(-1)
  , htrack_type(-1)
  , htrack_min_cache(0)
  , htrack_max_cache(-1)
  , htrack_default_duration(-1)
  , default_duration_forced(true)
  , default_track_warning_printed(false)
  , huid(0)
  , htrack_max_add_block_ids(-1)
  , hcodec_private(NULL)
  , hcodec_private_length(0)
  , haudio_sampling_freq(-1.0)
  , haudio_output_sampling_freq(-1.0)
  , haudio_channels(-1)
  , haudio_bit_depth(-1)
  , hvideo_pixel_width(-1)
  , hvideo_pixel_height(-1)
  , hvideo_display_width(-1)
  , hvideo_display_height(-1)
  , hcompression(COMPRESSION_UNSPECIFIED)
  , timecode_factory_application_mode(TFA_AUTOMATIC)
  , last_cue_timecode(-1)
  , has_been_flushed(false)
  , ti(p_ti)
  , reader(p_reader)
  , connected_to(0)
  , correction_timecode_offset(0)
  , append_timecode_offset(0)
  , max_timecode_seen(0)
  , relaxed_timecode_checking(false)
{
  // Let's see if the user specified timecode sync for this track.
  if (map_has_key(ti.timecode_syncs, ti.id))
    ti.tcsync = ti.timecode_syncs[ti.id];
  else if (map_has_key(ti.timecode_syncs, -1))
    ti.tcsync = ti.timecode_syncs[-1];
  if (0 == ti.tcsync.numerator)
    ti.tcsync.numerator = 1;
  if (0 == ti.tcsync.denominator)
    ti.tcsync.denominator = 1;

  // Let's see if the user has specified which cues he wants for this track.
  if (map_has_key(ti.cue_creations, ti.id))
    ti.cues = ti.cue_creations[ti.id];
  else if (map_has_key(ti.cue_creations, -1))
    ti.cues = ti.cue_creations[-1];

  // Let's see if the user has given a default track flag for this track.
  if (map_has_key(ti.default_track_flags, ti.id))
    ti.default_track = ti.default_track_flags[ti.id];
  else if (map_has_key(ti.default_track_flags, -1))
    ti.default_track = ti.default_track_flags[-1];

  // Let's see if the user has given a forced track flag for this track.
  if (map_has_key(ti.forced_track_flags, ti.id))
    ti.forced_track = ti.forced_track_flags[ti.id];
  else if (map_has_key(ti.forced_track_flags, -1))
    ti.forced_track = ti.forced_track_flags[-1];

  // Let's see if the user has specified a language for this track.
  if (map_has_key(ti.languages, ti.id))
    ti.language = ti.languages[ti.id];
  else if (map_has_key(ti.languages, -1))
    ti.language = ti.languages[-1];

  // Let's see if the user has specified a sub charset for this track.
  if (map_has_key(ti.sub_charsets, ti.id))
    ti.sub_charset = ti.sub_charsets[ti.id];
  else if (map_has_key(ti.sub_charsets, -1))
    ti.sub_charset = ti.sub_charsets[-1];

  // Let's see if the user has specified a sub charset for this track.
  if (map_has_key(ti.all_tags, ti.id))
    ti.tags_file_name = ti.all_tags[ti.id];
  else if (map_has_key(ti.all_tags, -1))
    ti.tags_file_name = ti.all_tags[-1];
  if (ti.tags_file_name != "") {
    ti.tags = new KaxTags;
    parse_xml_tags(ti.tags_file_name, ti.tags);
  }

  // Let's see if the user has specified how this track should be compressed.
  if (map_has_key(ti.compression_list, ti.id))
    ti.compression = ti.compression_list[ti.id];
  else if (map_has_key(ti.compression_list, -1))
    ti.compression = ti.compression_list[-1];

  // Let's see if the user has specified a name for this track.
  if (map_has_key(ti.track_names, ti.id))
    ti.track_name = ti.track_names[ti.id];
  else if (map_has_key(ti.track_names, -1))
    ti.track_name = ti.track_names[-1];

  // Let's see if the user has specified external timecodes for this track.
  if (map_has_key(ti.all_ext_timecodes, ti.id))
    ti.ext_timecodes = ti.all_ext_timecodes[ti.id];
  else if (map_has_key(ti.all_ext_timecodes, -1))
    ti.ext_timecodes = ti.all_ext_timecodes[-1];

  // Let's see if the user has specified an aspect ratio or display dimensions
  // for this track.
  int i = -2;
  if (map_has_key(ti.display_properties, ti.id))
    i = ti.id;
  else if (map_has_key(ti.display_properties, -1))
    i = -1;
  if (-2 != i) {
    display_properties_t &dprop = ti.display_properties[i];
    if (0 > dprop.aspect_ratio) {
      ti.display_width            = dprop.width;
      ti.display_height           = dprop.height;
      ti.display_dimensions_given = true;
    } else {
      ti.aspect_ratio             = dprop.aspect_ratio;
      ti.aspect_ratio_given       = true;
      ti.aspect_ratio_is_factor   = dprop.ar_factor;
    }
  }
  if (ti.aspect_ratio_given && ti.display_dimensions_given) {
    if (ti.aspect_ratio_is_factor)
      mxerror_tid(ti.fname, ti.id, boost::format(Y("Both the aspect ratio factor and '--display-dimensions' were given.\n")));
    else
      mxerror_tid(ti.fname, ti.id, boost::format(Y("Both the aspect ratio and '--display-dimensions' were given.\n")));
  }

  // Let's see if the user has specified a FourCC for this track.
  if (map_has_key(ti.all_fourccs, ti.id))
    ti.fourcc = ti.all_fourccs[ti.id];
  else if (map_has_key(ti.all_fourccs, -1))
    ti.fourcc = ti.all_fourccs[-1];

  // Let's see if the user has specified a FourCC for this track.
  ti.pixel_cropping_specified = true;
  if (map_has_key(ti.pixel_crop_list, ti.id))
    ti.pixel_cropping = ti.pixel_crop_list[ti.id];
  else if (map_has_key(ti.pixel_crop_list, -1))
    ti.pixel_cropping = ti.pixel_crop_list[-1];
  else
    ti.pixel_cropping_specified = false;

  // Let's see if the user has specified a stereo mode for this track.
  if (map_has_key(ti.stereo_mode_list, ti.id))
    ti.stereo_mode = ti.stereo_mode_list[ti.id];
  else if (map_has_key(ti.stereo_mode_list, -1))
    ti.stereo_mode = ti.stereo_mode_list[-1];

  // Let's see if the user has specified a default duration for this track.
  if (map_has_key(ti.default_durations, ti.id))
    htrack_default_duration = ti.default_durations[ti.id];
  else if (map_has_key(ti.default_durations, -1))
    htrack_default_duration = ti.default_durations[-1];
  else
    default_duration_forced = false;

  // Let's see if the user has set a max_block_add_id
  if (map_has_key(ti.max_blockadd_ids, ti.id))
    htrack_max_add_block_ids = ti.max_blockadd_ids[ti.id];
  else if (map_has_key(ti.max_blockadd_ids, -1))
    htrack_max_add_block_ids = ti.max_blockadd_ids[-1];

  // Let's see if the user has specified a NALU size length for this track.
  if (map_has_key(ti.nalu_size_lengths, ti.id))
    ti.nalu_size_length = ti.nalu_size_lengths[ti.id];
  else if (map_has_key(ti.nalu_size_lengths, -1))
    ti.nalu_size_length = ti.nalu_size_lengths[-1];

  // Set default header values to 'unset'.
  if (!reader->appending)
    hserialno = create_track_number(reader, ti.id);

  timecode_factory = timecode_factory_c::create(ti.ext_timecodes, ti.fname, ti.id);
}

generic_packetizer_c::~generic_packetizer_c() {
  safefree(hcodec_private);
  if (!packet_queue.empty())
    mxerror_tid(ti.fname, ti.id, boost::format(Y("Packet queue not empty (flushed: %1%). Frames have been lost during remux. %2%\n")) % has_been_flushed % BUGMSG);
}

void
generic_packetizer_c::set_tag_track_uid() {
  if (NULL == ti.tags)
    return;

  convert_old_tags(*ti.tags);
  int idx_tags;
  for (idx_tags = 0; ti.tags->ListSize() > idx_tags; ++idx_tags) {
    KaxTag *tag = (KaxTag *)(*ti.tags)[idx_tags];

    int idx_tag;
    for (idx_tag = 0; tag->ListSize() > idx_tag; idx_tag++) {
      EbmlElement *el = (*tag)[idx_tag];

      if (!is_id(el, KaxTagTargets))
        continue;

      KaxTagTargets *targets = static_cast<KaxTagTargets *>(el);
      int idx_target         = 0;

      while (targets->ListSize() > idx_target) {
        EbmlElement *uid_el = (*targets)[idx_target];
        if (is_id(uid_el, KaxTagTrackUID)) {
          targets->Remove(idx_target);
          delete uid_el;

        } else
          ++idx_target;
      }
    }

    GetChildAs<KaxTagTrackUID, EbmlUInteger>(GetChild<KaxTagTargets>(tag)) = huid;

    fix_mandatory_tag_elements(tag);

    if (!tag->CheckMandatory())
      mxerror(boost::format(Y("The tags in '%1%' could not be parsed: some mandatory elements are missing.\n"))
              % (ti.tags_file_name != "" ? ti.tags_file_name : ti.fname));
  }
}

int
generic_packetizer_c::set_uid(uint32_t uid) {
  if (is_unique_uint32(uid, UNIQUE_TRACK_IDS)) {
    add_unique_uint32(uid, UNIQUE_TRACK_IDS);
    huid = uid;
    if (NULL != track_entry)
      GetChildAs<KaxTrackUID, EbmlUInteger>(track_entry) = huid;

    return 1;
  }

  return 0;
}

void
generic_packetizer_c::set_track_type(int type,
                                     timecode_factory_application_e tfa_mode) {
  htrack_type = type;

  if ((track_audio == type) && (CUE_STRATEGY_UNSPECIFIED == ti.cues))
    ti.cues = CUE_STRATEGY_SPARSE;

  if (track_audio == type)
    reader->num_audio_tracks++;

  else if (track_video == type) {
    reader->num_video_tracks++;
    if (NULL == g_video_packetizer)
      g_video_packetizer = this;

  } else
    reader->num_subtitle_tracks++;

  if (   (TFA_AUTOMATIC == tfa_mode)
      && (TFA_AUTOMATIC == timecode_factory_application_mode))
    timecode_factory_application_mode
      = (track_video    == type) ? TFA_FULL_QUEUEING
      : (track_subtitle == type) ? TFA_IMMEDIATE
      : (track_buttons  == type) ? TFA_IMMEDIATE
      :                            TFA_FULL_QUEUEING;

  else if (TFA_AUTOMATIC != tfa_mode)
    timecode_factory_application_mode = tfa_mode;

  if ((NULL != timecode_factory.get()) && (track_video != type) && (track_audio != type))
    timecode_factory->set_preserve_duration(true);
}

void
generic_packetizer_c::set_track_name(const string &name) {
  ti.track_name = name;
  if ((NULL != track_entry) && !name.empty())
    GetChildAs<KaxTrackName, EbmlUnicodeString>(track_entry) = cstrutf8_to_UTFstring(ti.track_name);
}

void
generic_packetizer_c::set_codec_id(const string &id) {
  hcodec_id = id;
  if ((NULL != track_entry) && !id.empty())
    GetChildAs<KaxCodecID, EbmlString>(track_entry) = hcodec_id;
}

void
generic_packetizer_c::set_codec_private(const unsigned char *cp,
                                        int length) {
  safefree(hcodec_private);

  if (NULL == cp) {
    hcodec_private        = NULL;
    hcodec_private_length = 0;
    return;
  }

  hcodec_private        = (unsigned char *)safememdup(cp, length);
  hcodec_private_length = length;

  if (NULL != track_entry)
    GetChild<KaxCodecPrivate>(*track_entry).CopyBuffer((binary *)hcodec_private, hcodec_private_length);
}

void
generic_packetizer_c::set_track_min_cache(int min_cache) {
  htrack_min_cache = min_cache;
  if (NULL != track_entry)
    GetChildAs<KaxTrackMinCache, EbmlUInteger>(track_entry) = min_cache;
}

void
generic_packetizer_c::set_track_max_cache(int max_cache) {
  htrack_max_cache = max_cache;
  if (NULL != track_entry)
    GetChildAs<KaxTrackMaxCache, EbmlUInteger>(track_entry) = max_cache;
}

void
generic_packetizer_c::set_track_default_duration(int64_t def_dur) {
  if (default_duration_forced)
    return;

  htrack_default_duration = (int64_t)(def_dur * ti.tcsync.numerator / ti.tcsync.denominator);

  if (NULL != track_entry)
    GetChildAs<KaxTrackDefaultDuration, EbmlUInteger>(track_entry) = htrack_default_duration;
}

void
generic_packetizer_c::set_track_max_additionals(int max_add_block_ids) {
  htrack_max_add_block_ids = max_add_block_ids;
  if (NULL != track_entry)
    GetChildAs<KaxMaxBlockAdditionID, EbmlUInteger>(track_entry) = max_add_block_ids;
}

int64_t
generic_packetizer_c::get_track_default_duration() {
  return htrack_default_duration;
}

void
generic_packetizer_c::set_track_forced_flag(bool forced_track) {
  ti.forced_track = forced_track;
  if (NULL != track_entry)
    GetChildAs<KaxTrackFlagForced, EbmlUInteger>(track_entry) = forced_track ? 1 : 0;
}

void
generic_packetizer_c::set_audio_sampling_freq(float freq) {
  haudio_sampling_freq = freq;
  if (NULL != track_entry)
    GetChildAs<KaxAudioSamplingFreq, EbmlFloat>(GetChild<KaxTrackAudio>(track_entry)) = haudio_sampling_freq;
}

void
generic_packetizer_c::set_audio_output_sampling_freq(float freq) {
  haudio_output_sampling_freq = freq;
  if (NULL != track_entry)
    GetChildAs<KaxAudioOutputSamplingFreq, EbmlFloat>(GetChild<KaxTrackAudio>(track_entry)) = haudio_output_sampling_freq;
}

void
generic_packetizer_c::set_audio_channels(int channels) {
  haudio_channels = channels;
  if (NULL != track_entry)
    GetChildAs<KaxAudioChannels, EbmlUInteger>(GetChild<KaxTrackAudio>(*track_entry)) = haudio_channels;
}

void
generic_packetizer_c::set_audio_bit_depth(int bit_depth) {
  haudio_bit_depth = bit_depth;
  if (NULL != track_entry)
    GetChildAs<KaxAudioBitDepth, EbmlUInteger>(GetChild<KaxTrackAudio>(*track_entry)) = haudio_bit_depth;
}

void
generic_packetizer_c::set_video_pixel_width(int width) {
  hvideo_pixel_width = width;
  if (NULL != track_entry)
    GetChildAs<KaxVideoPixelWidth, EbmlUInteger>(GetChild<KaxTrackVideo>(*track_entry)) = hvideo_pixel_width;
}

void
generic_packetizer_c::set_video_pixel_height(int height) {
  hvideo_pixel_height = height;
  if (NULL != track_entry)
    GetChildAs<KaxVideoPixelHeight, EbmlUInteger>(GetChild<KaxTrackVideo>(*track_entry)) = hvideo_pixel_height;
}

void
generic_packetizer_c::set_video_display_width(int width) {
  hvideo_display_width = width;
  if (NULL != track_entry)
    GetChildAs<KaxVideoDisplayWidth, EbmlUInteger>(GetChild<KaxTrackVideo>(*track_entry)) = hvideo_display_width;
}

void
generic_packetizer_c::set_video_display_height(int height) {
  hvideo_display_height = height;
  if (NULL != track_entry)
    GetChildAs<KaxVideoDisplayHeight, EbmlUInteger>(GetChild<KaxTrackVideo>(*track_entry)) = hvideo_display_height;
}

void
generic_packetizer_c::set_as_default_track(int type,
                                           int priority) {
  if (g_default_tracks_priority[type] < priority) {
    g_default_tracks_priority[type] = priority;
    g_default_tracks[type]          = hserialno;

  } else if (   (DEFAULT_TRACK_PRIORITY_CMDLINE == priority)
             && (g_default_tracks[type] != hserialno)
             && !default_track_warning_printed) {
    mxwarn(boost::format(Y("Another default track for %1% tracks has already been set. The 'default' flag for track %2% of '%3%' will not be set.\n"))
           % (DEFTRACK_TYPE_AUDIO == type ? "audio" : DEFTRACK_TYPE_VIDEO == type ? "video" : "subtitle") % ti.id % ti.fname);
    default_track_warning_printed = true;
  }
}

void
generic_packetizer_c::set_language(const string &language) {
  ti.language = language;
  if (NULL != track_entry)
    GetChildAs<KaxTrackLanguage, EbmlString>(track_entry) = ti.language;
}

void
generic_packetizer_c::set_video_pixel_cropping(int left,
                                               int top,
                                               int right,
                                               int bottom) {
  ti.pixel_cropping.left   = left;
  ti.pixel_cropping.top    = top;
  ti.pixel_cropping.right  = right;
  ti.pixel_cropping.bottom = bottom;

  if (NULL != track_entry) {
    KaxTrackVideo &video = GetChild<KaxTrackVideo>(track_entry);

    GetChildAs<KaxVideoPixelCropLeft,   EbmlUInteger>(video) = ti.pixel_cropping.left;
    GetChildAs<KaxVideoPixelCropTop,    EbmlUInteger>(video) = ti.pixel_cropping.top;
    GetChildAs<KaxVideoPixelCropRight,  EbmlUInteger>(video) = ti.pixel_cropping.right;
    GetChildAs<KaxVideoPixelCropBottom, EbmlUInteger>(video) = ti.pixel_cropping.bottom;
  }
}

void
generic_packetizer_c::set_stereo_mode(stereo_mode_e stereo_mode) {
  ti.stereo_mode = stereo_mode;

  if ((NULL != track_entry) && (STEREO_MODE_UNSPECIFIED != stereo_mode))
    GetChildAs<KaxVideoStereoMode, EbmlUInteger>(GetChild<KaxTrackVideo>(*track_entry)) = ti.stereo_mode;
}

void
generic_packetizer_c::set_headers() {
  if (0 < connected_to)
    return;

  bool found = false;
  int idx;
  for (idx = 0; ptzrs_in_header_order.size() > idx; ++idx)
    if (this == ptzrs_in_header_order[idx]) {
      found = true;
      break;
    }

  if (!found)
    ptzrs_in_header_order.push_back(this);

  if (NULL == track_entry) {
    track_entry      = NULL == g_kax_last_entry ? &GetChild<KaxTrackEntry>(g_kax_tracks) : &GetNextChild<KaxTrackEntry>(g_kax_tracks, *g_kax_last_entry);
    g_kax_last_entry = track_entry;
    track_entry->SetGlobalTimecodeScale((int64_t)g_timecode_scale);
  }

  GetChildAs<KaxTrackNumber, EbmlUInteger>(track_entry) = hserialno;

  if (0 == huid)
    huid = create_unique_uint32(UNIQUE_TRACK_IDS);

  GetChildAs<KaxTrackUID, EbmlUInteger>(track_entry)    = huid;

  if (-1 != htrack_type)
    GetChildAs<KaxTrackType, EbmlUInteger>(track_entry) = htrack_type;

  if (!hcodec_id.empty())
    GetChildAs<KaxCodecID, EbmlString>(track_entry)     = hcodec_id;

  if (NULL != hcodec_private)
    GetChild<KaxCodecPrivate>(*track_entry).CopyBuffer((binary *)hcodec_private, hcodec_private_length);

  if (-1 != htrack_min_cache)
    GetChildAs<KaxTrackMinCache, EbmlUInteger>(track_entry)      = htrack_min_cache;

  if (-1 != htrack_max_cache)
    GetChildAs<KaxTrackMaxCache, EbmlUInteger>(track_entry)      = htrack_max_cache;

  if (-1 != htrack_max_add_block_ids)
    GetChildAs<KaxMaxBlockAdditionID, EbmlUInteger>(track_entry) = htrack_max_add_block_ids;

  if (NULL != timecode_factory.get())
    htrack_default_duration = (int64_t)timecode_factory->get_default_duration(htrack_default_duration);
  if (-1.0 != htrack_default_duration)
    GetChildAs<KaxTrackDefaultDuration, EbmlUInteger>(track_entry) = htrack_default_duration;

  idx = TRACK_TYPE_TO_DEFTRACK_TYPE(htrack_type);

  if (boost::logic::indeterminate(ti.default_track))
    set_as_default_track(idx, DEFAULT_TRACK_PRIORITY_FROM_TYPE);
  else if (ti.default_track)
    set_as_default_track(idx, DEFAULT_TRACK_PRIORITY_CMDLINE);
  else if (g_default_tracks[idx] == hserialno)
    g_default_tracks[idx] = 0;

  GetChildAs<KaxTrackLanguage, EbmlString>(track_entry) = ti.language != "" ? ti.language : g_default_language.c_str();

  if (!ti.track_name.empty())
    GetChildAs<KaxTrackName, EbmlUnicodeString>(track_entry) = cstrutf8_to_UTFstring(ti.track_name);

  if (!boost::logic::indeterminate(ti.forced_track))
    GetChildAs<KaxTrackFlagForced, EbmlUInteger>(track_entry) = ti.forced_track ? 1 : 0;

  if (track_video == htrack_type) {
    KaxTrackVideo &video = GetChild<KaxTrackVideo>(track_entry);

    if ((-1 != hvideo_pixel_height) && (-1 != hvideo_pixel_width)) {
      if ((-1 == hvideo_display_width) || (-1 == hvideo_display_height) || ti.aspect_ratio_given || ti.display_dimensions_given) {
        if (ti.display_dimensions_given) {
          hvideo_display_width  = ti.display_width;
          hvideo_display_height = ti.display_height;

        } else {
          if (!ti.aspect_ratio_given)
            ti.aspect_ratio = (float)hvideo_pixel_width                   / (float)hvideo_pixel_height;

          else if (ti.aspect_ratio_is_factor)
            ti.aspect_ratio = (float)hvideo_pixel_width * ti.aspect_ratio / (float)hvideo_pixel_height;

          if (ti.aspect_ratio > ((float)hvideo_pixel_width / (float)hvideo_pixel_height)) {
            hvideo_display_width  = irnd(hvideo_pixel_height * ti.aspect_ratio);
            hvideo_display_height = hvideo_pixel_height;

          } else {
            hvideo_display_width  = hvideo_pixel_width;
            hvideo_display_height = irnd(hvideo_pixel_width / ti.aspect_ratio);
          }
        }
      }

      GetChildAs<KaxVideoPixelWidth,    EbmlUInteger>(video) = hvideo_pixel_width;
      GetChildAs<KaxVideoPixelHeight,   EbmlUInteger>(video) = hvideo_pixel_height;

      GetChildAs<KaxVideoDisplayWidth,  EbmlUInteger>(video) = hvideo_display_width;
      GetChildAs<KaxVideoDisplayHeight, EbmlUInteger>(video) = hvideo_display_height;

      GetChild<KaxVideoDisplayWidth>(video).SetDefaultSize(4);
      GetChild<KaxVideoDisplayHeight>(video).SetDefaultSize(4);

      if (ti.pixel_cropping_specified) {
        GetChildAs<KaxVideoPixelCropLeft,   EbmlUInteger>(video) = ti.pixel_cropping.left;
        GetChildAs<KaxVideoPixelCropTop,    EbmlUInteger>(video) = ti.pixel_cropping.top;
        GetChildAs<KaxVideoPixelCropRight,  EbmlUInteger>(video) = ti.pixel_cropping.right;
        GetChildAs<KaxVideoPixelCropBottom, EbmlUInteger>(video) = ti.pixel_cropping.bottom;
      }

      if (STEREO_MODE_UNSPECIFIED != ti.stereo_mode)
        GetChildAs<KaxVideoStereoMode, EbmlUInteger>(video) = ti.stereo_mode;
    }

  } else if (track_audio == htrack_type) {
    KaxTrackAudio &audio = GetChild<KaxTrackAudio>(track_entry);

    if (-1   != haudio_sampling_freq)
      GetChildAs<KaxAudioSamplingFreq, EbmlFloat>(audio)       = haudio_sampling_freq;

    if (-1.0 != haudio_output_sampling_freq)
      GetChildAs<KaxAudioOutputSamplingFreq, EbmlFloat>(audio) = haudio_output_sampling_freq;

    if (-1   != haudio_channels)
      GetChildAs<KaxAudioChannels, EbmlUInteger>(audio)        = haudio_channels;

    if (-1   != haudio_bit_depth)
      GetChildAs<KaxAudioBitDepth, EbmlUInteger>(audio)        = haudio_bit_depth;

  } else if (track_buttons == htrack_type) {
    if ((-1 != hvideo_pixel_height) && (-1 != hvideo_pixel_width)) {
      KaxTrackVideo &video = GetChild<KaxTrackVideo>(track_entry);

      GetChildAs<KaxVideoPixelWidth,  EbmlUInteger>(video) = hvideo_pixel_width;
      GetChildAs<KaxVideoPixelHeight, EbmlUInteger>(video) = hvideo_pixel_height;
    }

  }

  if (COMPRESSION_UNSPECIFIED != ti.compression)
    hcompression = ti.compression;
  if ((COMPRESSION_UNSPECIFIED != hcompression) && (COMPRESSION_NONE != hcompression)) {
    KaxContentEncoding &c_encoding = GetChild<KaxContentEncoding>(GetChild<KaxContentEncodings>(track_entry));

    GetChildAs<KaxContentEncodingOrder, EbmlUInteger>(c_encoding) = 0; // First modification.
    GetChildAs<KaxContentEncodingType,  EbmlUInteger>(c_encoding) = 0; // It's a compression.
    GetChildAs<KaxContentEncodingScope, EbmlUInteger>(c_encoding) = 1; // Only the frame contents have been compresed.

    compressor = compressor_c::create(hcompression);
    compressor->set_track_headers(c_encoding);
  }

  if (g_no_lacing)
    track_entry->EnableLacing(false);

  set_tag_track_uid();
  if (NULL != ti.tags) {
    while (ti.tags->ListSize() != 0) {
      KaxTag *tag = (KaxTag *)(*ti.tags)[0];
      add_tags(tag);
      ti.tags->Remove(0);
    }
  }
}

void
generic_packetizer_c::fix_headers() {
  GetChildAs<KaxTrackFlagDefault, EbmlUInteger>(track_entry) = g_default_tracks[TRACK_TYPE_TO_DEFTRACK_TYPE(htrack_type)] == hserialno ? 1 : 0;

  track_entry->SetGlobalTimecodeScale((int64_t)g_timecode_scale);
}

void
generic_packetizer_c::add_packet(packet_cptr pack) {
  if (NULL == reader->ptzr_first_packet)
    reader->ptzr_first_packet = this;

  // strip elements to be removed
  if (   (-1                     != htrack_max_add_block_ids)
      && (pack->data_adds.size()  > htrack_max_add_block_ids))
    pack->data_adds.resize(htrack_max_add_block_ids);

  if (NULL != compressor.get()) {
    try {
      compressor->compress(pack->data);
      int i;
      for (i = 0; pack->data_adds.size() > i; ++i)
        compressor->compress(pack->data_adds[i]);

    } catch (compression_error_c &e) {
      mxerror_tid(ti.fname, ti.id, boost::format(Y("Compression failed: %1%\n")) % e.get_error());
    }

  } else {
    pack->data->grab();
    int i;
    for (i = 0; i < pack->data_adds.size(); i++)
      pack->data_adds[i]->grab();
  }
  pack->source = this;

  enqueued_bytes += pack->data->get_size();

  if ((0 > pack->bref) && (0 <= pack->fref)) {
    int64_t tmp = pack->bref;
    pack->bref  = pack->fref;
    pack->fref  = tmp;
  }

  if (1 != connected_to)
    add_packet2(pack);
  else
    deferred_packets.push_back(pack);
}

#define ADJUST_TIMECODE(x) (int64_t)((x + correction_timecode_offset + append_timecode_offset) * ti.tcsync.numerator / ti.tcsync.denominator) + ti.tcsync.displacement

void
generic_packetizer_c::add_packet2(packet_cptr pack) {
  pack->timecode   = ADJUST_TIMECODE(pack->timecode);
  if (0 <= pack->bref)
    pack->bref     = ADJUST_TIMECODE(pack->bref);
  if (0 <= pack->fref)
    pack->fref     = ADJUST_TIMECODE(pack->fref);
  if (0 < pack->duration)
    pack->duration = (int64_t)(pack->duration * ti.tcsync.numerator / ti.tcsync.denominator);

  if ((2 > htrack_min_cache) && (0 <= pack->fref)) {
    set_track_min_cache(2);
    rerender_track_headers();

  } else if ((1 > htrack_min_cache) && (0 <= pack->bref)) {
    set_track_min_cache(1);
    rerender_track_headers();
  }

  if (0 > pack->timecode)
    return;

  // 'timecode < safety_last_timecode' may only occur for B frames. In this
  // case we have the coding order, e.g. IPB1B2 and the timecodes
  // I: 0, P: 120, B1: 40, B2: 80.
  if (!relaxed_timecode_checking && (pack->timecode < safety_last_timecode) && (0 > pack->fref)) {
    if (track_audio == htrack_type) {
      int64_t needed_timecode_offset  = safety_last_timecode + safety_last_duration - pack->timecode;
      correction_timecode_offset     += needed_timecode_offset;
      pack->timecode                 += needed_timecode_offset;
      if (0 <= pack->bref)
        pack->bref += needed_timecode_offset;
      if (0 <= pack->fref)
        pack->fref += needed_timecode_offset;

      mxwarn_tid(ti.fname, ti.id,
                 boost::format(Y("The current packet's timecode is smaller than that of the previous packet. "
                                 "This usually means that the source file is a Matroska file that has not been created 100%% correctly. "
                                 "The timecodes of all packets will be adjusted by %1%ms in order not to lose any data. "
                                 "This may throw audio/video synchronization off, but that can be corrected with mkvmerge's \"--sync\" option. "
                                 "If you already use \"--sync\" and you still get this warning then do NOT worry -- this is normal. "
                                 "If this error happens more than once and you get this message more than once for a particular track "
                                 "then either is the source file badly mastered, or mkvmerge contains a bug. "
                                 "In this case you should contact the author Moritz Bunkus <moritz@bunkus.org>.\n"))
                 % ((needed_timecode_offset + 500000) / 1000000));

    } else if (hack_engaged(ENGAGE_ENABLE_TIMECODE_WARNING))
      mxwarn_tid(ti.fname, ti.id,
                 boost::format(Y("pr_generic.cpp/generic_packetizer_c::add_packet(): timecode < last_timecode (%1% < %2%). %3%\n"))
                 % format_timecode(pack->timecode) % format_timecode(safety_last_timecode) % BUGMSG);
  }

  safety_last_timecode          = pack->timecode;
  safety_last_duration          = pack->duration;
  pack->timecode_before_factory = pack->timecode;

  packet_queue.push_back(pack);
  if ((NULL == timecode_factory.get()) || (TFA_IMMEDIATE == timecode_factory_application_mode))
    apply_factory_once(pack);
  else
    apply_factory();
}

void
generic_packetizer_c::process_deferred_packets() {
  deque<packet_cptr>::iterator packet;

  mxforeach(packet, deferred_packets)
    add_packet2(*packet);
  deferred_packets.clear();
}

packet_cptr
generic_packetizer_c::get_packet() {
  if (packet_queue.empty() || !packet_queue.front()->factory_applied)
    return packet_cptr(NULL);

  packet_cptr pack = packet_queue.front();
  packet_queue.pop_front();

  enqueued_bytes -= pack->data->get_size();

  --next_packet_wo_assigned_timecode;
  if (0 > next_packet_wo_assigned_timecode)
    next_packet_wo_assigned_timecode = 0;

  return pack;
}

void
generic_packetizer_c::apply_factory_once(packet_cptr &packet) {
  if (NULL == timecode_factory.get()) {
    packet->assigned_timecode = packet->timecode;
    packet->gap_following     = false;
  } else
    packet->gap_following     = timecode_factory->get_next(packet);

  packet->factory_applied     = true;

  mxverb(4,
         boost::format("apply_factory_once(): source %1% t %2% tbf %3% at %4%\n")
         % packet->source->get_source_track_num() % packet->timecode % packet->timecode_before_factory % packet->assigned_timecode);

  max_timecode_seen         = std::max(max_timecode_seen, packet->assigned_timecode + packet->duration);
  reader->max_timecode_seen = std::max(max_timecode_seen, reader->max_timecode_seen);

  ++next_packet_wo_assigned_timecode;
}

void
generic_packetizer_c::apply_factory() {
  if (packet_queue.empty())
    return;

  // Find the first packet to which the factory hasn't been applied yet.
  packet_cptr_di p_start = packet_queue.begin() + next_packet_wo_assigned_timecode;

  while ((packet_queue.end() != p_start) && (*p_start)->factory_applied)
    ++p_start;

  if (packet_queue.end() == p_start)
    return;

  if (TFA_SHORT_QUEUEING == timecode_factory_application_mode)
    apply_factory_short_queueing(p_start);

  else
    apply_factory_full_queueing(p_start);
}

void
generic_packetizer_c::apply_factory_short_queueing(packet_cptr_di &p_start) {
  while (packet_queue.end() != p_start) {
    // Find the next packet with a timecode bigger than the start packet's
    // timecode. All packets between those two including the start packet
    // and excluding the end packet can be timestamped.
    packet_cptr_di p_end = p_start + 1;
    while ((packet_queue.end() != p_end) && ((*p_end)->timecode_before_factory < (*p_start)->timecode_before_factory))
      ++p_end;

    // Abort if no such packet was found, but keep on assigning if the
    // packetizer has been flushed already.
    if (!has_been_flushed && (packet_queue.end() == p_end))
      return;

    // Now assign timecodes to the ones between p_start and p_end...
    packet_cptr_di p_current;
    for (p_current = p_start + 1; p_current != p_end; ++p_current)
      apply_factory_once(*p_current);
    // ...and to p_start itself.
    apply_factory_once(*p_start);

    p_start = p_end;
  }
}

struct packet_sorter_t {
  int m_index;
  static deque<packet_cptr> *m_packet_queue;

  packet_sorter_t(int index)
    : m_index(index)
  {
  }

  bool operator <(const packet_sorter_t &cmp) const {
    return (*m_packet_queue)[m_index]->timecode < (*m_packet_queue)[cmp.m_index]->timecode;
  }
};

deque<packet_cptr> *packet_sorter_t::m_packet_queue = NULL;

void
generic_packetizer_c::apply_factory_full_queueing(packet_cptr_di &p_start) {
  packet_sorter_t::m_packet_queue = &packet_queue;

  while (packet_queue.end() != p_start) {
    // Find the next I frame packet.
    packet_cptr_di p_end = p_start + 1;
    while ((packet_queue.end() != p_end) &&
           ((0 <= (*p_end)->fref) || (0 <= (*p_end)->bref)))
      ++p_end;

    // Abort if no such packet was found, but keep on assigning if the
    // packetizer has been flushed already.
    if (!has_been_flushed && (packet_queue.end() == p_end))
      return;

    // Now sort the frames by their timecode as the factory has to be
    // applied to the packets in the same order as they're timestamped.
    vector<packet_sorter_t> sorter;
    bool needs_sorting        = false;
    int64_t previous_timecode = 0;
    int                       i = distance(packet_queue.begin(), p_start);

    packet_cptr_di p_current;
    for (p_current = p_start; p_current != p_end; ++i, ++p_current) {
      sorter.push_back(packet_sorter_t(i));
      if (packet_queue[i]->timecode < previous_timecode)
        needs_sorting = true;
      previous_timecode = packet_queue[i]->timecode;
    }

    if (needs_sorting)
      sort(sorter.begin(), sorter.end());

    // Finally apply the factory.
    for (i = 0; sorter.size() > i; ++i)
      apply_factory_once(packet_queue[sorter[i].m_index]);

    p_start = p_end;
  }
}

void
generic_packetizer_c::force_duration_on_last_packet() {
  if (packet_queue.empty()) {
    mxverb_tid(2, ti.fname, ti.id, "force_duration_on_last_packet: packet queue is empty\n");
    return;
  }
  packet_cptr &packet        = packet_queue.back();
  packet->duration_mandatory = true;
  mxverb_tid(2, ti.fname, ti.id,
             boost::format("force_duration_on_last_packet: forcing at %1% with %|2$.3f|ms\n") % format_timecode(packet->timecode) % (packet->duration / 1000.0));
}

int64_t
generic_packetizer_c::handle_avi_audio_sync(int64_t num_bytes,
                                            bool vbr) {
  if ((0 == ti.avi_samples_per_sec) || (0 == ti.avi_block_align) || (0 == ti.avi_avg_bytes_per_sec) || !ti.avi_audio_sync_enabled) {
    enable_avi_audio_sync(false);
    return -1;
  }

  int64_t duration;
  if (!vbr)
     duration = num_bytes * 1000000000 / ti.avi_avg_bytes_per_sec;

  else {
    double samples = 0.0;

    int i;
    for (i = 0; (ti.avi_block_sizes.size() > i) && (0 < num_bytes); ++i) {
      int64_t block_size = ti.avi_block_sizes[i];
      int cur_bytes      = num_bytes < block_size ? num_bytes : block_size;

      samples   += (double)ti.avi_samples_per_chunk * cur_bytes / ti.avi_block_align;
      num_bytes -= cur_bytes;
    }
    duration = (int64_t)(samples * 1000000000 / ti.avi_samples_per_sec);
  }

  enable_avi_audio_sync(false);

  return duration;
}

void
generic_packetizer_c::connect(generic_packetizer_c *src,
                              int64_t p_append_timecode_offset) {
  m_free_refs                = src->m_free_refs;
  m_next_free_refs           = src->m_next_free_refs;
  track_entry                = src->track_entry;
  hserialno                  = src->hserialno;
  htrack_type                = src->htrack_type;
  htrack_default_duration    = src->htrack_default_duration;
  huid                       = src->huid;
  hcompression               = src->hcompression;
  compressor                 = compressor_c::create(hcompression);
  last_cue_timecode          = src->last_cue_timecode;
  timecode_factory           = src->timecode_factory;
  correction_timecode_offset = 0;

  if (-1 == p_append_timecode_offset)
    append_timecode_offset   = src->max_timecode_seen;
  else
    append_timecode_offset   = p_append_timecode_offset;

  connected_to++;
  if (2 == connected_to)
    process_deferred_packets();
}

void
generic_packetizer_c::set_displacement_maybe(int64_t displacement) {
  if ((1 == ti.tcsync.numerator) && (1 == ti.tcsync.denominator) && (0 == ti.tcsync.displacement))
    ti.tcsync.displacement = displacement;
}

bool
generic_packetizer_c::contains_gap() {
  if (NULL != timecode_factory.get())
    return timecode_factory->contains_gap();
  else
    return false;
}

void
generic_packetizer_c::flush() {
  has_been_flushed = true;
  apply_factory();
}

//--------------------------------------------------------------------

#define add_all_requested_track_ids(type, container)                                              \
  for (map<int64_t, type>::const_iterator i = ti.container.begin(); ti.container.end() != i; ++i) \
    add_requested_track_id(i->first);

#define add_all_requested_track_ids2(container) \
  for (int i = 0; i < ti.container.size(); i++) \
    add_requested_track_id(ti.container[i]);

generic_reader_c::generic_reader_c(track_info_c &_ti)
  : ti(_ti)
  , ptzr_first_packet(NULL)
  , max_timecode_seen(0)
  , chapters(NULL)
  , appending(false)
  , num_video_tracks(0)
  , num_audio_tracks(0)
  , num_subtitle_tracks(0)
  , reference_timecode_tolerance(0)
{

  add_all_requested_track_ids2(atracks);
  add_all_requested_track_ids2(vtracks);
  add_all_requested_track_ids2(stracks);
  add_all_requested_track_ids2(btracks);
  add_all_requested_track_ids(string, all_fourccs);
  add_all_requested_track_ids(display_properties_t, display_properties);
  add_all_requested_track_ids(timecode_sync_t, timecode_syncs);
  add_all_requested_track_ids(cue_strategy_e, cue_creations);
  add_all_requested_track_ids(bool, default_track_flags);
  add_all_requested_track_ids(string, languages);
  add_all_requested_track_ids(string, sub_charsets);
  add_all_requested_track_ids(string, all_tags);
  add_all_requested_track_ids(bool, all_aac_is_sbr);
  add_all_requested_track_ids(compression_method_e, compression_list);
  add_all_requested_track_ids(string, track_names);
  add_all_requested_track_ids(string, all_ext_timecodes);
  add_all_requested_track_ids(pixel_crop_t, pixel_crop_list);
}

generic_reader_c::~generic_reader_c() {
  int i;

  for (i = 0; i < reader_packetizers.size(); i++)
    delete reader_packetizers[i];

  delete chapters;
}

void
generic_reader_c::read_all() {
  int i;

  for (i = 0; reader_packetizers.size() > i; ++i) {
    while (read(reader_packetizers[i], true) != 0)
      ;
  }
}

bool
generic_reader_c::demuxing_requested(char type,
                                     int64_t id) {
  vector<int64_t> *tracks = NULL;

  if ('v' == type) {
    if (ti.no_video)
      return false;
    tracks = &ti.vtracks;

  } else if ('a' == type) {
    if (ti.no_audio)
      return false;
    tracks = &ti.atracks;

  } else if ('s' == type) {
    if (ti.no_subs)
      return false;
    tracks = &ti.stracks;

  } else if ('b' == type) {
    if (ti.no_buttons)
      return false;
    tracks = &ti.btracks;

  } else
    mxerror(boost::format(Y("pr_generic.cpp/generic_reader_c::demuxing_requested(): Invalid track type %1%.")) % type);

  if (tracks->empty())
    return true;

  int i;
  for (i = 0; i < tracks->size(); i++)
    if ((*tracks)[i] == id)
      return true;

  return false;
}

attach_mode_e
generic_reader_c::attachment_requested(int64_t id) {
  if (ti.no_attachments)
    return ATTACH_MODE_SKIP;

  if (ti.attach_mode_list.empty())
    return ATTACH_MODE_TO_ALL_FILES;

  if (map_has_key(ti.attach_mode_list, id))
    return ti.attach_mode_list[id];

  if (map_has_key(ti.attach_mode_list, -1))
    return ti.attach_mode_list[-1];

  return ATTACH_MODE_SKIP;
}

int
generic_reader_c::add_packetizer(generic_packetizer_c *ptzr) {
  reader_packetizers.push_back(ptzr);
  used_track_ids.push_back(ptzr->ti.id);
  if (!appending)
    add_packetizer_globally(ptzr);

  return reader_packetizers.size() - 1;
}

void
generic_reader_c::set_timecode_offset(int64_t offset) {
  max_timecode_seen = offset;

  vector<generic_packetizer_c *>::const_iterator it;
  mxforeach(it, reader_packetizers)
    (*it)->correction_timecode_offset = offset;
}

void
generic_reader_c::set_headers() {
  vector<generic_packetizer_c *>::const_iterator it;

  mxforeach(it, reader_packetizers)
    (*it)->set_headers();
}

void
generic_reader_c::set_headers_for_track(int64_t tid) {
  vector<generic_packetizer_c *>::const_iterator it;

  mxforeach(it, reader_packetizers)
    if ((*it)->ti.id == tid) {
      (*it)->set_headers();
      break;
    }
}

void
generic_reader_c::check_track_ids_and_packetizers() {
  add_available_track_ids();
  if (reader_packetizers.size() == 0)
    mxwarn_fn(ti.fname, Y("No tracks will be copied from this file. This usually indicates a mistake in the command line.\n"));

  int r;
  for (r = 0; requested_track_ids.size() > r; ++r) {
    bool found = false;
    int a;
    for (a = 0; available_track_ids.size() > a; ++a)
      if (requested_track_ids[r] == available_track_ids[a]) {
        found = true;
        break;
      }

    if (!found)
      mxwarn_fn(ti.fname,
                boost::format(Y("A track with the ID %1% was requested but not found in the file. The corresponding option will be ignored.\n"))
                % requested_track_ids[r]);
  }
}

void
generic_reader_c::add_requested_track_id(int64_t id) {
  if (-1 == id)
    return;

  bool found = false;
  int i;
  for (i = 0; i < requested_track_ids.size(); i++)
    if (requested_track_ids[i] == id) {
      found = true;
      break;
    }

  if (!found)
    requested_track_ids.push_back(id);
}

int64_t
generic_reader_c::get_queued_bytes() {
  int64_t  bytes = 0;

  vector<generic_packetizer_c *>::const_iterator it;
  mxforeach(it, reader_packetizers)
    bytes += (*it)->get_queued_bytes();

  return bytes;
}

void
generic_reader_c::flush_packetizers() {
  vector<generic_packetizer_c *>::const_iterator it;

  mxforeach(it, reader_packetizers)
    (*it)->flush();
}

void
generic_reader_c::id_result_container_unsupported(const string &filename,
                                                  const string &info) {
  if (g_identifying) {
    if (g_identify_for_mmg)
      mxinfo(boost::format("File '%1%': unsupported container: %2%\n") % filename % info);
    else
      mxinfo(boost::format(Y("File '%1%': unsupported container: %2%\n")) % filename % info);
    mxexit(3);

  } else
    mxerror(boost::format(Y("The file '%1%' is a non-supported file type (%2%).\n")) % filename % info);
}

void
generic_reader_c::id_result_container(const string &info,
                                      const string &verbose_info) {
  id_results_container.info = info;
  id_results_container.verbose_info.clear();
  if (!verbose_info.empty())
    id_results_container.verbose_info.push_back(verbose_info);
}

void
generic_reader_c::id_result_container(const string &info,
                                      const vector<string> &verbose_info) {
  id_results_container.info         = info;
  id_results_container.verbose_info = verbose_info;
}

void
generic_reader_c::id_result_track(int64_t track_id,
                                  const string &type,
                                  const string &info,
                                  const string &verbose_info) {
  id_result_t result(track_id, type, info, empty_string, 0);
  if (!verbose_info.empty())
    result.verbose_info.push_back(verbose_info);
  id_results_tracks.push_back(result);
}

void
generic_reader_c::id_result_track(int64_t track_id,
                                  const string &type,
                                  const string &info,
                                  const vector<string> &verbose_info) {
  id_result_t result(track_id, type, info, empty_string, 0);
  result.verbose_info = verbose_info;
  id_results_tracks.push_back(result);
}

void
generic_reader_c::id_result_attachment(int64_t attachment_id,
                                       const string &type,
                                       int size,
                                       const string &file_name,
                                       const string &description) {
  id_result_t result(attachment_id, type, file_name, description, size);
  id_results_attachments.push_back(result);
}

void
generic_reader_c::display_identification_results() {
  string format_file, format_track, format_attachment, format_att_description, format_att_file_name;

  if (g_identify_for_mmg) {
    format_file            =   "File '%1%': container: %2%";
    format_track           =   "Track ID %1%: %2% (%3%)";
    format_attachment      =   "Attachment ID %1%: type \"%2%\", size %3% bytes";
    format_att_description =   ", description \"%1%\"";
    format_att_file_name   =   ", file name \"%1%\"";

  } else {
    format_file            = Y("File '%1%': container: %2%");
    format_track           = Y("Track ID %1%: %2% (%3%)");
    format_attachment      = Y("Attachment ID %1%: type '%2%', size %3% bytes");
    format_att_description = Y(", description '%1%'");
    format_att_file_name   = Y(", file name '%1%'");
  }

  mxinfo(boost::format(format_file) % ti.fname % id_results_container.info);

  if (g_identify_verbose && !id_results_container.verbose_info.empty())
    mxinfo(boost::format(" [%1%]") % join(" ", id_results_container.verbose_info));

  mxinfo("\n");

  int i;
  for (i = 0; i < id_results_tracks.size(); ++i) {
    id_result_t &result = id_results_tracks[i];

    mxinfo(boost::format(format_track) % result.id % result.type % result.info);

    if (g_identify_verbose && !result.verbose_info.empty())
      mxinfo(boost::format(" [%1%]") % join(" ", result.verbose_info));

    mxinfo("\n");
  }

  for (i = 0; i < id_results_attachments.size(); ++i) {
    id_result_t &result = id_results_attachments[i];

    mxinfo(boost::format(format_attachment) % result.id % id_escape_string(result.type) % result.size);

    if (!result.description.empty())
      mxinfo(boost::format(format_att_description) % id_escape_string(result.description));

    if (!result.info.empty())
      mxinfo(boost::format(format_att_file_name) % id_escape_string(result.info));

    mxinfo("\n");
  }
}

std::string
generic_reader_c::id_escape_string(const std::string &s) {
  return g_identify_for_mmg ? escape(s) : s;
}

//
//--------------------------------------------------------------------

track_info_c::track_info_c()
  : initialized(true)
  , id(0)
  , no_audio(false)
  , no_video(false)
  , no_subs(false)
  , no_buttons(false)
  , private_data(NULL)
  , private_size(0)
  , aspect_ratio(0.0)
  , display_width(0)
  , display_height(0)
  , aspect_ratio_given(false)
  , aspect_ratio_is_factor(false)
  , display_dimensions_given(false)
  , cues(CUE_STRATEGY_UNSPECIFIED)
  , default_track(boost::logic::indeterminate)
  , forced_track(boost::logic::indeterminate)
  , tags(NULL)
  , compression(COMPRESSION_UNSPECIFIED)
  , pixel_cropping_specified(false)
  , stereo_mode(STEREO_MODE_UNSPECIFIED)
  , nalu_size_length(0)
  , no_chapters(false)
  , no_attachments(false)
  , no_tags(false)
  , avi_block_align(0)
  , avi_samples_per_sec(0)
  , avi_avg_bytes_per_sec(0)
  , avi_samples_per_chunk(0)
  , avi_audio_sync_enabled(false)
{
  memset(&pixel_cropping, 0, sizeof(pixel_crop_t));
}

void
track_info_c::free_contents() {
  if (!initialized)
    return;

  safefree(private_data);
  delete tags;

  initialized = false;
}

track_info_c &
track_info_c::operator =(const track_info_c &src) {
  free_contents();

  id                         = src.id;
  fname                      = src.fname;

  no_audio                   = src.no_audio;
  no_video                   = src.no_video;
  no_subs                    = src.no_subs;
  no_buttons                 = src.no_buttons;
  atracks                    = src.atracks;
  btracks                    = src.btracks;
  stracks                    = src.stracks;
  vtracks                    = src.vtracks;

  private_size               = src.private_size;
  private_data               = (unsigned char *)safememdup(src.private_data, private_size);

  all_fourccs                = src.all_fourccs;

  display_properties         = src.display_properties;
  aspect_ratio               = src.aspect_ratio;
  aspect_ratio_given         = false;
  aspect_ratio_is_factor     = false;
  display_dimensions_given   = false;

  timecode_syncs             = src.timecode_syncs;
  memcpy(&tcsync, &src.tcsync, sizeof(timecode_sync_t));

  cue_creations              = src.cue_creations;
  cues                       = src.cues;

  default_track_flags        = src.default_track_flags;
  default_track              = src.default_track;

  forced_track_flags         = src.forced_track_flags;
  forced_track               = src.forced_track;

  languages                  = src.languages;
  language                   = src.language;

  sub_charsets               = src.sub_charsets;
  sub_charset                = src.sub_charset;

  all_tags                   = src.all_tags;
  tags_file_name             = src.tags_file_name;
  tags                       = NULL != src.tags ? static_cast<KaxTags *>(src.tags->Clone()) : NULL;

  all_aac_is_sbr             = src.all_aac_is_sbr;

  compression_list           = src.compression_list;
  compression                = src.compression;

  track_names                = src.track_names;
  track_name                 = src.track_name;

  all_ext_timecodes          = src.all_ext_timecodes;
  ext_timecodes              = src.ext_timecodes;

  pixel_crop_list            = src.pixel_crop_list;
  pixel_cropping             = src.pixel_cropping;
  pixel_cropping_specified   = src.pixel_cropping_specified;

  stereo_mode_list           = src.stereo_mode_list;
  stereo_mode                = src.stereo_mode;

  nalu_size_lengths          = src.nalu_size_lengths;
  nalu_size_length           = src.nalu_size_length;

  attach_mode_list           = src.attach_mode_list;

  no_chapters                = src.no_chapters;
  no_attachments             = src.no_attachments;
  no_tags                    = src.no_tags;

  chapter_charset            = src.chapter_charset;

  avi_block_align            = src.avi_block_align;
  avi_samples_per_sec        = src.avi_samples_per_sec;
  avi_avg_bytes_per_sec      = src.avi_avg_bytes_per_sec;
  avi_samples_per_chunk      = src.avi_samples_per_chunk;
  avi_block_sizes.clear();
  avi_audio_sync_enabled     = false;

  default_durations          = src.default_durations;
  max_blockadd_ids           = src.max_blockadd_ids;

  initialized                = true;

  return *this;
}
