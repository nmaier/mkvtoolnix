/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  pr_generic.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief functions common for all readers/packetizers
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <matroska/KaxContentEncoding.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>

#include <map>
#include <string>

#include "base64.h"
#include "common.h"
#include "commonebml.h"
#include "compression.h"
#include "mkvmerge.h"
#include "pr_generic.h"
#include "tagparser.h"

using namespace std;

generic_packetizer_c::generic_packetizer_c(generic_reader_c *nreader,
                                           track_info_c *nti) throw(error_c) {
  int i;
  audio_sync_t *as;
  cue_creation_t *cc;
  int64_t id;
  language_t *lang;
  tags_t *tags;
  display_properties_t *dprop;
  fourcc_t *fourcc;
  bool found;

#ifdef DEBUG
  debug_facility.add_packetizer(this);
#endif
  reader = nreader;
  add_packetizer(this);
  duplicate_data = true;

  track_entry = NULL;
  ti = new track_info_c(*nti);
  free_refs = -1;
  enqueued_bytes = 0;
  safety_last_timecode = 0;

  // Let's see if the user specified audio sync for this track.
  found = false;
  for (i = 0; i < ti->audio_syncs->size(); i++) {
    as = &(*ti->audio_syncs)[i];
    if ((as->id == ti->id) || (as->id == -1)) { // -1 == all tracks
      found = true;
      ti->async = *as;
      break;
    }
  }
  if (!found && (ti->async.linear == 0.0)) {
    ti->async.linear = 1.0;
    ti->async.displacement = 0;
  }
  initial_displacement = ti->async.displacement;
  ti->async.displacement = 0;

  // Let's see if the user has specified which cues he wants for this track.
  ti->cues = CUES_UNSPECIFIED;
  for (i = 0; i < ti->cue_creations->size(); i++) {
    cc = &(*ti->cue_creations)[i];
    if ((cc->id == ti->id) || (cc->id == -1)) { // -1 == all tracks
      ti->cues = cc->cues;
      break;
    }
  }

  // Let's see if the user has given a default track flag for this track.
  for (i = 0; i < ti->default_track_flags->size(); i++) {
    id = (*ti->default_track_flags)[i];
    if ((id == ti->id) || (id == -1)) { // -1 == all tracks
      ti->default_track = true;
      break;
    }
  }

  // Let's see if the user has specified a language for this track.
  for (i = 0; i < ti->languages->size(); i++) {
    lang = &(*ti->languages)[i];
    if ((lang->id == ti->id) || (lang->id == -1)) { // -1 == all tracks
      safefree(ti->language);
      ti->language = safestrdup(lang->language);
      break;
    }
  }

  // Let's see if the user has specified a sub charset for this track.
  for (i = 0; i < ti->sub_charsets->size(); i++) {
    lang = &(*ti->sub_charsets)[i];
    if ((lang->id == ti->id) || (lang->id == -1)) { // -1 == all tracks
      safefree(ti->sub_charset);
      ti->sub_charset = safestrdup(lang->language);
      break;
    }
  }

  // Let's see if the user has specified a sub charset for this track.
  for (i = 0; i < ti->all_tags->size(); i++) {
    tags = &(*ti->all_tags)[i];
    if ((tags->id == ti->id) || (tags->id == -1)) { // -1 == all tracks
      ti->tags_ptr = tags;
      ti->tags = new KaxTags;
      parse_xml_tags(ti->tags_ptr->file_name, ti->tags);
      break;
    }
  }

  // Let's see if the user has specified how this track should be compressed.
  ti->compression = COMPRESSION_UNSPECIFIED;
  for (i = 0; i < ti->compression_list->size(); i++) {
    cc = &(*ti->compression_list)[i];
    if ((cc->id == ti->id) || (cc->id == -1)) { // -1 == all tracks
      ti->compression = cc->cues;
      break;
    }
  }

  // Let's see if the user has specified a name for this track.
  for (i = 0; i < ti->track_names->size(); i++) {
    lang = &(*ti->track_names)[i];
    if ((lang->id == ti->id) || (lang->id == -1)) { // -1 == all tracks
      safefree(ti->track_name);
      ti->track_name = safestrdup(lang->language);
      break;
    }
  }

  // Let's see if the user has specified external timecodes for this track.
  for (i = 0; i < ti->all_ext_timecodes->size(); i++) {
    lang = &(*ti->all_ext_timecodes)[i];
    if ((lang->id == ti->id) || (lang->id == -1)) { // -1 == all tracks
      safefree(ti->ext_timecodes);
      ti->ext_timecodes = safestrdup(lang->language);
      break;
    }
  }

  // Let's see if the user has specified an aspect ratio or display dimensions
  // for this track.
  for (i = 0; i < ti->display_properties->size(); i++) {
    dprop = &(*ti->display_properties)[i];
    if ((dprop->id == ti->id) || (dprop->id == -1)) { // -1 == all tracks
      if (dprop->aspect_ratio < 0) {
        ti->display_width = dprop->width;
        ti->display_height = dprop->height;
        ti->display_dimensions_given = true;
      } else {
        ti->aspect_ratio = dprop->aspect_ratio;
        ti->aspect_ratio_given = true;
      }
    }
  }
  if (ti->aspect_ratio_given && ti->display_dimensions_given)
    mxerror("Both '--aspect-ratio' and '--display-dimensions' were given for "
            "'%s'.\n", ti->fname);

  memset(ti->fourcc, 0, 5);
  // Let's see if the user has specified a FourCC for this track.
  for (i = 0; i < ti->all_fourccs->size(); i++) {
    fourcc = &(*ti->all_fourccs)[i];
    if ((fourcc->id == ti->id) || (fourcc->id == -1)) { // -1 == all tracks
      memcpy(ti->fourcc, fourcc->fourcc, 4);
      ti->fourcc[4] = 0;
      break;
    }
  }

  // Set default header values to 'unset'.
  hserialno = track_number++;
  huid = 0;
  htrack_type = -1;
  htrack_min_cache = -1;
  htrack_max_cache = -1;
  htrack_default_duration = -1;

  hcodec_id = NULL;
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
  compressor = NULL;

  dumped_packet_number = 0;

  frameno = 0;
  ext_timecodes_version = -1;
  ext_timecodes = NULL;
  current_tc_range = 0;
  if (ti->ext_timecodes != NULL)
    parse_ext_timecode_file(ti->ext_timecodes);
}

