/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   functions common for all readers/packetizers

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Modified by Steve Lhomme <steve.lhomme@free.fr>.
*/

#include <map>
#include <string>

#include <matroska/KaxContentEncoding.h>
#include <matroska/KaxTag.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>

#include "base64.h"
#include "common.h"
#include "commonebml.h"
#include "compression.h"
#include "mkvmerge.h"
#include "output_control.h"
#include "pr_generic.h"
#include "tagparser.h"
#include "tag_common.h"

using namespace std;

int64_t packet_t::packet_number_counter = 0;

packet_t::~packet_t() {
}

// ---------------------------------------------------------------------

vector<generic_packetizer_c *> ptzrs_in_header_order;

generic_packetizer_c::generic_packetizer_c(generic_reader_c *nreader,
                                           track_info_c &_ti)
  throw(error_c): ti(_ti) {
  int i;

#ifdef DEBUG
  debug_facility.add_packetizer(this);
#endif
  reader = nreader;

  track_entry = NULL;
  m_free_refs = -1;
  m_next_free_refs = -1;
  enqueued_bytes = 0;
  safety_last_timecode = 0;
  safety_last_duration = 0;
  relaxed_timecode_checking = false;
  last_cue_timecode = -1;
  correction_timecode_offset = 0;
  append_timecode_offset = 0;
  max_timecode_seen = 0;
  default_track_warning_printed = false;
  connected_to = 0;
  has_been_flushed = false;
  timecode_factory_application_mode = TFA_AUTOMATIC;

  // Let's see if the user specified audio sync for this track.
  if (map_has_key(ti.audio_syncs, ti.id))
    ti.async = ti.audio_syncs[ti.id];
  else if (map_has_key(ti.audio_syncs, -1))
    ti.async = ti.audio_syncs[-1];
  if (0.0 == ti.async.linear)
    ti.async.linear = 1.0;
  ti.async.displacement *= (int64_t)1000000; // ms to ns
  initial_displacement = ti.async.displacement;
  ti.async.displacement = 0;

  // Let's see if the user has specified a delay for this track.
  if (map_has_key(ti.packet_delays, ti.id))
    ti.packet_delay = ti.packet_delays[ti.id];
  else if (map_has_key(ti.packet_delays, -1))
    ti.packet_delay = ti.packet_delays[-1];

  // Let's see if the user has specified which cues he wants for this track.
  if (map_has_key(ti.cue_creations, ti.id))
    ti.cues = ti.cue_creations[ti.id];
  else if (map_has_key(ti.cue_creations, -1))
    ti.cues = ti.cue_creations[-1];

  // Let's see if the user has given a default track flag for this track.
  for (i = 0; ti.default_track_flags.size() > i; ++i)
    if ((-1 == ti.default_track_flags[i]) ||
        (ti.id == ti.default_track_flags[i])) {
      ti.default_track = true;
      break;
    }

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
  i = -2;
  if (map_has_key(ti.display_properties, ti.id))
    i = ti.id;
  else if (map_has_key(ti.display_properties, -1))
    i = -1;
  if (-2 != i) {
    display_properties_t &dprop = ti.display_properties[i];
    if (dprop.aspect_ratio < 0) {
      ti.display_width = dprop.width;
      ti.display_height = dprop.height;
      ti.display_dimensions_given = true;
    } else {
      ti.aspect_ratio = dprop.aspect_ratio;
      ti.aspect_ratio_given = true;
      ti.aspect_ratio_is_factor = dprop.ar_factor;
    }
  }
  if (ti.aspect_ratio_given && ti.display_dimensions_given)
    mxerror(_("Both '%s' and '--display-dimensions' were given "
              "for track " LLD " of '%s'.\n"), ti.aspect_ratio_is_factor ?
            _("Aspect ratio factor") : _("Aspect ratio"), ti.id,
            ti.fname.c_str());

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
  htrack_default_duration = -1;
  default_duration_forced = true;
  if (map_has_key(ti.default_durations, ti.id))
    htrack_default_duration = ti.default_durations[ti.id];
  else if (map_has_key(ti.default_durations, -1))
    htrack_default_duration = ti.default_durations[-1];
  else
    default_duration_forced = false;

  // Let's see if the user has set a max_block_add_id
  htrack_max_add_block_ids = -1;
  if (map_has_key(ti.max_blockadd_ids, ti.id))
    htrack_max_add_block_ids = ti.max_blockadd_ids[ti.id];
  else if (map_has_key(ti.max_blockadd_ids, -1))
    htrack_max_add_block_ids = ti.max_blockadd_ids[-1];

  // Let's see if the user has specified "AAC is SBR" for this track.
  if (map_has_key(ti.all_aac_is_sbr, ti.id))
    ti.aac_is_sbr = ti.all_aac_is_sbr[ti.id] ? 1 : 0;
  else if (map_has_key(ti.all_aac_is_sbr, -1))
    ti.aac_is_sbr = ti.all_aac_is_sbr[-1] ? 1 : 0;

  // Set default header values to 'unset'.
  if (!reader->appending)
    hserialno = create_track_number(reader, ti.id);
  huid = 0;
  htrack_type = -1;
  htrack_min_cache = 0;
  htrack_max_cache = -1;

  hcodec_id = "";
  hcodec_private = NULL;
  hcodec_private_length = 0;

  haudio_sampling_freq = -1.0;
  haudio_output_sampling_freq = -1.0;
  haudio_channels = -1;
  haudio_bit_depth = -1;

  hvideo_pixel_width = -1;
  hvideo_pixel_height = -1;
  hvideo_display_width = -1;
  hvideo_display_height = -1;

  hcompression = COMPRESSION_UNSPECIFIED;

  timecode_factory = timecode_factory_c::create(ti.ext_timecodes,
                                                ti.fname, ti.id);
}

