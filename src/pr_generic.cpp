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

#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>

#include "common.h"
#include "mkvmerge.h"
#include "pr_generic.h"
#include "tagparser.h"

generic_packetizer_c::generic_packetizer_c(generic_reader_c *nreader,
                                           track_info_t *nti) throw(error_c) {
  int i;
  audio_sync_t *as;
  cue_creation_t *cc;
  int64_t id;
  language_t *lang;
  tags_t *tags;
  bool found;

#ifdef DEBUG
  debug_facility.add_packetizer(this);
#endif
  reader = nreader;
  add_packetizer(this);
  duplicate_data = true;

  track_entry = NULL;
  ti = duplicate_track_info(nti);
  free_refs = -1;

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
  haudio_channels = -1;
  haudio_bit_depth = -1;

  hvideo_pixel_width = -1;
  hvideo_pixel_height = -1;
  hvideo_display_width = -1;
  hvideo_display_height = -1;

  dumped_packet_number = 0;
}

generic_packetizer_c::~generic_packetizer_c() {
  free_track_info(ti);

  if (hcodec_id != NULL)
    safefree(hcodec_id);
  if (hcodec_private != NULL)
    safefree(hcodec_private);
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

void generic_packetizer_c::set_codec_id(const char *id) {
  safefree(hcodec_id);
  if (id == NULL) {
    hcodec_id = NULL;
    return;
  }
  hcodec_id = safestrdup(id);
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
}

void generic_packetizer_c::set_track_min_cache(int min_cache) {
  htrack_min_cache = min_cache;
}

void generic_packetizer_c::set_track_max_cache(int max_cache) {
  htrack_max_cache = max_cache;
}

void generic_packetizer_c::set_track_default_duration_ns(int64_t def_dur) {
  htrack_default_duration = def_dur;
}

int64_t generic_packetizer_c::get_track_default_duration_ns() {
  return htrack_default_duration;
}

void generic_packetizer_c::set_audio_sampling_freq(float freq) {
  haudio_sampling_freq = freq;
}

void generic_packetizer_c::set_audio_channels(int channels) {
  haudio_channels = channels;
}

void generic_packetizer_c::set_audio_bit_depth(int bit_depth) {
  haudio_bit_depth = bit_depth;
}

void generic_packetizer_c::set_video_pixel_width(int width) {
  hvideo_pixel_width = width;
}

void generic_packetizer_c::set_video_pixel_height(int height) {
  hvideo_pixel_height = height;
}

void generic_packetizer_c::set_video_display_width(int width) {
  hvideo_display_width = width;
}

void generic_packetizer_c::set_video_display_height(int height) {
  hvideo_display_height = height;
}

void generic_packetizer_c::set_video_aspect_ratio(float ar) {
  ti->aspect_ratio = ar;
}

void generic_packetizer_c::set_as_default_track(int type) {
  int idx;

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

  if (type == track_audio)
    idx = 0;
  else if (type == track_video)
    idx = 1;
  else if (type == track_subtitle)
    idx = 2;
  else
    die("pr_generic.cpp/generic_packetizer_c::force_default_track(): Unknown "
        "track type %d.", type);

  if (default_tracks[idx] > 0)
    mxwarn("Another default track for %s tracks has already "
           "been set. Not setting the 'default' flag for this track.\n",
           idx == 0 ? "audio" : idx == 'v' ? "video" : "subtitle");
  else
    default_tracks[idx] = hserialno;
}

void generic_packetizer_c::set_language(const char *language) {
  safefree(ti->language);
  ti->language = safestrdup(language);
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

  if (ti->language != NULL) {
    *(static_cast<EbmlString *>
      (&GetChild<KaxTrackLanguage>(*track_entry))) = ti->language;
  }

  if (htrack_type == track_video) {
    KaxTrackVideo &video =
      GetChild<KaxTrackVideo>(*track_entry);

    if ((hvideo_pixel_height != -1) && (hvideo_pixel_width != -1)) {
      if ((hvideo_display_width == -1) || (hvideo_display_height == -1) ||
          ti->aspect_ratio_given) {
        if (!ti->aspect_ratio_given)
          ti->aspect_ratio = (float)hvideo_pixel_width /
            (float)hvideo_pixel_height;
        if (ti->aspect_ratio >
            ((float)hvideo_pixel_width / (float)hvideo_pixel_height)) {
          disp_width = (int)(hvideo_pixel_height * ti->aspect_ratio);
          disp_height = hvideo_pixel_height;

        } else {
          disp_width = hvideo_pixel_width;
          disp_height = (int)(hvideo_pixel_width / ti->aspect_ratio);

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
      *(static_cast<EbmlFloat *> (&GetChild<KaxAudioSamplingFreq>(audio))) =
        haudio_sampling_freq;

    if (haudio_channels != -1)
      *(static_cast<EbmlUInteger *> (&GetChild<KaxAudioChannels>(audio))) =
        haudio_channels;

    if (haudio_bit_depth != -1)
      *(static_cast<EbmlUInteger *> (&GetChild<KaxAudioBitDepth>(audio))) =
        haudio_bit_depth;

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
                                      int ref_priority) {
  packet_t *pack;

  if (data == NULL)
    return;
  if (timecode < 0)
    die("pr_generic.cpp/generic_packetizer_c::add_packet(): timecode < 0 "
        "(%lld)", timecode);

  pack = (packet_t *)safemalloc(sizeof(packet_t));
  memset(pack, 0, sizeof(packet_t));
  if (duplicate_data)
    pack->data = (unsigned char *)safememdup(data, length);
  else
    pack->data = data;
  pack->length = length;
  pack->timecode = timecode;
  pack->bref = bref;
  pack->fref = fref;
  pack->ref_priority = ref_priority;
  pack->duration = duration;
  pack->duration_mandatory = duration_mandatory;
  pack->source = this;

  packet_queue.push_back(pack);
}

packet_t *generic_packetizer_c::get_packet() {
  packet_t *pack;

  if (packet_queue.size() == 0)
    return NULL;

  pack = packet_queue.front();
  packet_queue.pop_front();

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
  int64_t bytes;
  int i;

  for (i = 0, bytes = 0; i < packet_queue.size(); i++)
    bytes += packet_queue[i]->length;

  return bytes;
}

void generic_packetizer_c::rerender_headers(mm_io_c *out) {
  out->save_pos(track_entry->GetElementPosition());
  track_entry->Render(*out);
  out->restore_pos();
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

//--------------------------------------------------------------------

generic_reader_c::generic_reader_c(track_info_t *nti) {
  ti = duplicate_track_info(nti);
}

generic_reader_c::~generic_reader_c() {
  free_track_info(ti);
}

bool generic_reader_c::demuxing_requested(char type, int64_t id) {
  vector<int64_t> *tracks;
  int i;

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

track_info_t *duplicate_track_info(track_info_t *src) {
  track_info_t *dst;
  int i;

  if (src == NULL)
    return NULL;

  dst = (track_info_t *)safememdup(src, sizeof(track_info_t));
  dst->fname = safestrdup(src->fname);
  dst->atracks = new vector<int64_t>(*src->atracks);
  dst->vtracks = new vector<int64_t>(*src->vtracks);
  dst->stracks = new vector<int64_t>(*src->stracks);
  dst->audio_syncs = new vector<audio_sync_t>(*src->audio_syncs);
  dst->cue_creations = new vector<cue_creation_t>(*src->cue_creations);
  dst->default_track_flags = new vector<int64_t>(*src->default_track_flags);
  dst->languages = new vector<language_t>(*src->languages);
  for (i = 0; i < src->languages->size(); i++)
    (*dst->languages)[i].language = safestrdup((*src->languages)[i].language);
  dst->sub_charsets = new vector<language_t>(*src->sub_charsets);
  for (i = 0; i < src->sub_charsets->size(); i++)
    (*dst->sub_charsets)[i].language =
      safestrdup((*src->sub_charsets)[i].language);
  dst->all_tags = new vector<tags_t>(*src->all_tags);
  for (i = 0; i < src->all_tags->size(); i++)
    (*dst->all_tags)[i].file_name = safestrdup((*src->all_tags)[i].file_name);
  dst->aac_is_sbr = new vector<int64_t>(*src->aac_is_sbr);
  dst->private_data = (unsigned char *)safememdup(src->private_data,
                                                  src->private_size);
  dst->language = safestrdup(src->language);
  dst->sub_charset = safestrdup(src->sub_charset);
  dst->tags = NULL;

  return dst;
}

void free_track_info(track_info_t *ti) {
  int i;

  if (ti == NULL)
    return;

  safefree(ti->fname);
  delete ti->atracks;
  delete ti->vtracks;
  delete ti->stracks;
  delete ti->audio_syncs;
  delete ti->cue_creations;
  delete ti->default_track_flags;
  for (i = 0; i < ti->languages->size(); i++)
    safefree((*ti->languages)[i].language);
  delete ti->languages;
  for (i = 0; i < ti->sub_charsets->size(); i++)
    safefree((*ti->sub_charsets)[i].language);
  delete ti->sub_charsets;
  for (i = 0; i < ti->all_tags->size(); i++)
    safefree((*ti->all_tags)[i].file_name);
  delete ti->all_tags;
  delete ti->aac_is_sbr;
  safefree(ti->language);
  safefree(ti->private_data);
  safefree(ti->sub_charset);
  if (ti->tags != NULL)
    delete ti->tags;
  safefree(ti);
}