generic_packetizer_c::~generic_packetizer_c() {
  delete ti;

  safefree(hcodec_id);
  safefree(hcodec_private);
  if (compressor != NULL)
    delete compressor;
  if (ext_timecodes != NULL)
    delete ext_timecodes;
}

void generic_packetizer_c::set_tag_track_uid() {
  int is, it;
  KaxTag *tag;
  KaxTagTargets *targets;
  EbmlElement *el;

  if (ti->tags == NULL)
    return;

  for (is = 0; is < ti->tags->ListSize(); is++) {
    tag = (KaxTag *)(*ti->tags)[is];

    for (it = 0; it < tag->ListSize(); it++) {
      el = (*tag)[it];
      if (el->Generic().GlobalId == KaxTagTargets::ClassInfos.GlobalId) {
        tag->Remove(it);
        delete el;
        it--;
        continue;
      }
    }

    targets = &GetChild<KaxTagTargets>(*tag);
    *(static_cast<EbmlUInteger *>(&GetChild<KaxTagTrackUID>(*targets))) =
      huid;

    if (!tag->CheckMandatory())
      mxerror("Could not parse the tags in '%s': some mandatory "
              "elements are missing.\n", ti->tags_ptr->file_name);
  }
}

int generic_packetizer_c::read() {
  return reader->read(this);
}

void generic_packetizer_c::reset() {
  track_entry = NULL;
}

void generic_packetizer_c::set_cue_creation(int ncreate_cue_data) {
  ti->cues = ncreate_cue_data;
}

int generic_packetizer_c::get_cue_creation() {
  return ti->cues;
}

void generic_packetizer_c::set_free_refs(int64_t nfree_refs) {
  free_refs = nfree_refs;
}

int64_t generic_packetizer_c::get_free_refs() {
  return free_refs;
}

KaxTrackEntry *generic_packetizer_c::get_track_entry() {
  return track_entry;
}

int generic_packetizer_c::set_uid(uint32_t uid) {
  if (is_unique_uint32(uid)) {
    add_unique_uint32(uid);
    huid = uid;
    if (track_entry != NULL)
      *(static_cast<EbmlUInteger *>(&GetChild<KaxTrackUID>(*track_entry))) =
        huid;
    return 1;
  }

  return 0;
}

void generic_packetizer_c::set_track_type(int type) {
  htrack_type = type;

  if (ti->default_track)
    force_default_track(type);
  else
    set_as_default_track(type);
}

int generic_packetizer_c::get_track_type() {
  return htrack_type;
}

int generic_packetizer_c::get_track_num() {
  return hserialno;
}

void generic_packetizer_c::set_track_name(const char *name) {
  safefree(ti->track_name);
  if (name == NULL) {
    ti->track_name = NULL;
    return;
  }
  ti->track_name = safestrdup(name);
  if (track_entry != NULL)
    *(static_cast<EbmlUnicodeString *>
      (&GetChild<KaxTrackName>(*track_entry))) =
      cstrutf8_to_UTFstring(ti->track_name);
}