generic_packetizer_c::~generic_packetizer_c() {
  safefree(hcodec_private);
  if (!packet_queue.empty())
    mxerror("Packet queue not empty for new track ID " LLD " (flushed: %d). "
            "Frames have been lost during remux. %s\n", ti.id,
            has_been_flushed, BUGMSG);
}

void
generic_packetizer_c::set_tag_track_uid() {
  int is, it, k;
  KaxTag *tag;
  KaxTagTargets *targets;
  EbmlElement *el;

  if (ti.tags == NULL)
    return;

  convert_old_tags(*ti.tags);
  for (is = 0; is < ti.tags->ListSize(); is++) {
    tag = (KaxTag *)(*ti.tags)[is];

    for (it = 0; it < tag->ListSize(); it++) {
      el = (*tag)[it];
      if (is_id(el, KaxTagTargets)) {
        targets = static_cast<KaxTagTargets *>(el);
        k = 0;
        while (k < targets->ListSize()) {
          el = (*targets)[k];
          if (is_id(el, KaxTagTrackUID)) {
            targets->Remove(k);
            delete el;
          } else
            k++;
        }
      }
    }

    targets = &GetChild<KaxTagTargets>(*tag);
    *(static_cast<EbmlUInteger *>(&GetChild<KaxTagTrackUID>(*targets))) =
      huid;

    fix_mandatory_tag_elements(tag);
    if (!tag->CheckMandatory())
      mxerror(_("The tags in '%s' could not be parsed: some mandatory "
                "elements are missing.\n"),
              ti.tags_file_name != "" ? ti.tags_file_name.c_str():
              ti.fname.c_str());
  }
}

int
generic_packetizer_c::set_uid(uint32_t uid) {
  if (is_unique_uint32(uid, UNIQUE_TRACK_IDS)) {
    add_unique_uint32(uid, UNIQUE_TRACK_IDS);
    huid = uid;
    if (track_entry != NULL)
      *(static_cast<EbmlUInteger *>(&GetChild<KaxTrackUID>(*track_entry))) =
        huid;
    return 1;
  }

  return 0;
}

void
generic_packetizer_c::set_track_type(int type,
                                     timecode_factory_application_e tfa_mode) {
  htrack_type = type;

  if ((type == track_audio) && (ti.cues == CUE_STRATEGY_UNSPECIFIED))
    ti.cues = CUE_STRATEGY_SPARSE;
  if (type == track_audio)
    reader->num_audio_tracks++;
  else if (type == track_video) {
    reader->num_video_tracks++;
    if (video_packetizer == NULL)
      video_packetizer = this;
  } else
    reader->num_subtitle_tracks++;

  if ((TFA_AUTOMATIC == tfa_mode) &&
      (TFA_AUTOMATIC == timecode_factory_application_mode))
    timecode_factory_application_mode =
      (track_video == type) ? TFA_FULL_QUEUEING :
      (track_subtitle == type) ? TFA_IMMEDIATE :
      (track_buttons == type) ? TFA_IMMEDIATE :
      TFA_FULL_QUEUEING;
  else if (TFA_AUTOMATIC != tfa_mode)
    timecode_factory_application_mode = tfa_mode;
}

void
generic_packetizer_c::set_track_name(const string &name) {
  ti.track_name = name;
  if ((track_entry != NULL) && (name != ""))
    *(static_cast<EbmlUnicodeString *>
      (&GetChild<KaxTrackName>(*track_entry))) =
      cstrutf8_to_UTFstring(ti.track_name);
}

void
generic_packetizer_c::set_codec_id(const string &id) {
  hcodec_id = id;
  if ((track_entry != NULL) && (id != ""))
    *(static_cast<EbmlString *>
      (&GetChild<KaxCodecID>(*track_entry))) = hcodec_id;
}

void
generic_packetizer_c::set_codec_private(const unsigned char *cp,
                                        int length) {
  safefree(hcodec_private);
  if (cp == NULL) {
    hcodec_private = NULL;
    hcodec_private_length = 0;
    return;
  }
  hcodec_private = (unsigned char *)safememdup(cp, length);
  hcodec_private_length = length;
  if (track_entry != NULL) {
    KaxCodecPrivate &codec_private = GetChild<KaxCodecPrivate>(*track_entry);
    codec_private.CopyBuffer((binary *)hcodec_private, hcodec_private_length);
  }
}

void
generic_packetizer_c::set_track_min_cache(int min_cache) {
  htrack_min_cache = min_cache;
  if (track_entry != NULL)
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxTrackMinCache>(*track_entry))) = min_cache;
}

void
generic_packetizer_c::set_track_max_cache(int max_cache) {
  htrack_max_cache = max_cache;
  if (track_entry != NULL)
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxTrackMinCache>(*track_entry))) = max_cache;
}

void
generic_packetizer_c::set_track_default_duration(int64_t def_dur) {
  if (default_duration_forced)
    return;
  htrack_default_duration = def_dur;
  if (track_entry != NULL)
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxTrackDefaultDuration>(*track_entry))) =
      htrack_default_duration;
}

void
generic_packetizer_c::set_track_max_additionals(int max_add_block_ids) {
  htrack_max_add_block_ids = max_add_block_ids;
  if (track_entry != NULL)
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxMaxBlockAdditionID>(*track_entry))) = max_add_block_ids;
}

int64_t
generic_packetizer_c::get_track_default_duration() {
  return htrack_default_duration;
}

void
generic_packetizer_c::set_audio_sampling_freq(float freq) {
  haudio_sampling_freq = freq;
  if (track_entry != NULL) {
    KaxTrackAudio &audio = GetChild<KaxTrackAudio>(*track_entry);
    *(static_cast<EbmlFloat *>(&GetChild<KaxAudioSamplingFreq>(audio))) =
      haudio_sampling_freq;
  }
}

