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
    \version \$Id: pr_generic.cpp,v 1.47 2003/05/26 21:49:11 mosu Exp $
    \brief functions common for all readers/packetizers
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include "KaxTracks.h"
#include "KaxTrackEntryData.h"
#include "KaxTrackAudio.h"
#include "KaxTrackVideo.h"

#include "common.h"
#include "mkvmerge.h"
#include "pr_generic.h"

static int default_tracks[3] = {0, 0, 0};

generic_packetizer_c::generic_packetizer_c(generic_reader_c *nreader,
                                           track_info_t *nti) throw(error_c) {
#ifdef DEBUG
  debug_c::add_packetizer(this);
#endif
  reader = nreader;
  add_packetizer(this);
  duplicate_data = true;

  track_entry = NULL;
  ti = duplicate_track_info(nti);
  free_refs = -1;

  // Set default header values to 'unset'.
  hserialno = track_number++;
  huid = 0;
  htrack_type = -1;
  htrack_min_cache = -1;
  htrack_max_cache = -1;

  hcodec_id = NULL;
  hcodec_private = NULL;
  hcodec_private_length = 0;

  haudio_sampling_freq = -1.0;
  haudio_channels = -1;
  haudio_bit_depth = -1;

  hvideo_pixel_width = -1;
  hvideo_pixel_height = -1;
  hvideo_frame_rate = -1.0;
}

generic_packetizer_c::~generic_packetizer_c() {
  free_track_info(ti);

  if (hcodec_id != NULL)
    safefree(hcodec_id);
  if (hcodec_private != NULL)
    safefree(hcodec_private);
}

int generic_packetizer_c::read() {
  return reader->read();
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

void generic_packetizer_c::set_codec_id(char *id) {
  safefree(hcodec_id);
  if (id == NULL) {
    hcodec_id = NULL;
    return;
  }
  hcodec_id = safestrdup(id);
}

void generic_packetizer_c::set_codec_private(unsigned char *cp, int length) {
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

void generic_packetizer_c::set_video_aspect_ratio(float ar) {
  ti->aspect_ratio = ar;
}

void generic_packetizer_c::set_video_frame_rate(float frame_rate) {
  hvideo_frame_rate = frame_rate;
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
    die("Unknown track type %d.", type);

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
    die("Unknown track type %d.", type);

  if (default_tracks[idx] > 0)
    fprintf(stdout, "Warning: Another default track for %s tracks has already "
            "been set. Not setting the 'default' flag for this track.\n",
            idx == 0 ? "audio" : idx == 'v' ? "video" : "subtitle");
  else
    default_tracks[idx] = hserialno;
}

void generic_packetizer_c::set_language(char *language) {
  safefree(ti->language);
  ti->language = safestrdup(language);
}

void generic_packetizer_c::set_headers() {
  int idx;

  if (track_entry == NULL) {
    if (kax_last_entry == NULL)
      track_entry = &GetChild<KaxTrackEntry>(*kax_tracks);
    else
      track_entry =
        &GetNextChild<KaxTrackEntry>(*kax_tracks, *kax_last_entry);
    kax_last_entry = track_entry;
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

  if (hcodec_id != NULL) {
    KaxCodecID &codec_id = GetChild<KaxCodecID>(*track_entry);
    codec_id.CopyBuffer((binary *)hcodec_id, strlen(hcodec_id) + 1);
  }

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

    if (hvideo_pixel_height != -1) {
      *(static_cast<EbmlUInteger *>
        (&GetChild<KaxVideoPixelHeight>(video))) = hvideo_pixel_height;
      if ((ti->aspect_ratio != 1.0) &&
          ((hvideo_pixel_width / hvideo_pixel_height) != ti->aspect_ratio))
        *(static_cast<EbmlUInteger *>
          (&GetChild<KaxVideoDisplayHeight>(video))) = hvideo_pixel_height;
    }

    if (hvideo_pixel_width != -1) {
      *(static_cast<EbmlUInteger *>
        (&GetChild<KaxVideoPixelWidth>(video))) = hvideo_pixel_width;
      if ((ti->aspect_ratio != 1.0) &&
          ((hvideo_pixel_width / hvideo_pixel_height) != ti->aspect_ratio))
        *(static_cast<EbmlUInteger *>
          (&GetChild<KaxVideoDisplayWidth>(video))) =
          (uint64_t)(hvideo_pixel_height * ti->aspect_ratio);
    }

    if (hvideo_frame_rate != -1.0)
      *(static_cast<EbmlFloat *>
        (&GetChild<KaxVideoFrameRate>(video))) = hvideo_frame_rate;

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
}

void generic_packetizer_c::duplicate_data_on_add(bool duplicate) {
  duplicate_data = duplicate;
}

void generic_packetizer_c::add_packet(unsigned char  *data, int length,
                                      int64_t timecode,
                                      int64_t duration, int duration_mandatory,
                                      int64_t bref, int64_t fref) {
  packet_t *pack;

  if (data == NULL)
    return;
  if (timecode < 0)
    die("timecode < 0 (%lld)", timecode);

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

//--------------------------------------------------------------------

generic_reader_c::generic_reader_c(track_info_t *nti) {
  ti = duplicate_track_info(nti);
}

generic_reader_c::~generic_reader_c() {
  free_track_info(ti);
}

//--------------------------------------------------------------------

track_info_t *duplicate_track_info(track_info_t *src) {
  track_info_t *dst;

  if (src == NULL)
    return NULL;

  dst = (track_info_t *)safememdup(src, sizeof(track_info_t));
  dst->fname = safestrdup(src->fname);
  dst->atracks = safestrdup(src->atracks);
  dst->vtracks = safestrdup(src->vtracks);
  dst->stracks = safestrdup(src->stracks);
  dst->private_data = (unsigned char *)safememdup(src->private_data,
                                                  src->private_size);
  dst->language = safestrdup(src->language);
  dst->sub_charset = safestrdup(src->sub_charset);

  return dst;
}

void free_track_info(track_info_t *ti) {
  if (ti == NULL)
    return;

  safefree(ti->fname);
  safefree(ti->atracks);
  safefree(ti->vtracks);
  safefree(ti->stracks);
  safefree(ti->private_data);
  safefree(ti->language);
  safefree(ti->sub_charset);
  safefree(ti);
}