void generic_packetizer_c::set_codec_id(const char *id) {
  safefree(hcodec_id);
  if (id == NULL) {
    hcodec_id = NULL;
    return;
  }
  hcodec_id = safestrdup(id);
  if (track_entry != NULL)
    *(static_cast<EbmlString *>
      (&GetChild<KaxCodecID>(*track_entry))) = hcodec_id;
}

void generic_packetizer_c::set_codec_private(const unsigned char *cp,
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

void generic_packetizer_c::set_track_min_cache(int min_cache) {
  htrack_min_cache = min_cache;
  if (track_entry != NULL)
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxTrackMinCache>(*track_entry))) = min_cache;
}

void generic_packetizer_c::set_track_max_cache(int max_cache) {
  htrack_max_cache = max_cache;
  if (track_entry != NULL)
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxTrackMinCache>(*track_entry))) = max_cache;
}

void generic_packetizer_c::set_track_default_duration_ns(int64_t def_dur) {
  htrack_default_duration = def_dur;
  if (track_entry != NULL)
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxTrackDefaultDuration>(*track_entry))) =
      htrack_default_duration;
}

int64_t generic_packetizer_c::get_track_default_duration_ns() {
  return htrack_default_duration;
}

void generic_packetizer_c::set_audio_sampling_freq(float freq) {
  haudio_sampling_freq = freq;
  if (track_entry != NULL) {
    KaxTrackAudio &audio = GetChild<KaxTrackAudio>(*track_entry);
    *(static_cast<EbmlFloat *>(&GetChild<KaxAudioSamplingFreq>(audio))) =
      haudio_sampling_freq;
  }
}

void generic_packetizer_c::set_audio_output_sampling_freq(float freq) {
  haudio_output_sampling_freq = freq;
  if (track_entry != NULL) {
    KaxTrackAudio &audio = GetChild<KaxTrackAudio>(*track_entry);
    *(static_cast<EbmlFloat *>(&GetChild<KaxAudioOutputSamplingFreq>(audio))) =
      haudio_output_sampling_freq;
  }
}

void generic_packetizer_c::set_audio_channels(int channels) {
  haudio_channels = channels;
  if (track_entry != NULL) {
    KaxTrackAudio &audio = GetChild<KaxTrackAudio>(*track_entry);
    *(static_cast<EbmlUInteger *>(&GetChild<KaxAudioChannels>(audio))) =
      haudio_channels;
  }
}

void generic_packetizer_c::set_audio_bit_depth(int bit_depth) {
  haudio_bit_depth = bit_depth;
  if (track_entry != NULL) {
    KaxTrackAudio &audio = GetChild<KaxTrackAudio>(*track_entry);
    *(static_cast<EbmlUInteger *>(&GetChild<KaxAudioBitDepth>(audio))) =
      haudio_bit_depth;
  }
}

void generic_packetizer_c::set_video_pixel_width(int width) {
  hvideo_pixel_width = width;
  if (track_entry != NULL) {
    KaxTrackVideo &video = GetChild<KaxTrackVideo>(*track_entry);
    *(static_cast<EbmlUInteger *>(&GetChild<KaxVideoPixelWidth>(video))) =
      hvideo_pixel_width;
  }
}

void generic_packetizer_c::set_video_pixel_height(int height) {
  hvideo_pixel_height = height;
  if (track_entry != NULL) {
    KaxTrackVideo &video = GetChild<KaxTrackVideo>(*track_entry);
    *(static_cast<EbmlUInteger *>(&GetChild<KaxVideoPixelHeight>(video))) =
      hvideo_pixel_height;
  }
}

void generic_packetizer_c::set_video_display_width(int width) {
  hvideo_display_width = width;
  if (track_entry != NULL) {
    KaxTrackVideo &video = GetChild<KaxTrackVideo>(*track_entry);
    *(static_cast<EbmlUInteger *>(&GetChild<KaxVideoDisplayWidth>(video))) =
      hvideo_display_width;
  }
}

void generic_packetizer_c::set_video_display_height(int height) {
  hvideo_display_height = height;
  if (track_entry != NULL) {
    KaxTrackVideo &video = GetChild<KaxTrackVideo>(*track_entry);
    *(static_cast<EbmlUInteger *>(&GetChild<KaxVideoDisplayHeight>(video))) =
      hvideo_display_height;
  }
}

void generic_packetizer_c::set_video_aspect_ratio(float ar) {
  ti->aspect_ratio = ar;
}