void
generic_packetizer_c::set_audio_output_sampling_freq(float freq) {
  haudio_output_sampling_freq = freq;
  if (track_entry != NULL) {
    KaxTrackAudio &audio = GetChild<KaxTrackAudio>(*track_entry);
    *(static_cast<EbmlFloat *>(&GetChild<KaxAudioOutputSamplingFreq>(audio))) =
      haudio_output_sampling_freq;
  }
}

void
generic_packetizer_c::set_audio_channels(int channels) {
  haudio_channels = channels;
  if (track_entry != NULL) {
    KaxTrackAudio &audio = GetChild<KaxTrackAudio>(*track_entry);
    *(static_cast<EbmlUInteger *>(&GetChild<KaxAudioChannels>(audio))) =
      haudio_channels;
  }
}

void
generic_packetizer_c::set_audio_bit_depth(int bit_depth) {
  haudio_bit_depth = bit_depth;
  if (track_entry != NULL) {
    KaxTrackAudio &audio = GetChild<KaxTrackAudio>(*track_entry);
    *(static_cast<EbmlUInteger *>(&GetChild<KaxAudioBitDepth>(audio))) =
      haudio_bit_depth;
  }
}

void
generic_packetizer_c::set_video_pixel_width(int width) {
  hvideo_pixel_width = width;
  if (track_entry != NULL) {
    KaxTrackVideo &video = GetChild<KaxTrackVideo>(*track_entry);
    *(static_cast<EbmlUInteger *>(&GetChild<KaxVideoPixelWidth>(video))) =
      hvideo_pixel_width;
  }
}

void
generic_packetizer_c::set_video_pixel_height(int height) {
  hvideo_pixel_height = height;
  if (track_entry != NULL) {
    KaxTrackVideo &video = GetChild<KaxTrackVideo>(*track_entry);
    *(static_cast<EbmlUInteger *>(&GetChild<KaxVideoPixelHeight>(video))) =
      hvideo_pixel_height;
  }
}

void
generic_packetizer_c::set_video_display_width(int width) {
  hvideo_display_width = width;
  if (track_entry != NULL) {
    KaxTrackVideo &video = GetChild<KaxTrackVideo>(*track_entry);
    *(static_cast<EbmlUInteger *>(&GetChild<KaxVideoDisplayWidth>(video))) =
      hvideo_display_width;
  }
}

void
generic_packetizer_c::set_video_display_height(int height) {
  hvideo_display_height = height;
  if (track_entry != NULL) {
    KaxTrackVideo &video = GetChild<KaxTrackVideo>(*track_entry);
    *(static_cast<EbmlUInteger *>(&GetChild<KaxVideoDisplayHeight>(video))) =
      hvideo_display_height;
  }
}

void
generic_packetizer_c::set_as_default_track(int type,
                                           int priority) {
  if (default_tracks_priority[type] < priority) {
    default_tracks_priority[type] = priority;
    default_tracks[type] = hserialno;
  } else if ((priority == DEFAULT_TRACK_PRIORITY_CMDLINE) &&
             (default_tracks[type] != hserialno) &&
             !default_track_warning_printed) {
    mxwarn(_("Another default track for %s tracks has already "
             "been set. The 'default' flag for track " LLD " of '%s' will not "
             "be set.\n"),
           type == 0 ? "audio" : type == 'v' ? "video" : "subtitle",
           ti.id, ti.fname.c_str());
    default_track_warning_printed = true;
  }
}

void
generic_packetizer_c::set_language(const string &language) {
  ti.language = language;
  if (track_entry != NULL)
    *(static_cast<EbmlString *>
      (&GetChild<KaxTrackLanguage>(*track_entry))) = ti.language;
}

void
generic_packetizer_c::set_video_pixel_cropping(int left,
                                               int top,
                                               int right,
                                               int bottom) {
  ti.pixel_cropping.left = left;
  ti.pixel_cropping.top = top;
  ti.pixel_cropping.right = right;
  ti.pixel_cropping.bottom = bottom;

  if (track_entry != NULL) {
    KaxTrackVideo &video = GetChild<KaxTrackVideo>(*track_entry);

    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxVideoPixelCropLeft>(video))) =
      ti.pixel_cropping.left;
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxVideoPixelCropTop>(video))) =
      ti.pixel_cropping.top;
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxVideoPixelCropRight>(video))) =
      ti.pixel_cropping.right;
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxVideoPixelCropBottom>(video))) =
      ti.pixel_cropping.bottom;
  }
}

void
generic_packetizer_c::set_stereo_mode(stereo_mode_e stereo_mode) {
  ti.stereo_mode = stereo_mode;

  if ((NULL != track_entry) &&
      (STEREO_MODE_UNSPECIFIED != stereo_mode)) {
    KaxTrackVideo &video = GetChild<KaxTrackVideo>(*track_entry);

    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxVideoStereoMode>(video))) = ti.stereo_mode;
  }
}

