/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  pr_generic.h

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief class definition for the generic reader and packetizer
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __PR_GENERIC_H
#define __PR_GENERIC_H

#include "os.h"

#include <deque>
#include <vector>

#include <matroska/KaxAttachments.h>
#include <matroska/KaxBlock.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTags.h>

#include "error.h"
#include "mm_io.h"

using namespace libmatroska;
using namespace std;

#define CUES_UNSPECIFIED -1
#define CUES_NONE         0
#define CUES_IFRAMES      1
#define CUES_ALL          2

typedef struct {
  int displacement;
  double linear;
  int64_t id;
} audio_sync_t;

typedef struct {
  KaxBlockGroup *group;
  KaxBlock *block;
  KaxCluster *cluster;
  unsigned char *data;
  int length, superseeded, ref_priority;
  int64_t timecode, bref, fref, duration, packet_num;
  bool duration_mandatory;
  void *source;
} packet_t;

typedef struct {
  int cues;
  int64_t id;
} cue_creation_t;

typedef struct {
  char *language;
  int64_t id;
} language_t;

typedef struct {
  char *file_name;
  int64_t id;
} tags_t;

typedef struct {
  // The track ID.
  int64_t id;

  // Options used by the readers.
  char *fname;
  bool no_audio, no_video, no_subs;
  vector<int64_t> *atracks, *vtracks, *stracks;

  // Options used by the packetizers.
  unsigned char *private_data;
  int private_size;

  char fourcc[5];
  float aspect_ratio;
  bool aspect_ratio_given;

  vector<audio_sync_t> *audio_syncs; // As given on the command line
  audio_sync_t async;           // For this very track

  vector<cue_creation_t> *cue_creations; // As given on the command line
  int cues;                     // For this very track

  vector<int64_t> *default_track_flags; // As given on the command line
  bool default_track;           // For this very track

  vector<language_t> *languages; // As given on the command line
  char *language;               // For this very track

  vector<language_t> *sub_charsets; // As given on the command line
  char *sub_charset;            // For this very track

  vector<tags_t> *all_tags;     // As given on the command line
  tags_t *tags_ptr;             // For this very track
  KaxTags *tags;                // For this very track

  vector<int64_t> *aac_is_sbr;  // For AAC+/HE-AAC/SBR

  vector<cue_creation_t> *compression_list; // As given on the command line
  int compression;              // For this very track

  vector<language_t> *track_names; // As given on the command line
  char *track_name;             // For this very track

  bool no_chapters, no_attachments, no_tags;
} track_info_t;

class generic_reader_c;

class generic_packetizer_c {
protected:
  deque<packet_t *> packet_queue;
  generic_reader_c *reader;
  bool duplicate_data;

  track_info_t *ti;
  int64_t free_refs, enqueued_bytes;

  KaxTrackEntry *track_entry;

  // Header entries. Can be set via set_XXX and will be 'rendered'
  // by set_headers().
  int hserialno, htrack_type, htrack_min_cache, htrack_max_cache;
  int64_t htrack_default_duration;
  uint32_t huid;

  char *hcodec_id;
  unsigned char *hcodec_private;
  int hcodec_private_length;

  float haudio_sampling_freq, haudio_output_sampling_freq;
  int haudio_channels, haudio_bit_depth;

  int hvideo_pixel_width, hvideo_pixel_height;
  int hvideo_display_width, hvideo_display_height;

  int64_t dumped_packet_number;

public:
  generic_packetizer_c(generic_reader_c *nreader, track_info_t *nti)
    throw (error_c);
  virtual ~generic_packetizer_c();

  virtual int read();

  virtual void reset();

  virtual void duplicate_data_on_add(bool duplicate);
  virtual void add_packet(unsigned char *data, int lenth, int64_t timecode,
                          int64_t duration, bool duration_mandatory = false,
                          int64_t bref = -1, int64_t fref = -1,
                          int ref_priority = -1);
  virtual packet_t *get_packet();
  virtual int packet_available();
  virtual int64_t get_smallest_timecode();
  virtual int64_t get_queued_bytes();

  virtual void set_free_refs(int64_t nfree_refs);
  virtual int64_t get_free_refs();
  virtual void set_headers();
  virtual void rerender_headers(mm_io_c *out);
  virtual int process(unsigned char *data, int size,
                      int64_t timecode = -1, int64_t length = -1,
                      int64_t bref = -1, int64_t fref = -1) = 0;

  virtual void dump_debug_info() = 0;

  virtual void set_cue_creation(int create_cue_data);
  virtual int get_cue_creation();

  virtual KaxTrackEntry *get_track_entry();
  virtual int get_track_num();

  virtual int set_uid(uint32_t uid);
  virtual void set_track_type(int type);
  virtual int get_track_type();
  virtual void set_language(const char *language);

  virtual void set_codec_id(const char *id);
  virtual void set_codec_private(const unsigned char *cp, int length);

  virtual void set_track_min_cache(int min_cache);
  virtual void set_track_max_cache(int max_cache);
  virtual void set_track_default_duration_ns(int64_t default_duration);
  virtual int64_t get_track_default_duration_ns();

  virtual void set_audio_sampling_freq(float freq);
  virtual void set_audio_output_sampling_freq(float freq);
  virtual void set_audio_channels(int channels);
  virtual void set_audio_bit_depth(int bit_depth);

  virtual void set_video_pixel_width(int width);
  virtual void set_video_pixel_height(int height);
  virtual void set_video_display_width(int width);
  virtual void set_video_display_height(int height);
  virtual void set_video_aspect_ratio(float ar);

  virtual void set_as_default_track(int type);
  virtual void force_default_track(int type);

  virtual void set_tag_track_uid();

  virtual void set_track_name(const char *name);

protected:
  virtual void dump_packet(const void *buffer, int size);
};

class generic_reader_c {
protected:
  track_info_t *ti;
public:
  generic_reader_c(track_info_t *nti);
  virtual ~generic_reader_c();
  virtual int read(generic_packetizer_c *ptzr) = 0;
  virtual int display_priority() = 0;
  virtual void display_progress(bool final = false) = 0;
  virtual void set_headers() = 0;
  virtual void identify() = 0;

  virtual void add_attachments(KaxAttachments *a) {
  };
//   virtual void set_tag_track_uids() = 0;

protected:
  virtual bool demuxing_requested(char type, int64_t id);
};

track_info_t *duplicate_track_info(track_info_t *src);
void free_track_info(track_info_t *ti);

#endif  // __PR_GENERIC_H
