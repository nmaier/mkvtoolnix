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
    \version \$Id: pr_generic.h,v 1.33 2003/05/03 20:22:18 mosu Exp $
    \brief class definition for the generic reader and packetizer
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#ifndef __PR_GENERIC_H
#define __PR_GENERIC_H

#include "KaxBlock.h"
#include "KaxCluster.h"
#include "KaxTracks.h"

#include "error.h"
#include "queue.h"

using namespace LIBMATROSKA_NAMESPACE;

#define CUES_UNSPECIFIED -1
#define CUES_NONE         0
#define CUES_IFRAMES      1
#define CUES_ALL          2

typedef struct {
  int    displacement;
  double linear;
} audio_sync_t;

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

  audio_sync_t async;

  int default_track;

  char *language;
} track_info_t;

class generic_packetizer_c: public q_c {
protected:
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

  int hdefault_track;

public:
  generic_packetizer_c(track_info_t *nti) throw (error_c);
  virtual ~generic_packetizer_c();

  virtual void set_free_refs(int64_t nfree_refs);
  virtual int64_t get_free_refs();
  virtual void set_headers();
  virtual int process(unsigned char *data, int size,
                      int64_t timecode = -1, int64_t length = -1,
                      int64_t bref = -1, int64_t fref = -1) = 0;

  virtual void set_cue_creation(int create_cue_data);
  virtual int get_cue_creation();

  virtual KaxTrackEntry *get_track_entry();

  virtual void set_serial(int serial = -1);
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
  virtual void set_video_frame_rate(float frame_rate);

  virtual void set_as_default_track(char type);
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