void
generic_packetizer_c::set_headers() {
  int idx, disp_width, disp_height;
  KaxTag *tag;
  bool found;

  if (connected_to > 0)
    return;

  found = false;
  for (idx = 0; idx < ptzrs_in_header_order.size(); idx++)
    if (ptzrs_in_header_order[idx] == this) {
      found = true;
      break;
    }
  if (!found)
    ptzrs_in_header_order.push_back(this);

  if (track_entry == NULL) {
    if (kax_last_entry == NULL)
      track_entry = &GetChild<KaxTrackEntry>(kax_tracks);
    else
      track_entry =
        &GetNextChild<KaxTrackEntry>(kax_tracks, *kax_last_entry);
    kax_last_entry = track_entry;
    track_entry->SetGlobalTimecodeScale((int64_t)timecode_scale);
  }

  KaxTrackNumber &tnumber = GetChild<KaxTrackNumber>(*track_entry);
  *(static_cast<EbmlUInteger *>(&tnumber)) = hserialno;

  if (huid == 0)
    huid = create_unique_uint32(UNIQUE_TRACK_IDS);

  KaxTrackUID &tuid = GetChild<KaxTrackUID>(*track_entry);
  *(static_cast<EbmlUInteger *>(&tuid)) = huid;

  if (htrack_type != -1)
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxTrackType>(*track_entry))) = htrack_type;

  if (hcodec_id != "")
    *(static_cast<EbmlString *>
      (&GetChild<KaxCodecID>(*track_entry))) = hcodec_id;

  if (hcodec_private != NULL) {
    KaxCodecPrivate &codec_private = GetChild<KaxCodecPrivate>(*track_entry);
    codec_private.CopyBuffer((binary *)hcodec_private, hcodec_private_length);
  }

  if (htrack_min_cache != -1)
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxTrackMinCache>(*track_entry))) = htrack_min_cache;

  if (htrack_max_cache != -1)
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxTrackMaxCache>(*track_entry))) = htrack_max_cache;

  if (htrack_max_add_block_ids != -1)
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxMaxBlockAdditionID>(*track_entry))) =
      htrack_max_add_block_ids;

  if (NULL != timecode_factory.get())
    htrack_default_duration =
      (int64_t)timecode_factory->get_default_duration(htrack_default_duration);
  if (htrack_default_duration != -1.0)
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxTrackDefaultDuration>(*track_entry))) =
      htrack_default_duration;

  if (htrack_type == track_audio)
    idx = DEFTRACK_TYPE_AUDIO;
  else if (htrack_type == track_video)
    idx = DEFTRACK_TYPE_VIDEO;
  else
    idx = DEFTRACK_TYPE_SUBS;

  if (ti.default_track)
    set_as_default_track(idx, DEFAULT_TRACK_PRIORITY_CMDLINE);
  else
    set_as_default_track(idx, DEFAULT_TRACK_PRIORITY_FROM_TYPE);

  if (ti.language != "")
    *(static_cast<EbmlString *>
      (&GetChild<KaxTrackLanguage>(*track_entry))) = ti.language;
  else
    *(static_cast<EbmlString *>
      (&GetChild<KaxTrackLanguage>(*track_entry))) = default_language.c_str();

  if (ti.track_name != "")
    *(static_cast<EbmlUnicodeString *>
      (&GetChild<KaxTrackName>(*track_entry))) =
      cstrutf8_to_UTFstring(ti.track_name);

  if (htrack_type == track_video) {
    KaxTrackVideo &video =
      GetChild<KaxTrackVideo>(*track_entry);

    if ((hvideo_pixel_height != -1) && (hvideo_pixel_width != -1)) {
      if ((hvideo_display_width == -1) || (hvideo_display_height == -1) ||
          ti.aspect_ratio_given || ti.display_dimensions_given) {
        if (ti.display_dimensions_given) {
          disp_width = ti.display_width;
          disp_height = ti.display_height;

        } else {
          if (!ti.aspect_ratio_given)
            ti.aspect_ratio = (float)hvideo_pixel_width /
              (float)hvideo_pixel_height;
          else if (ti.aspect_ratio_is_factor)
            ti.aspect_ratio = (float)hvideo_pixel_width * ti.aspect_ratio /
              (float)hvideo_pixel_height;
          if (ti.aspect_ratio >
              ((float)hvideo_pixel_width / (float)hvideo_pixel_height)) {
            disp_width = irnd(hvideo_pixel_height * ti.aspect_ratio);
            disp_height = hvideo_pixel_height;

          } else {
            disp_width = hvideo_pixel_width;
            disp_height = irnd(hvideo_pixel_width / ti.aspect_ratio);

          }

        }

      } else {
        disp_width = hvideo_display_width;
        disp_height = hvideo_display_height;
      }

      *(static_cast<EbmlUInteger *>
        (&GetChild<KaxVideoPixelWidth>(video))) = hvideo_pixel_width;
      *(static_cast<EbmlUInteger *>
        (&GetChild<KaxVideoPixelHeight>(video))) = hvideo_pixel_height;

      KaxVideoDisplayWidth &dwidth =
        GetChild<KaxVideoDisplayWidth>(video);
      *(static_cast<EbmlUInteger *>(&dwidth)) = disp_width;
      dwidth.SetDefaultSize(4);
      ti.display_width = disp_width;

      KaxVideoDisplayHeight &dheight =
        GetChild<KaxVideoDisplayHeight>(video);
      *(static_cast<EbmlUInteger *>(&dheight)) = disp_height;
      dheight.SetDefaultSize(4);
      ti.display_height = disp_height;

      if (ti.pixel_cropping_specified) {
        *(static_cast<EbmlUInteger *>
          (&GetChild<KaxVideoPixelCropLeft>(video))) =
          ti.pixel_cropping.left;
        *(static_cast<EbmlUInteger *>
          (&GetChild<KaxVideoPixelCropTop>(video))) =
          ti.pixel_cropping.top;
        *(static_cast<EbmlUInteger *>
          (&GetChild<KaxVideoPixelCropRight>(video))) =
          ti.pixel_cropping.right;
        *(static_cast<EbmlUInteger *>
          (&GetChild<KaxVideoPixelCropBottom>(video))) =
          ti.pixel_cropping.bottom;
      }

      if (STEREO_MODE_UNSPECIFIED != ti.stereo_mode)
        *(static_cast<EbmlUInteger *>
          (&GetChild<KaxVideoStereoMode>(video))) = ti.stereo_mode;
    }

  } else if (htrack_type == track_audio) {
    KaxTrackAudio &audio =
      GetChild<KaxTrackAudio>(*track_entry);

    if (haudio_sampling_freq != -1.0)
      *(static_cast<EbmlFloat *>(&GetChild<KaxAudioSamplingFreq>(audio))) =
        haudio_sampling_freq;

    if (haudio_output_sampling_freq != -1.0)
      *(static_cast<EbmlFloat *>
        (&GetChild<KaxAudioOutputSamplingFreq>(audio))) =
        haudio_output_sampling_freq;

    if (haudio_channels != -1)
      *(static_cast<EbmlUInteger *>(&GetChild<KaxAudioChannels>(audio))) =
        haudio_channels;

    if (haudio_bit_depth != -1)
      *(static_cast<EbmlUInteger *>(&GetChild<KaxAudioBitDepth>(audio))) =
        haudio_bit_depth;

  } else if (htrack_type == track_buttons) {
    if ((hvideo_pixel_height != -1) && (hvideo_pixel_width != -1)) {
      KaxTrackVideo &video = GetChild<KaxTrackVideo>(*track_entry);

      *(static_cast<EbmlUInteger *>
        (&GetChild<KaxVideoPixelWidth>(video))) = hvideo_pixel_width;
      *(static_cast<EbmlUInteger *>
        (&GetChild<KaxVideoPixelHeight>(video))) = hvideo_pixel_height;
    }

  }

  if (ti.compression != COMPRESSION_UNSPECIFIED)
    hcompression = ti.compression;
  if ((hcompression != COMPRESSION_UNSPECIFIED) &&
      (hcompression != COMPRESSION_NONE)) {
    KaxContentEncodings *c_encodings;
    KaxContentEncoding *c_encoding;

    c_encodings = &GetChild<KaxContentEncodings>(*track_entry);
    c_encoding = &GetChild<KaxContentEncoding>(*c_encodings);
    // First modification
    *static_cast<EbmlUInteger *>
      (&GetChild<KaxContentEncodingOrder>(*c_encoding)) = 0;
    // It's a compression.
    *static_cast<EbmlUInteger *>
      (&GetChild<KaxContentEncodingType>(*c_encoding)) = 0;
    // Only the frame contents have been compresed.
    *static_cast<EbmlUInteger *>
      (&GetChild<KaxContentEncodingScope>(*c_encoding)) = 1;

    compressor = compressor_c::create(hcompression);
    compressor->set_track_headers(*c_encoding);
  }

  if (no_lacing)
    track_entry->EnableLacing(false);

  set_tag_track_uid();
  if (ti.tags != NULL) {
    while (ti.tags->ListSize() != 0) {
      tag = (KaxTag *)(*ti.tags)[0];
      add_tags(tag);
      ti.tags->Remove(0);
    }
  }
}