void generic_packetizer_c::set_as_default_track(int type) {
  int idx;

  idx = 0;
  if (type == track_audio)
    idx = 0;
  else if (type == track_video)
    idx = 1;
  else if (type == track_subtitle)
    idx = 2;
  else
    die("pr_generic.cpp/generic_packetizer_c::set_as_default_track(): Unknown "
        "track type %d.", type);

  if (default_tracks[idx] == 0)
    default_tracks[idx] = -1 * hserialno;
}

void generic_packetizer_c::force_default_track(int type) {
  int idx;

  idx = 0;
  if (type == track_audio)
    idx = 0;
  else if (type == track_video)
    idx = 1;
  else if (type == track_subtitle)
    idx = 2;
  else
    die("pr_generic.cpp/generic_packetizer_c::force_default_track(): Unknown "
        "track type %d.", type);

  if ((default_tracks[idx] > 0) && !identifying)
    mxwarn("Another default track for %s tracks has already "
           "been set. Not setting the 'default' flag for this track.\n",
           idx == 0 ? "audio" : idx == 'v' ? "video" : "subtitle");
  else
    default_tracks[idx] = hserialno;
}

void generic_packetizer_c::set_language(const char *language) {
  safefree(ti->language);
  ti->language = safestrdup(language);
  if (track_entry != NULL)
    *(static_cast<EbmlString *>
      (&GetChild<KaxTrackLanguage>(*track_entry))) = ti->language;
}

void generic_packetizer_c::set_default_compression_method(int method) {
  hcompression = method;
}

