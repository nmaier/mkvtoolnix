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
    \version \$Id: pr_generic.h,v 1.44 2003/05/25 15:35:39 mosu Exp $
    \brief class definition for the generic reader and packetizer
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#ifndef __PR_GENERIC_H
#define __PR_GENERIC_H

#include <deque>

#include "KaxBlock.h"
#include "KaxCluster.h"
#include "KaxTracks.h"

#include "error.h"

using namespace LIBMATROSKA_NAMESPACE;
using namespace std;

#define CUES_UNSPECIFIED -1
#define CUES_NONE         0
#define CUES_IFRAMES      1
#define CUES_ALL          2

typedef struct {
  int    displacement;
  double linear;
} audio_sync_t;

typedef struct {
  KaxBlockGroup *group;
  KaxBlock *block;
  KaxCluster *cluster;
  unsigned char *data;
  int length, superseeded, duration_mandatory;
  int64_t timecode, bref, fref, duration;
  void *source;
} packet_t;

typedef struct {
  // Options used by the readers.
  char *fname;
  unsigned char *atracks, *vtracks, *stracks;
  int cues;

  // Options used by the packetizers.
  unsigned char *private_data;
  int private_size;
  int no_utf8_subs;

  char fourcc[5];
  float aspect_ratio;

  audio_sync_t async;

  int default_track;

  char *language, *sub_charset;
} track_info_t;

class generic_reader_c;

class generic_packetizer_c {
protected:
  deque<packet_t *> packet_queue;
  generic_reader_c *reader;
  bool duplicate_data;

  track_info_t *ti;
  int64_t free_refs;

  KaxTrackEntry *track_entry;

  // Header entries. Can be set via set_XXX and will be 'rendered'
  // by set_headers().
  int hserialno, htrack_type, htrack_min_cache, htrack_max_cache;
  uint32_t huid;

  char *hcodec_id;
  unsigned char *hcodec_private;
  int hcodec_private_length;

  float haudio_sampling_freq;
  int haudio_channels, haudio_bit_depth;

  int hvideo_pixel_width, hvideo_pixel_height;
  float hvideo_frame_rate;

public:
  generic_packetizer_c(generic_reader_c *nreader, track_info_t *nti)
    throw (error_c);
  virtual ~generic_packetizer_c();

  virtual int read();

  virtual void duplicate_data_on_add(bool duplicate);
  virtual void add_packet(unsigned char *data, int lenth, int64_t timecode,
                          int64_t duration, int duration_mandatory = 0,
                          int64_t bref = -1, int64_t fref = -1);
  virtual packet_t *get_packet();
  virtual int packet_available();
  virtual int64_t get_smallest_timecode();
  virtual int64_t get_queued_bytes();

  virtual void set_free_refs(int64_t nfree_refs);
  virtual int64_t get_free_refs();
  virtual void set_headers();
  virtual int process(unsigned char *data, int size,
                      int64_t timecode = -1, int64_t length = -1,
                      int64_t bref = -1, int64_t fref = -1) = 0;

  virtual void dump_debug_info() = 0;

  virtual void set_cue_creation(int create_cue_data);
  virtual int get_cue_creation();

  virtual KaxTrackEntry *get_track_entry();

  virtual int set_uid(uint32_t uid);
  virtual void set_track_type(int type);
  virtual void set_language(char *language);

  virtual void set_codec_id(char *id);
  virtual void set_codec_private(unsigned char *cp, int length);

  virtual void set_track_min_cache(int min_cache);
  virtual void set_track_max_cache(int max_cache);

  virtual void set_audio_sampling_freq(float freq);
  virtual void set_audio_channels(int channels);
  virtual void set_audio_bit_depth(int bit_depth);

  virtual void set_video_pixel_width(int width);
  virtual void set_video_pixel_height(int height);
  virtual void set_video_aspect_ratio(float ar);
  virtual void set_video_frame_rate(float frame_rate);

  virtual void set_as_default_track(int type);
  virtual void force_default_track(int type);
};

class generic_reader_c {
protected:
  track_info_t *ti;
public:
  generic_reader_c(track_info_t *nti);
  virtual ~generic_reader_c();
  virtual int read() = 0;
  virtual packet_t *get_packet() = 0;
  virtual int display_priority() = 0;
  virtual void display_progress() = 0;
  virtual void set_headers() = 0;
};

track_info_t *duplicate_track_info(track_info_t *src);
void free_track_info(track_info_t *ti);

#endif  // __PR_GENERIC_H