void
generic_packetizer_c::fix_headers() {
  int idx;

  if (htrack_type == track_audio)
    idx = DEFTRACK_TYPE_AUDIO;
  else if (htrack_type == track_video)
    idx = DEFTRACK_TYPE_VIDEO;
  else
    idx = DEFTRACK_TYPE_SUBS;

  if (default_tracks[idx] == hserialno)
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxTrackFlagDefault>(*track_entry))) = 1;
  else
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxTrackFlagDefault>(*track_entry))) = 0;

  track_entry->SetGlobalTimecodeScale((int64_t)timecode_scale);
}

void
generic_packetizer_c::add_packet(packet_cptr pack) {
  int length, i;

  if (reader->ptzr_first_packet == NULL)
    reader->ptzr_first_packet = this;

  // strip elements to be removed
  if ((htrack_max_add_block_ids != -1) &&
      (htrack_max_add_block_ids < pack->data_adds.size()))
    pack->data_adds.resize(htrack_max_add_block_ids);

  length = pack->data->get_size();
  if (NULL != compressor.get()) {
    try {
      compressor->compress(pack->data);
      for (i = 0; i < pack->data_adds.size(); i++)
        compressor->compress(pack->data_adds[i]);
    } catch (compression_error_c &e) {
      mxerror(FMT_TID "Compression failed: %s\n", ti.fname.c_str(),
              (int64_t)ti.id, e.get_error().c_str());
    }
  } else {
    pack->data->grab();
    for (i = 0; i < pack->data_adds.size(); i++)
      pack->data_adds[i]->grab();
  }
  pack->source = this;

  enqueued_bytes += pack->data->get_size();

  if ((0 > pack->bref) && (0 <= pack->fref)) {
    int64_t tmp = pack->bref;
    pack->bref = pack->fref;
    pack->fref = tmp;
  }

  if (connected_to != 1)
    add_packet2(pack);
  else
    deferred_packets.push_back(pack);
}