void generic_packetizer_c::set_headers() {
  int idx, disp_width, disp_height;
  KaxTag *tag;

  if (track_entry == NULL) {
    if (kax_last_entry == NULL)
      track_entry = &GetChild<KaxTrackEntry>(*kax_tracks);
    else
      track_entry =
        &GetNextChild<KaxTrackEntry>(*kax_tracks, *kax_last_entry);
    kax_last_entry = track_entry;
    track_entry->SetGlobalTimecodeScale(TIMECODE_SCALE);
  }

  KaxTrackNumber &tnumber = GetChild<KaxTrackNumber>(*track_entry);
  *(static_cast<EbmlUInteger *>(&tnumber)) = hserialno;

  if (huid == 0)
    huid = create_unique_uint32();

  KaxTrackUID &tuid = GetChild<KaxTrackUID>(*track_entry);
  *(static_cast<EbmlUInteger *>(&tuid)) = huid;

  if (htrack_type != -1)
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxTrackType>(*track_entry))) = htrack_type;

  if (hcodec_id != NULL)
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

  if (htrack_default_duration != -1.0)
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxTrackDefaultDuration>(*track_entry))) =
      htrack_default_duration;

  if (htrack_type == track_audio)
    idx = 0;
  else if (htrack_type == track_video)
    idx = 1;
  else
    idx = 2;

  if ((default_tracks[idx] == hserialno) ||
      (default_tracks[idx] == -1 * hserialno))
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxTrackFlagDefault>(*track_entry))) = 1;
  else
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxTrackFlagDefault>(*track_entry))) = 0;

  if (ti->language != NULL)
    *(static_cast<EbmlString *>
      (&GetChild<KaxTrackLanguage>(*track_entry))) = ti->language;

  if (ti->track_name != NULL)
    *(static_cast<EbmlUnicodeString *>
      (&GetChild<KaxTrackName>(*track_entry))) =
      cstrutf8_to_UTFstring(ti->track_name);
  
  if (htrack_type == track_video) {
    KaxTrackVideo &video =
      GetChild<KaxTrackVideo>(*track_entry);

    if ((hvideo_pixel_height != -1) && (hvideo_pixel_width != -1)) {
      if ((hvideo_display_width == -1) || (hvideo_display_height == -1) ||
          ti->aspect_ratio_given || ti->display_dimensions_given) {
        if (ti->display_dimensions_given) {
          disp_width = ti->display_width;
          disp_height = ti->display_height;

        } else {
          if (!ti->aspect_ratio_given)
            ti->aspect_ratio = (float)hvideo_pixel_width /
              (float)hvideo_pixel_height;
          if (ti->aspect_ratio >
              ((float)hvideo_pixel_width / (float)hvideo_pixel_height)) {
            disp_width = myrnd(hvideo_pixel_height * ti->aspect_ratio);
            disp_height = hvideo_pixel_height;

          } else {
            disp_width = hvideo_pixel_width;
            disp_height = myrnd(hvideo_pixel_width / ti->aspect_ratio);

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

      KaxVideoDisplayHeight &dheight =
        GetChild<KaxVideoDisplayHeight>(video);
      *(static_cast<EbmlUInteger *>(&dheight)) = disp_height;
      dheight.SetDefaultSize(4);
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

  }

  if (ti->compression != COMPRESSION_UNSPECIFIED)
    hcompression = ti->compression;
  if ((hcompression != COMPRESSION_UNSPECIFIED) &&
      (hcompression != COMPRESSION_NONE)) {
    KaxContentEncodings *c_encodings;
    KaxContentEncoding *c_encoding;
    KaxContentCompression *c_comp;

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

    c_comp = &GetChild<KaxContentCompression>(*c_encoding);
    // Set compression method.
    *static_cast<EbmlUInteger *>(&GetChild<KaxContentCompAlgo>(*c_comp)) =
      hcompression - 1;

    compressor = compression_c::create(hcompression);
  }

  if (no_lacing)
    track_entry->EnableLacing(false);

  set_tag_track_uid();
  if (ti->tags != NULL) {
    while (ti->tags->ListSize() != 0) {
      tag = (KaxTag *)(*ti->tags)[0];
      add_tags(tag);
      ti->tags->Remove(0);
    }
  }
}

void generic_packetizer_c::duplicate_data_on_add(bool duplicate) {
  duplicate_data = duplicate;
}

void generic_packetizer_c::add_packet(unsigned char  *data, int length,
                                      int64_t timecode,
                                      int64_t duration,
                                      bool duration_mandatory,
                                      int64_t bref, int64_t fref,
                                      int ref_priority,
                                      copy_packet_mode_t copy_this) {
  packet_t *pack;

  if (data == NULL)
    return;
  if (timecode < 0) {
    drop_packet(data, copy_this);
    return;
  }
  // 'timecode < safety_last_timecode' may only occur for B frames. In this
  // case we have the coding order, e.g. IPB1B2 and the timecodes
  // I: 0, P: 120, B1: 40, B2: 80.
  if ((timecode < safety_last_timecode) && (fref < 0))
    mxwarn("pr_generic.cpp/generic_packetizer_c::add_packet(): timecode < "
           "last_timecode (%lld < %lld)", timecode, safety_last_timecode);
  safety_last_timecode = timecode;

  pack = (packet_t *)safemalloc(sizeof(packet_t));
  memset(pack, 0, sizeof(packet_t));

  if (compressor != NULL) {
    pack->data = compressor->compress(data, length);
    if ((copy_this == cp_no) ||
        ((copy_this == cp_default) && !duplicate_data))
      safefree(data);
  } else if ((copy_this == cp_yes) ||
             ((copy_this == cp_default) && duplicate_data)) {
    if (!fast_mode)
      pack->data = (unsigned char *)safememdup(data, length);
    else
      pack->data = (unsigned char *)safemalloc(length);
  } else
    pack->data = data;
  pack->length = length;
  pack->timecode = timecode;
  pack->bref = bref;
  pack->fref = fref;
  pack->ref_priority = ref_priority;
  pack->duration = duration;
  pack->duration_mandatory = duration_mandatory;
  pack->source = this;
  pack->assigned_timecode = get_next_timecode(timecode);

  packet_queue.push_back(pack);

  enqueued_bytes += pack->length;
}

void generic_packetizer_c::drop_packet(unsigned char *data,
                                       copy_packet_mode_t copy_this) {
  if ((copy_this == cp_no) ||
      ((copy_this == cp_default) && !duplicate_data))
    safefree(data);
}

packet_t *generic_packetizer_c::get_packet() {
  packet_t *pack;

  if (packet_queue.size() == 0)
    return NULL;

  pack = packet_queue.front();
  packet_queue.pop_front();

  enqueued_bytes -= pack->length;

  return pack;
}

int generic_packetizer_c::packet_available() {
  return packet_queue.size();
}

int64_t generic_packetizer_c::get_smallest_timecode() {
  if (packet_queue.size() == 0)
    return 0x0FFFFFFF;

  return packet_queue.front()->timecode;
}

int64_t generic_packetizer_c::get_queued_bytes() {
  return enqueued_bytes;
}

void generic_packetizer_c::dump_packet(const void *buffer, int size) {
  char *path;
  mm_io_c *out;

  if (dump_packets == NULL)
    return;

  path = (char *)safemalloc(strlen(dump_packets) + 1 + 30);
  mxprints(path, "%s/%u-%010lld", dump_packets, hserialno,
           dumped_packet_number);
  dumped_packet_number++;
  try {
    out = new mm_io_c(path, MODE_CREATE);
    out->write(buffer, size);
    delete out;
  } catch(...) {
  }
  safefree(path);
}

void generic_packetizer_c::parse_ext_timecode_file(const char *name) {
  mm_io_c *in;
  string line;
  timecode_range_c t;
  vector<string> fields;
  vector<timecode_range_c>::iterator iit, pit;
  uint32_t i, line_no;
  bool done;
  double default_fps;

  in = NULL;
  try {
    in = new mm_text_io_c(name);
  } catch(...) {
    mxerror("Could not open the timecode file '%s' for reading.\n", name);
  }

  if (!in->getline2(line) || !starts_with_case(line, "# timecode format v") ||
      !parse_int(&line.c_str()[strlen("# timecode format v")],
                 ext_timecodes_version))
    mxerror("The timcode file '%s' contains an unsupported/unrecognized "
            "format line. The very first line should be '# timecode format "
            "v1'.\n", name);
  if (ext_timecodes_version != 1)
    mxerror("The timcode file '%s' contains an unsupported/unrecognized "
            "format (version %d).\n", name, ext_timecodes_version);
  line_no = 1;
  do {
    if (!in->getline2(line))
      mxerror("The timcode file '%s' does not contain a valie 'Assume' line "
              "with the default FPS.\n", name);
    line_no++;
    strip(line);
    if ((line.length() != 0) && (line[0] != '#'))
      break;
  } while (true);
  if (!starts_with_case(line, "assume "))
    mxerror("The timcode file '%s' does not contain a valie 'Assume' line "
            "with the default FPS.\n", name);
  line.erase(0, 6);
  strip(line);
  if (!parse_double(line.c_str(), default_fps))
    mxerror("The timcode file '%s' does not contain a valie 'Assume' line "
            "with the default FPS.\n", name);
  ext_timecodes = new vector<timecode_range_c>;

  while (in->getline2(line)) {
    line_no++;
    strip(line, true);
    if ((line.length() == 0) || (line[0] == '#'))
      continue;

    fields = split(line.c_str());
    strip(fields, true);
    if (fields.size() != 3) {
      mxwarn("Line %d of the timecode file '%s' could not be parsed: number "
             "of fields is not = 3.\n", line_no, name);
      continue;
    }

    if (!parse_int(fields[0].c_str(), t.start_frame)) {
      mxwarn("Line %d of the timecode file '%s' could not be parsed (start "
             "frame).\n", line_no, name);
      continue;
    }
    if (!parse_int(fields[1].c_str(), t.end_frame)) {
      mxwarn("Line %d of the timecode file '%s' could not be parsed (end "
             "frame).\n", line_no, name);
      continue;
    }
    if (!parse_double(fields[2].c_str(), t.fps)) {
      mxwarn("Line %d of the timecode file '%s' could not be parsed (FPS).\n",
             line_no, name);
      continue;
    }

    if ((t.fps <= 0) || (t.start_frame < 0) || (t.end_frame < 0) ||
        (t.end_frame < t.start_frame)) {
      mxwarn("Line %d of the timecode file '%s' contains inconsistent data.\n",
             line_no, name);
      continue;
    }

    ext_timecodes->push_back(t);
  }
  delete in;

  mxverb(3, "ext_timecodes: Version %d, default fps %f, %u entries.\n",
         ext_timecodes_version, default_fps, ext_timecodes->size());

  if (ext_timecodes->size() == 0)
    mxwarn("The timecode file '%s' does not contain any valid entry.\n",
           name);

  sort(ext_timecodes->begin(), ext_timecodes->end());
  if (ext_timecodes->size() > 0) {
    do {
      done = true;
      iit = ext_timecodes->begin();
      for (i = 0; i < (ext_timecodes->size() - 1); i++) {
        iit++;
        if ((*ext_timecodes)[i].end_frame <
            ((*ext_timecodes)[i + 1].start_frame - 1)) {
          t.start_frame = (*ext_timecodes)[i].end_frame + 1;
          t.end_frame = (*ext_timecodes)[i + 1].start_frame - 1;
          t.fps = default_fps;
          ext_timecodes->insert(iit, t);
          done = false;
          break;
        }
      }
    } while (!done);
    if ((*ext_timecodes)[0].start_frame != 0) {
      t.start_frame = 0;
      t.end_frame = (*ext_timecodes)[0].start_frame - 1;
      t.fps = default_fps;
      ext_timecodes->insert(ext_timecodes->begin(), t);
    }
    t.start_frame = (*ext_timecodes)[ext_timecodes->size() - 1].end_frame + 1;
  } else
    t.start_frame = 0;
  t.end_frame = 0xfffffffffffffffll;
  t.fps = default_fps;
  ext_timecodes->push_back(t);

  (*ext_timecodes)[0].base_timecode = 0.0;
  pit = ext_timecodes->begin();
  for (iit = pit + 1; iit < ext_timecodes->end(); iit++, pit++)
    iit->base_timecode = pit->base_timecode +
      ((double)pit->end_frame - (double)pit->start_frame + 1) * 1000.0 /
      pit->fps;

  for (iit = ext_timecodes->begin(); iit < ext_timecodes->end(); iit++)
    mxverb(3, "ext_timecodes: entry %lld -> %lld at %f with %f\n",
           iit->start_frame, iit->end_frame, iit->fps, iit->base_timecode);
}

int64_t generic_packetizer_c::get_next_timecode(int64_t timecode) {
  int64_t new_timecode;
  timecode_range_c *t;

  if (ext_timecodes == NULL)
    return timecode;

  t = &(*ext_timecodes)[current_tc_range];
  new_timecode = (int64_t)(t->base_timecode + 1000.0 *
                           (frameno - t->start_frame) / t->fps);
  frameno++;
  if ((frameno > t->end_frame) &&
      (current_tc_range < (ext_timecodes->size() - 1)))
    current_tc_range++;

  mxverb(4, "ext_timecodes: %lld for %lld\n", new_timecode, frameno - 1);

  return new_timecode;
}

bool generic_packetizer_c::needs_negative_displacement(float duration) {
  return ((initial_displacement < 0) &&
          (ti->async.displacement > initial_displacement));
}

bool generic_packetizer_c::needs_positive_displacement(float duration) {
  return ((initial_displacement > 0) &&
          (iabs(ti->async.displacement - initial_displacement) >
           (duration / 2)));
}

void generic_packetizer_c::displace(float by_ms) {
  ti->async.displacement += (int64_t)by_ms;
  if (initial_displacement < 0) {
    if (ti->async.displacement < initial_displacement)
      initial_displacement = 0;
  } else if (iabs(initial_displacement - ti->async.displacement) <
             (by_ms / 2))
    initial_displacement = 0;
}

//--------------------------------------------------------------------

generic_reader_c::generic_reader_c(track_info_c *nti) {
  ti = new track_info_c(*nti);
}

generic_reader_c::~generic_reader_c() {
  delete ti;
}

bool generic_reader_c::demuxing_requested(char type, int64_t id) {
  vector<int64_t> *tracks;
  int i;

  tracks = NULL;
  if (type == 'v') {
    if (ti->no_video)
      return false;
    tracks = ti->vtracks;
  } else if (type == 'a') {
    if (ti->no_audio)
      return false;
    tracks = ti->atracks;
  } else if (type == 's') {
    if (ti->no_subs)
      return false;
    tracks = ti->stracks;
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

//--------------------------------------------------------------------

track_info_c::track_info_c():
  initialized(true),
  id(0), fname(NULL),
  no_audio(false), no_video(false), no_subs(false),
  private_data(NULL), private_size(0),
  aspect_ratio(0.0), display_width(0), display_height(0),
  aspect_ratio_given(false), display_dimensions_given(false),
  cues(0), default_track(false), language(NULL), sub_charset(NULL),
  tags_ptr(NULL), tags(NULL),
  compression(COMPRESSION_NONE),
  track_name(NULL), ext_timecodes(NULL),
  no_chapters(false), no_attachments(false), no_tags(false) {
  atracks = new vector<int64_t>;
  vtracks = new vector<int64_t>;
  stracks = new vector<int64_t>;

  all_fourccs = new vector<fourcc_t>;
  memset(fourcc, 0, 5);
  display_properties = new vector<display_properties_t>;
  audio_syncs = new vector<audio_sync_t>;
  async.linear = 1.0;
  async.displacement = 0;
  cue_creations = new vector<cue_creation_t>;
  default_track_flags = new vector<int64_t>;
  languages = new vector<language_t>;
  sub_charsets = new vector<language_t>;
  all_tags = new vector<tags_t>;
  aac_is_sbr = new vector<int64_t>;
  compression_list = new vector<cue_creation_t>;
  track_names = new vector<language_t>;
  all_ext_timecodes = new vector<language_t>;
  track_order = new vector<int64_t>;
}

track_info_c::track_info_c(const track_info_c &src):
  initialized(false) {
  *this = src;
}

track_info_c::~track_info_c() {
  free_contents();
}

void track_info_c::free_contents() {
  uint32_t i;

  if (!initialized)
    return;

  safefree(fname);
  delete atracks;
  delete vtracks;
  delete stracks;
  delete audio_syncs;
  delete cue_creations;
  delete default_track_flags;
  for (i = 0; i < languages->size(); i++)
    safefree((*languages)[i].language);
  delete languages;
  safefree(language);
  for (i = 0; i < sub_charsets->size(); i++)
    safefree((*sub_charsets)[i].language);
  delete sub_charsets;
  safefree(sub_charset);
  for (i = 0; i < all_tags->size(); i++)
    safefree((*all_tags)[i].file_name);
  delete all_tags;
  delete aac_is_sbr;
  delete compression_list;
  for (i = 0; i < track_names->size(); i++)
    safefree((*track_names)[i].language);
  delete track_names;
  safefree(track_name);
  for (i = 0; i < all_ext_timecodes->size(); i++)
    safefree((*all_ext_timecodes)[i].language);
  delete all_ext_timecodes;
  safefree(ext_timecodes);
  safefree(private_data);
  if (tags != NULL)
    delete tags;
  delete all_fourccs;
  delete display_properties;
  delete track_order;

  initialized = false;
}

track_info_c &track_info_c::operator =(const track_info_c &src) {
  uint32_t i;

  free_contents();

  id = src.id;
  fname = safestrdup(src.fname);

  no_audio = src.no_audio;
  no_video = src.no_video;
  no_subs = src.no_subs;

  atracks = new vector<int64_t>(*src.atracks);
  vtracks = new vector<int64_t>(*src.vtracks);
  stracks = new vector<int64_t>(*src.stracks);

  private_size = src.private_size;
  private_data = (unsigned char *)safememdup(src.private_data, private_size);

  all_fourccs = new vector<fourcc_t>(*src.all_fourccs);
  memcpy(fourcc, src.fourcc, 5);

  display_properties =
    new vector<display_properties_t>(*src.display_properties);
  aspect_ratio = src.aspect_ratio;
  aspect_ratio_given = false;
  display_dimensions_given = false;

  audio_syncs = new vector<audio_sync_t>(*src.audio_syncs);
  memcpy(&async, &src.async, sizeof(audio_sync_t));

  cue_creations = new vector<cue_creation_t>(*src.cue_creations);
  cues = src.cues;

  default_track_flags = new vector<int64_t>(*src.default_track_flags);
  default_track = src.default_track;

  languages = new vector<language_t>(*src.languages);
  for (i = 0; i < src.languages->size(); i++)
    (*languages)[i].language = safestrdup((*src.languages)[i].language);
  language = safestrdup(src.language);

  sub_charsets = new vector<language_t>(*src.sub_charsets);
  for (i = 0; i < src.sub_charsets->size(); i++)
    (*sub_charsets)[i].language =
      safestrdup((*src.sub_charsets)[i].language);
  sub_charset = safestrdup(src.sub_charset);

  all_tags = new vector<tags_t>(*src.all_tags);
  for (i = 0; i < src.all_tags->size(); i++)
    (*all_tags)[i].file_name = safestrdup((*src.all_tags)[i].file_name);
  tags_ptr = src.tags_ptr;
  tags = NULL;
//   tags = src.tags;

  aac_is_sbr = new vector<int64_t>(*src.aac_is_sbr);

  compression_list = new vector<cue_creation_t>(*src.compression_list);
  compression = src.compression;

  track_names = new vector<language_t>(*src.track_names);
  for (i = 0; i < src.track_names->size(); i++)
    (*track_names)[i].language =
      safestrdup((*src.track_names)[i].language);
  track_name = safestrdup(src.track_name);

  all_ext_timecodes = new vector<language_t>(*src.all_ext_timecodes);
  for (i = 0; i < src.all_ext_timecodes->size(); i++)
    (*all_ext_timecodes)[i].language =
      safestrdup((*src.all_ext_timecodes)[i].language);
  ext_timecodes = safestrdup(src.ext_timecodes);

  no_chapters = src.no_chapters;
  no_attachments = src.no_attachments;
  no_tags = src.no_tags;

  track_order = new vector<int64_t>(*src.track_order);

  initialized = true;

  return *this;
}

//--------------------------------------------------------------------

struct ltstr {
  bool operator()(const char* s1, const char* s2) const {
    return strcmp(s1, s2) < 0;
  }
};
static map<const char *, string, ltstr> pass_data;

void set_pass_data(track_info_c *ti, unsigned char *data, int size) {
  string key, value;

  key = string(ti->fname) + string("::") + to_string(ti->id);
  value = base64_encode(data, size);
  mxverb(4, "add_pass_data for %s orig length %d, b64 length %d\n",
         key.c_str(), size, value.length());
  pass_data[key.c_str()] = value;
}

unsigned char *retrieve_pass_data(track_info_c *ti, int &size) {
  map<const char *, string, ltstr>::iterator it;
  string key, value;
  unsigned char *data;

  key = string(ti->fname) + string("::") + to_string(ti->id);
  it = pass_data.find(key.c_str());
  if (it != pass_data.end()) {
    size = -1;
    return NULL;
  }

  value = it->second;
  data = (unsigned char *)safemalloc(value.length());
  size = base64_decode(value, data);
  data = (unsigned char *)saferealloc(data, size);
  mxverb(4, "retrieve_pass_data for %s b64 length %d, orig length %d\n",
         key.c_str(), value.length(), size);
  return data;
}
