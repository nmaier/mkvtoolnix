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
    \version \$Id: pr_generic.cpp,v 1.27 2003/04/29 16:23:12 mosu Exp $
    \brief functions common for all readers/packetizers
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <malloc.h>

#include "KaxTracks.h"
#include "KaxTrackEntryData.h"
#include "KaxTrackAudio.h"
#include "KaxTrackVideo.h"

#include "common.h"
#include "mkvmerge.h"
#include "pr_generic.h"

static int default_tracks[3] = {0, 0, 0};

generic_packetizer_c::generic_packetizer_c(track_info_t *nti) throw(error_c):
  q_c() {
  track_entry = NULL;
  ti = duplicate_track_info(nti);
  free_refs = -1;

  // Set default header values to 'unset'.
  hserialno = -2;
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

  hdefault_track = -1;
}

generic_packetizer_c::~generic_packetizer_c() {
  free_track_info(ti);

  if (hcodec_id != NULL)
    free(hcodec_id);
  if (hcodec_private != NULL)
    free(hcodec_private);
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

void generic_packetizer_c::set_serial(int serial = -1) {
  hserialno = serial;
}

void generic_packetizer_c::set_track_type(int type) {
  htrack_type = type;
}

void generic_packetizer_c::set_codec_id(char *id) {
  if (hcodec_id != NULL)
    free(hcodec_id);
  if (id == NULL) {
    hcodec_id = NULL;
    return;
  }
  hcodec_id = strdup(id);
  if (hcodec_id == NULL)
    die("malloc");
}

void generic_packetizer_c::set_codec_private(unsigned char *cp, int length) {
  if (hcodec_private != NULL)
    free(hcodec_private);
  if (cp == NULL) {
    hcodec_private = NULL;
    hcodec_private_length = 0;
    return;
  }
  hcodec_private = (unsigned char *)malloc(length);
  if (hcodec_private == NULL)
    die("malloc");
  memcpy(hcodec_private, cp, length);
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

void generic_packetizer_c::set_video_frame_rate(float frame_rate) {
  hvideo_frame_rate = frame_rate;
}

void generic_packetizer_c::set_as_default_track(char type) {
  int idx;

  if (type == 'a')
    idx = 0;
  else if (type == 'v')
    idx = 1;
  else
    idx = 2;

  if (default_tracks[idx] != 0)
    fprintf(stdout, "Warning: Another default track for %s tracks has already "
            "been set. Not setting the 'default' flag for this track.\n",
            idx == 0 ? "audio" : idx == 'v' ? "video" : "subtitle");
  else {
    default_tracks[idx] = 1;
    hdefault_track = 1;
  }
}

void generic_packetizer_c::set_header() {
  if (track_entry == NULL) {
    if (kax_last_entry == NULL)
      track_entry =
        &GetChild<KaxTrackEntry>(static_cast<KaxTracks &>(*kax_tracks));
    else
      track_entry =
        &GetNextChild<KaxTrackEntry>(static_cast<KaxTracks &>(*kax_tracks),
          static_cast<KaxTrackEntry &>(*kax_last_entry));
    kax_last_entry = track_entry;
  }

  if (hserialno != -2) {  
    if (hserialno == -1)
      hserialno = track_number++;
    KaxTrackNumber &tnumber =
      GetChild<KaxTrackNumber>(static_cast<KaxTrackEntry &>(*track_entry));
    *(static_cast<EbmlUInteger *>(&tnumber)) = hserialno;

    KaxTrackUID &tuid =
      GetChild<KaxTrackUID>(static_cast<KaxTrackEntry &>(*track_entry));
    *(static_cast<EbmlUInteger *>(&tuid)) = create_unique_uint32();
  }

  if (htrack_type != -1)
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxTrackType>(static_cast<KaxTrackEntry &>(*track_entry)))) =
        htrack_type;

  if (hcodec_id != NULL) {
    KaxCodecID &codec_id =
      GetChild<KaxCodecID>(static_cast<KaxTrackEntry &>(*track_entry));
    codec_id.CopyBuffer((binary *)hcodec_id, strlen(hcodec_id) + 1);
  }

  if (hcodec_private != NULL) {
    KaxCodecPrivate &codec_private = 
      GetChild<KaxCodecPrivate>(static_cast<KaxTrackEntry &>(*track_entry));
    codec_private.CopyBuffer((binary *)hcodec_private, hcodec_private_length);
  }

  if (htrack_min_cache != -1)
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxTrackMinCache>(static_cast<KaxTrackEntry &>
                                   (*track_entry))))
      = htrack_min_cache;

  if (htrack_max_cache != -1)
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxTrackMaxCache>(static_cast<KaxTrackEntry &>
                                   (*track_entry))))
      = htrack_max_cache;

  if (hdefault_track != -1)
    *(static_cast<EbmlUInteger *>
      (&GetChild<KaxTrackFlagDefault>(static_cast<KaxTrackEntry &>
                                      (*track_entry))))
      = hdefault_track;

  if (htrack_type == track_video) {
    KaxTrackVideo &video =
      GetChild<KaxTrackVideo>(static_cast<KaxTrackEntry &>(*track_entry));

    if (hvideo_pixel_height != -1)
      *(static_cast<EbmlUInteger *>
        (&GetChild<KaxVideoPixelHeight>(video))) = hvideo_pixel_height;

    if (hvideo_pixel_width != -1)
      *(static_cast<EbmlUInteger *>
        (&GetChild<KaxVideoPixelWidth>(video))) = hvideo_pixel_width;

    if (hvideo_frame_rate != -1.0)
      *(static_cast<EbmlFloat *>
        (&GetChild<KaxVideoFrameRate>(video))) = hvideo_frame_rate;

  } else if (htrack_type == track_audio) {
    KaxTrackAudio &audio =
      GetChild<KaxTrackAudio>(static_cast<KaxTrackEntry &>(*track_entry));

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

  dst = (track_info_t *)malloc(sizeof(track_info_t));
  if (dst == NULL)
    die("malloc");

  memcpy(dst, src, sizeof(track_info_t));
  if (src->fname != NULL) {
    dst->fname = strdup(src->fname);
    if (dst->fname == NULL)
      die("strdup");
  }
  if (src->atracks != NULL) {
    dst->atracks = (unsigned char *)strdup((char *)src->atracks);
    if (dst->atracks == NULL)
      die("strdup");
  }
  if (src->vtracks != NULL) {
    dst->vtracks = (unsigned char *)strdup((char *)src->vtracks);
    if (dst->vtracks == NULL)
      die("strdup");
  }
  if (src->stracks != NULL) {
    dst->stracks = (unsigned char *)strdup((char *)src->stracks);
    if (dst->stracks == NULL)
      die("strdup");
  }
  if (src->private_data != NULL) {
    dst->private_data = (unsigned char *)malloc(src->private_size);
    if (dst->private_data == NULL)
      die("malloc");
    memcpy(dst->private_data, src->private_data, src->private_size);
  }

  return dst;
}

void free_track_info(track_info_t *ti) {
  if (ti == NULL)
    return;

  if (ti->fname != NULL)
    free(ti->fname);
  if (ti->atracks != NULL)
    free(ti->atracks);
  if (ti->vtracks != NULL)
    free(ti->vtracks);
  if (ti->stracks != NULL)
    free(ti->stracks);
  if (ti->private_data != NULL)
    free(ti->private_data);

  free(ti);
}