void
generic_packetizer_c::add_packet2(packet_cptr pack) {
  pack->timecode += correction_timecode_offset + append_timecode_offset +
    ti.packet_delay;
  if (pack->bref >= 0)
    pack->bref += correction_timecode_offset + append_timecode_offset +
      ti.packet_delay;
  if (pack->fref >= 0)
    pack->fref += correction_timecode_offset + append_timecode_offset +
      ti.packet_delay;

  if ((htrack_min_cache < 2) && (pack->fref >= 0)) {
    set_track_min_cache(2);
    rerender_track_headers();
  } else if ((htrack_min_cache < 1) && (pack->bref >= 0)) {
    set_track_min_cache(1);
    rerender_track_headers();
  }

  if (pack->timecode < 0) {
    return;
  }

  // 'timecode < safety_last_timecode' may only occur for B frames. In this
  // case we have the coding order, e.g. IPB1B2 and the timecodes
  // I: 0, P: 120, B1: 40, B2: 80.
  if (!relaxed_timecode_checking &&
      (pack->timecode < safety_last_timecode) && (pack->fref < 0)) {
    if (htrack_type == track_audio) {
      int64_t needed_timecode_offset;

      needed_timecode_offset = safety_last_timecode +
        safety_last_duration - pack->timecode;
      correction_timecode_offset += needed_timecode_offset;
      pack->timecode += needed_timecode_offset;
      if (pack->bref >= 0)
        pack->bref += needed_timecode_offset;
      if (pack->fref >= 0)
        pack->fref += needed_timecode_offset;
      mxwarn(FMT_TID "The current packet's "
             "timecode is smaller than that of the previous packet. This "
             "usually means that the source file is a Matroska file that "
             "has not been created 100%% correctly. The timecodes of all "
             "packets will be adjusted by " LLD "ms in order not to lose any "
             "data. This may throw A/V sync off, but that can be corrected "
             "with mkvmerge's \"--sync\" option. If you already use "
             "\"--sync\" and you still get this warning then do NOT worry "
             "-- this is normal. "
             "If this error happens more than once and you get this message "
             "more than once for a particular track then "
             "either is the source file badly mastered, or mkvmerge "
             "contains a bug. In this case you should contact the author "
             "Moritz Bunkus <moritz@bunkus.org>.\n", ti.fname.c_str(), ti.id,
             (needed_timecode_offset + 500000) / 1000000);
    } else
      mxwarn("pr_generic.cpp/generic_packetizer_c::add_packet(): timecode < "
             "last_timecode (" FMT_TIMECODE " < " FMT_TIMECODE ") for " LLD
             " of '%s'. %s\n", ARG_TIMECODE_NS(pack->timecode),
             ARG_TIMECODE_NS(safety_last_timecode), ti.id, ti.fname.c_str(),
             BUGMSG);
  }
  safety_last_timecode = pack->timecode;
  safety_last_duration = pack->duration;
  pack->timecode_before_factory = pack->timecode;

  packet_queue.push_back(pack);
  if ((NULL == timecode_factory.get()) ||
      (TFA_IMMEDIATE == timecode_factory_application_mode))
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

bool
generic_packetizer_c::has_enough_packets() const {
  if (packet_queue.size() < 2)
    return false;

  const packet_cptr &pack = packet_queue.front();
  deque<packet_cptr>::const_iterator packet = packet_queue.begin();

  ++packet;
  while ((packet_queue.end() != packet) &&
         ((*packet)->timecode_before_factory <= pack->timecode_before_factory))
    ++packet;
  return (packet_queue.end() != packet);
}

packet_cptr
generic_packetizer_c::get_packet() {
  if ((packet_queue.size() == 0) || !packet_queue.front()->factory_applied)
    return packet_cptr(NULL);

  packet_cptr pack = packet_queue.front();
  packet_queue.pop_front();

  enqueued_bytes -= pack->data->get_size();

  return pack;
}

void
generic_packetizer_c::apply_factory_once(packet_cptr &packet) {
  if (NULL == timecode_factory.get()) {
    packet->assigned_timecode = packet->timecode;
    packet->gap_following = false;
  } else
    packet->gap_following = timecode_factory->get_next(packet);
  packet->factory_applied = true;

  mxverb(4, "apply_factory_once(): source " LLD " t " LLD " tbf " LLD " at "
         LLD "\n", packet->source->get_source_track_num(), packet->timecode,
         packet->timecode_before_factory, packet->assigned_timecode);

  if (max_timecode_seen < (packet->assigned_timecode + packet->duration))
    max_timecode_seen = packet->assigned_timecode + packet->duration;
  if (reader->max_timecode_seen < max_timecode_seen)
    reader->max_timecode_seen = max_timecode_seen;
}

void
generic_packetizer_c::apply_factory() {
  packet_cptr_di p_start;

  if (packet_queue.empty())
    return;

  // Find the first packet to which the factory hasn't been applied yet.
  p_start = packet_queue.begin();
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
  packet_cptr_di p_end, p_current;

  while (packet_queue.end() != p_start) {
    // Find the next packet with a timecode bigger than the start packet's
    // timecode. All packets between those two including the start packet
    // and excluding the end packet can be timestamped.
    p_end = p_start + 1;
    while ((packet_queue.end() != p_end) &&
           ((*p_end)->timecode_before_factory <
            (*p_start)->timecode_before_factory))
      ++p_end;

    // Abort if no such packet was found, but keep on assigning if the
    // packetizer has been flushed already.
    if (!has_been_flushed && (packet_queue.end() == p_end))
      return;

    // Now assign timecodes to the ones between p_start and p_end...
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

  packet_sorter_t(int index):
    m_index(index) {}

  bool operator <(const packet_sorter_t &cmp) const {
    return (*m_packet_queue)[m_index]->timecode <
      (*m_packet_queue)[cmp.m_index]->timecode;
  }
};

deque<packet_cptr> *packet_sorter_t::m_packet_queue = NULL;

void
generic_packetizer_c::apply_factory_full_queueing(packet_cptr_di &p_start) {
  packet_cptr_di p_end, p_current;
  vector<packet_sorter_t> sorter;
  bool needs_sorting;
  int64_t previous_timecode;
  int i;

  packet_sorter_t::m_packet_queue = &packet_queue;

  while (packet_queue.end() != p_start) {
    // Find the next I frame packet.
    p_end = p_start + 1;
    while ((packet_queue.end() != p_end) &&
           ((0 <= (*p_end)->fref) || (0 <= (*p_end)->bref)))
      ++p_end;

    // Abort if no such packet was found, but keep on assigning if the
    // packetizer has been flushed already.
    if (!has_been_flushed && (packet_queue.end() == p_end))
      return;

    // Now sort the frames by their timecode as the factory has to be
    // applied to the packets in the same order as they're timestamped.
    sorter.clear();
    needs_sorting = false;
    previous_timecode = 0;
    i = distance(packet_queue.begin(), p_start);
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
generic_packetizer_c::displace(float by_ns) {
  ti.async.displacement += (int64_t)by_ns;
  if (initial_displacement < 0) {
    if (ti.async.displacement < initial_displacement) {
      mxverb(3, "EODIS 1 by %f dis is now " LLD " with idis " LLD "\n",
             by_ns, ti.async.displacement, initial_displacement);
      initial_displacement = 0;
    }
  } else if (iabs(initial_displacement - ti.async.displacement) <
             (by_ns / 2)) {
    mxverb(3, "EODIS 2 by %f dis is now " LLD " with idis " LLD "\n",
           by_ns, ti.async.displacement, initial_displacement);
    initial_displacement = 0;
  }
}

void
generic_packetizer_c::force_duration_on_last_packet() {
  if (packet_queue.empty()) {
    mxverb(2, "force_duration_on_last_packet: packet queue is empty for "
           "'%s'/" LLD "\n", ti.fname.c_str(), ti.id);
    return;
  }
  packet_cptr &packet = packet_queue.back();
  packet->duration_mandatory = true;
  mxverb(2, "force_duration_on_last_packet: forcing at " FMT_TIMECODE " with "
         "%.3fms for '%s'/" LLD "\n", ARG_TIMECODE_NS(packet->timecode),
         packet->duration / 1000.0, ti.fname.c_str(), ti.id);
}

int64_t
generic_packetizer_c::handle_avi_audio_sync(int64_t num_bytes,
                                            bool vbr) {
  double samples;
  int64_t block_size, duration;
  int i, cur_bytes;

  if ((ti.avi_samples_per_sec == 0) || (ti.avi_block_align == 0) ||
      (ti.avi_avg_bytes_per_sec == 0) || !ti.avi_audio_sync_enabled) {
    enable_avi_audio_sync(false);
    return -1;
  }

  if (!vbr)
     duration = num_bytes * 1000000000 / ti.avi_avg_bytes_per_sec;
  else {
    samples = 0.0;
    for (i = 0; (i < ti.avi_block_sizes.size()) && (num_bytes > 0); i++) {
      block_size = ti.avi_block_sizes[i];
      cur_bytes = num_bytes < block_size ? num_bytes : block_size;
      samples += (double)ti.avi_samples_per_chunk * cur_bytes /
        ti.avi_block_align;
      num_bytes -= cur_bytes;
    }
    duration = (int64_t)(samples * 1000000000 / ti.avi_samples_per_sec);
  }

  enable_avi_audio_sync(false);
  initial_displacement += duration;

  return duration;
}

void
generic_packetizer_c::connect(generic_packetizer_c *src,
                              int64_t _append_timecode_offset) {
  m_free_refs = src->m_free_refs;
  m_next_free_refs = src->m_next_free_refs;
  track_entry = src->track_entry;
  hserialno = src->hserialno;
  htrack_type = src->htrack_type;
  htrack_default_duration = src->htrack_default_duration;
  huid  = src->huid;
  hcompression = src->hcompression;
  compressor = compressor_c::create(hcompression);
  last_cue_timecode = src->last_cue_timecode;
  timecode_factory = src->timecode_factory;
  correction_timecode_offset = 0;
  if (_append_timecode_offset == -1)
    append_timecode_offset = src->max_timecode_seen;
  else
    append_timecode_offset = _append_timecode_offset;
  connected_to++;
  if (connected_to == 2)
    process_deferred_packets();
}

void
generic_packetizer_c::set_displacement_maybe(int64_t displacement) {
  if ((ti.async.linear != 1.0) || (ti.async.displacement != 0) ||
      (initial_displacement != 0))
    return;
  initial_displacement = displacement;
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

#define add_all_requested_track_ids(type, container) \
  for (map<int64_t, type>::const_iterator i = ti.container.begin(); \
       ti.container.end() != i; ++i) \
    add_requested_track_id(i->first);
#define add_all_requested_track_ids2(container) \
  for (int i = 0; i < ti.container.size(); i++) \
    add_requested_track_id(ti.container[i]);

generic_reader_c::generic_reader_c(track_info_c &_ti):
  ti(_ti),
  ptzr_first_packet(NULL),
  max_timecode_seen(0),
  chapters(NULL),
  appending(false),
  num_video_tracks(0), num_audio_tracks(0), num_subtitle_tracks(0),
  reference_timecode_tolerance(0) {

  add_all_requested_track_ids2(atracks);
  add_all_requested_track_ids2(vtracks);
  add_all_requested_track_ids2(stracks);
  add_all_requested_track_ids2(btracks);
  add_all_requested_track_ids(string, all_fourccs);
  add_all_requested_track_ids(display_properties_t, display_properties);
  add_all_requested_track_ids(audio_sync_t, audio_syncs);
  add_all_requested_track_ids(cue_strategy_e, cue_creations);
  add_all_requested_track_ids2(default_track_flags);
  add_all_requested_track_ids(string, languages);
  add_all_requested_track_ids(string, sub_charsets);
  add_all_requested_track_ids(string, all_tags);
  add_all_requested_track_ids(bool, all_aac_is_sbr);
  add_all_requested_track_ids(int64_t, packet_delays);
  add_all_requested_track_ids(compression_method_e, compression_list);
  add_all_requested_track_ids(string, track_names);
  add_all_requested_track_ids(string, all_ext_timecodes);
  add_all_requested_track_ids(pixel_crop_t, pixel_crop_list);
}

generic_reader_c::~generic_reader_c() {
  int i;

  for (i = 0; i < reader_packetizers.size(); i++)
    delete reader_packetizers[i];
  if (chapters != NULL)
    delete chapters;
}

void
generic_reader_c::read_all() {
  int i;

  for (i = 0; i < reader_packetizers.size(); i++) {
    while (read(reader_packetizers[i], true) != 0)
      ;
  }
}

bool
generic_reader_c::demuxing_requested(char type,
                                     int64_t id) {
  vector<int64_t> *tracks;
  int i;

  tracks = NULL;
  if (type == 'v') {
    if (ti.no_video)
      return false;
    tracks = &ti.vtracks;
  } else if (type == 'a') {
    if (ti.no_audio)
      return false;
    tracks = &ti.atracks;
  } else if (type == 's') {
    if (ti.no_subs)
      return false;
    tracks = &ti.stracks;
  } else if (type == 'b') {
    if (ti.no_buttons)
      return false;
    tracks = &ti.btracks;
  } else
    die("pr_generic.cpp/generic_reader_c::demuxing_requested(): Invalid track "
        "type %c.", type);

  if (tracks->size() == 0)
    return true;

  for (i = 0; i < tracks->size(); i++)
    if ((*tracks)[i] == id)
      return true;

  return false;
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
  vector<generic_packetizer_c *>::const_iterator it;

  max_timecode_seen = offset;
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
  int r, a;
  bool found;

  add_available_track_ids();
  if (reader_packetizers.size() == 0)
    mxwarn(FMT_FN "No tracks will be copied from this file. This usually "
           "indicates a mistake in the command line.\n", ti.fname.c_str());

  for (r = 0; r < requested_track_ids.size(); r++) {
    found = false;
    for (a = 0; a < available_track_ids.size(); a++)
      if (requested_track_ids[r] == available_track_ids[a]) {
        found = true;
        break;
      }

    if (!found)
      mxwarn(FMT_FN "A track with the ID " LLD " was requested but not found "
             "in the file. The corresponding option will be ignored.\n",
             ti.fname.c_str(), requested_track_ids[r]);
  }
}

void
generic_reader_c::add_requested_track_id(int64_t id) {
  int i;
  bool found;

  if (id == -1)
    return;

  found = false;
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
  vector<generic_packetizer_c *>::const_iterator it;
  int64_t bytes;

  bytes = 0;
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

//
//--------------------------------------------------------------------

track_info_c::track_info_c():
  initialized(true),
  id(0),
  no_audio(false),
  no_video(false),
  no_subs(false),
  no_buttons(false),
  private_data(NULL),
  private_size(0),
  aspect_ratio(0.0),
  display_width(0),
  display_height(0),
  aspect_ratio_given(false),
  aspect_ratio_is_factor(false),
  display_dimensions_given(false),
  cues(CUE_STRATEGY_UNSPECIFIED),
  default_track(false),
  tags(NULL),
  aac_is_sbr(-1),
  packet_delay(0),
  compression(COMPRESSION_UNSPECIFIED),
  pixel_cropping_specified(false),
  stereo_mode(STEREO_MODE_UNSPECIFIED),
  no_chapters(false),
  no_attachments(false),
  no_tags(false),
  avi_audio_sync_enabled(false) {

  async.linear = 1.0;
  async.displacement = 0;
  memset(&pixel_cropping, 0, sizeof(pixel_crop_t));
}

void
track_info_c::free_contents() {
  if (!initialized)
    return;

  safefree(private_data);
  if (tags != NULL)
    delete tags;

  initialized = false;
}

track_info_c &
track_info_c::operator =(const track_info_c &src) {
  free_contents();

  id = src.id;
  fname = src.fname;

  no_audio = src.no_audio;
  no_video = src.no_video;
  no_subs = src.no_subs;
  no_buttons = src.no_buttons;
  atracks = src.atracks;
  btracks = src.btracks;
  stracks = src.stracks;
  vtracks = src.vtracks;

  private_size = src.private_size;
  private_data = (unsigned char *)safememdup(src.private_data, private_size);

  all_fourccs = src.all_fourccs;

  display_properties = src.display_properties;
  aspect_ratio = src.aspect_ratio;
  aspect_ratio_given = false;
  aspect_ratio_is_factor = false;
  display_dimensions_given = false;

  audio_syncs = src.audio_syncs;
  memcpy(&async, &src.async, sizeof(audio_sync_t));

  cue_creations = src.cue_creations;
  cues = src.cues;

  default_track_flags = src.default_track_flags;
  default_track = src.default_track;

  languages = src.languages;
  language = src.language;

  sub_charsets = src.sub_charsets;
  sub_charset = src.sub_charset;

  all_tags = src.all_tags;
  tags_file_name = src.tags_file_name;
  if (src.tags != NULL)
    tags = static_cast<KaxTags *>(src.tags->Clone());
  else
    tags = NULL;

  all_aac_is_sbr = src.all_aac_is_sbr;
  aac_is_sbr = src.aac_is_sbr;

  packet_delay = src.packet_delay;
  packet_delays = src.packet_delays;

  compression_list = src.compression_list;
  compression = src.compression;

  track_names = src.track_names;
  track_name = src.track_name;

  all_ext_timecodes = src.all_ext_timecodes;
  ext_timecodes = src.ext_timecodes;

  pixel_crop_list = src.pixel_crop_list;
  pixel_cropping = src.pixel_cropping;
  pixel_cropping_specified = src.pixel_cropping_specified;

  stereo_mode_list = src.stereo_mode_list;
  stereo_mode = src.stereo_mode;

  no_chapters = src.no_chapters;
  no_attachments = src.no_attachments;
  no_tags = src.no_tags;

  chapter_charset = src.chapter_charset;

  avi_block_align = src.avi_block_align;
  avi_samples_per_sec = src.avi_samples_per_sec;
  avi_avg_bytes_per_sec = src.avi_avg_bytes_per_sec;
  avi_samples_per_chunk = src.avi_samples_per_chunk;
  avi_block_sizes.clear();
  avi_audio_sync_enabled = false;

  default_durations = src.default_durations;
  max_blockadd_ids = src.max_blockadd_ids;

  initialized = true;

  return *this;
}
