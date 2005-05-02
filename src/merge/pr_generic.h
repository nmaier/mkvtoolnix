/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   class definition for the generic reader and packetizer

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Modified by Steve Lhomme <steve.lhomme@free.fr>.
*/

#ifndef __PR_GENERIC_H
#define __PR_GENERIC_H

#include "os.h"

#include <deque>
#include <vector>

#include <matroska/KaxAttachments.h>
#include <matroska/KaxBlock.h>
#include <matroska/KaxChapters.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTags.h>

#include "common_memory.h"
#include "compression.h"
#include "error.h"
#include "mm_io.h"
#include "smart_pointers.h"
#include "timecode_factory.h"

using namespace libmatroska;
using namespace std;

// CUE_STRATEGY_* control the creation of cue entries for a track.
// UNSPECIFIED: is used for command line parsing.
// NONE:        don't create any cue entries.
// IFRAMES:     Create cue entries for all I frames.
// ALL:         Create cue entries for all frames (not really useful).
// SPARSE:      Create cue entries for I frames if no video track exists, but
//              create at most one cue entries every two seconds. Used for
//              audio only files.
enum cue_strategy_e {
  CUE_STRATEGY_UNSPECIFIED = -1,
  CUE_STRATEGY_NONE,
  CUE_STRATEGY_IFRAMES,
  CUE_STRATEGY_ALL,
  CUE_STRATEGY_SPARSE
};

#define DEFTRACK_TYPE_AUDIO 0
#define DEFTRACK_TYPE_VIDEO 1
#define DEFTRACK_TYPE_SUBS  2

class generic_packetizer_c;
class generic_reader_c;

enum file_status_e {
  FILE_STATUS_DONE = 0,
  FILE_STATUS_HOLDING,
  FILE_STATUS_MOREDATA
};

struct audio_sync_t {
  int64_t displacement;
  double linear;
  int64_t id;

  audio_sync_t(): displacement(0), linear(0.0), id(0) {}
};

struct packet_delay_t {
  int64_t delay;
  int64_t id;

  packet_delay_t(): delay(0), id(0) {}
};

enum default_track_priority_e {
  DEFAULT_TRACK_PRIOIRTY_NONE = 0,
  DEFAULT_TRACK_PRIORITY_FROM_TYPE = 10,
  DEFAULT_TRACK_PRIORITY_FROM_SOURCE = 50,
  DEFAULT_TRACK_PRIORITY_CMDLINE = 255
};

#define FMT_FN "'%s': "
#define FMT_TID "'%s' track %lld: "

struct packet_t {
  KaxBlockGroup *group;
  KaxBlock *block;
  KaxCluster *cluster;
  unsigned char *data;
  int length, ref_priority;
  int64_t timecode, bref, fref, duration, packet_num, assigned_timecode;
  int64_t unmodified_assigned_timecode, unmodified_duration;
  bool duration_mandatory, superseeded, gap_following;
  generic_packetizer_c *source;
  vector<unsigned char *> data_adds;
  vector<int> data_adds_lengths;

  packet_t():
    group(NULL), block(NULL), cluster(NULL), data(NULL), length(0),
    ref_priority(0),
    timecode(0), bref(0), fref(0), duration(0),
    packet_num(0),
    assigned_timecode(0), unmodified_assigned_timecode(0),
    unmodified_duration(0),
    duration_mandatory(false), superseeded(false), gap_following(false),
    source(NULL) {}

  virtual ~packet_t() {
    vector<unsigned char *>::iterator i;

    safefree(data);
    foreach(i, data_adds)
      safefree(*i);
  }
};

struct cue_creation_t {
  cue_strategy_e cues;
  int64_t id;

  cue_creation_t(): cues(CUE_STRATEGY_NONE), id(0) {}
};

struct compression_method_t {
  compression_method_e method;
  int64_t id;

  compression_method_t(): method(COMPRESSION_UNSPECIFIED), id(0) {}
};

struct language_t {
  string language;
  int64_t id;

  language_t(): id(0) {}
};

struct track_name_t {
  string name;
  int64_t id;

  track_name_t(): id(0) {}
  track_name_t(const string &_name, int64_t _id): name(_name), id(_id) {}
};

struct ext_timecodes_t {
  string ext_timecodes;
  int64_t id;

  ext_timecodes_t(): id(0) {}
  ext_timecodes_t(const string &_ext_timecodes, int64_t _id):
    ext_timecodes(_ext_timecodes), id(_id) {}
};

struct subtitle_charset_t {
  string charset;
  int64_t id;

  subtitle_charset_t(): id(0) {}
};

struct tags_t {
  string file_name;
  int64_t id;

  tags_t(): id(0) {}
};

struct display_properties_t {
  float aspect_ratio;
  bool ar_factor;
  int width, height;
  int64_t id;

  display_properties_t(): aspect_ratio(0), ar_factor(false), width(0),
                          height(0), id(0) {}
};

typedef struct fourcc_t {
  char fourcc[5];
  int64_t id;

  fourcc_t(): id(0) {
    memset(fourcc, 0, 5);
  }
};

struct pixel_crop_t {
  int left, top, right, bottom;
  int64_t id;

  pixel_crop_t(): left(0), top(0), right(0), bottom(0), id(0) {}
};

struct max_blockadd_id_t {
  int64_t max_blockadd_id;
  int64_t id;

  max_blockadd_id_t(): max_blockadd_id(0), id(0) {}
};

struct default_duration_t {
  int64_t default_duration;
  int64_t id;

  default_duration_t(): default_duration(0), id(0) {}
};

class track_info_c {
protected:
  bool initialized;
public:
  // The track ID.
  int64_t id;

  // Options used by the readers.
  string fname;
  bool no_audio, no_video, no_subs, no_buttons;
  vector<int64_t> atracks, vtracks, stracks, btracks;

  // Options used by the packetizers.
  unsigned char *private_data;
  int private_size;

  vector<fourcc_t> all_fourccs;
  char fourcc[5];
  vector<display_properties_t> display_properties;
  float aspect_ratio;
  int display_width, display_height;
  bool aspect_ratio_given, aspect_ratio_is_factor, display_dimensions_given;

  vector<audio_sync_t> audio_syncs; // As given on the command line
  audio_sync_t async;           // For this very track

  vector<cue_creation_t> cue_creations; // As given on the command line
  cue_strategy_e cues;          // For this very track

  vector<int64_t> default_track_flags; // As given on the command line
  bool default_track;           // For this very track

  vector<language_t> languages; // As given on the command line
  string language;              // For this very track

  vector<subtitle_charset_t> sub_charsets; // As given on the command line
  string sub_charset;           // For this very track

  vector<tags_t> all_tags;     // As given on the command line
  int tags_ptr;                 // For this very track
  KaxTags *tags;                // For this very track

  vector<int64_t> aac_is_sbr;  // For AAC+/HE-AAC/SBR

  vector<packet_delay_t> packet_delays; // As given on the command line
  int64_t packet_delay;         // For this very track

  vector<compression_method_t> compression_list; // As given on the cmd line
  compression_method_e compression; // For this very track

  vector<track_name_t> track_names; // As given on the command line
  string track_name;            // For this very track

  vector<ext_timecodes_t> all_ext_timecodes; // As given on the command line
  string ext_timecodes;         // For this very track

  vector<pixel_crop_t> pixel_crop_list; // As given on the command line
  pixel_crop_t pixel_cropping;  // For this very track

  vector<default_duration_t> default_durations; // As given on the command line
  vector<max_blockadd_id_t> max_blockadd_ids; // As given on the command line

  bool no_chapters, no_attachments, no_tags;

  // Some file formats can contain chapters, but for some the charset
  // cannot be identified unambiguously (*cough* OGM *cough*).
  string chapter_charset;

  // The following variables are needed for the broken way of
  // syncing audio in AVIs: by prepending it with trash. Thanks to
  // the nandub author for this really, really sucky implementation.
  uint16_t avi_block_align;
  uint32_t avi_samples_per_sec;
  uint32_t avi_avg_bytes_per_sec;
  uint32_t avi_samples_per_chunk;
  vector<int64_t> avi_block_sizes;
  bool avi_audio_sync_enabled;

public:
  track_info_c();
  track_info_c(const track_info_c &src):
    initialized(false) {
    *this = src;
  }
  virtual ~track_info_c() {
    free_contents();
  }

  track_info_c &operator =(const track_info_c &src);
  virtual void free_contents();
};

#define PTZR(i) reader_packetizers[i]
#define PTZR0 PTZR(0)
#define NPTZR() reader_packetizers.size()

class generic_reader_c {
public:
  track_info_c ti;
  vector<generic_packetizer_c *> reader_packetizers;
  generic_packetizer_c *ptzr_first_packet;
  vector<int64_t> requested_track_ids, available_track_ids, used_track_ids;
  int64_t max_timecode_seen;
  KaxChapters *chapters;
  bool appending;
  int num_video_tracks, num_audio_tracks, num_subtitle_tracks;

public:
  generic_reader_c(track_info_c &_ti);
  virtual ~generic_reader_c();

  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false)
    = 0;
  virtual void read_all();
  virtual int get_progress() = 0;
  virtual void set_headers();
  virtual void set_headers_for_track(int64_t tid);
  virtual void identify() = 0;
  virtual void create_packetizer(int64_t tid) = 0;
  virtual void create_packetizers() {
    create_packetizer(0);
  }

  virtual void add_attachments(KaxAttachments *a) {
  }
  virtual int add_packetizer(generic_packetizer_c *ptzr);
  virtual void set_timecode_offset(int64_t offset);

  virtual void check_track_ids_and_packetizers();
  virtual void add_requested_track_id(int64_t id);
  virtual void add_available_track_ids() {
    available_track_ids.push_back(0);
  }

  virtual int64_t get_queued_bytes();

protected:
  virtual bool demuxing_requested(char type, int64_t id);
};

enum connection_result_e {
  CAN_CONNECT_YES,
  CAN_CONNECT_NO_FORMAT,
  CAN_CONNECT_NO_PARAMETERS
};

#define connect_check_a_samplerate(a, b) \
  if ((a) != (b)) { \
    error_message = mxsprintf("The sample rate of the two audio tracks is " \
                              "different: %d and %d", (int)(a), (int)(b)); \
    return CAN_CONNECT_NO_PARAMETERS; \
  }
#define connect_check_a_channels(a, b) \
  if ((a) != (b)) { \
    error_message = mxsprintf("The number of channels of the two audio " \
                              "tracks is different: %d and %d", (int)(a), \
                              (int)(b)); \
    return CAN_CONNECT_NO_PARAMETERS; \
  }
#define connect_check_a_bitdepth(a, b) \
  if ((a) != (b)) { \
    error_message = mxsprintf("The number of bits per sample of the two " \
                              "audio tracks is different: %d and %d", \
                              (int)(a), (int)(b)); \
    return CAN_CONNECT_NO_PARAMETERS; \
  }
#define connect_check_v_width(a, b) \
  if ((a) != (b)) { \
    error_message = mxsprintf("The width of the two  tracks is " \
                              "different: %d and %d", (int)(a), (int)(b)); \
    return CAN_CONNECT_NO_PARAMETERS; \
  }
#define connect_check_v_height(a, b) \
  if ((a) != (b)) { \
    error_message = mxsprintf("The height of the two tracks is " \
                              "different: %d and %d", (int)(a), (int)(b)); \
    return CAN_CONNECT_NO_PARAMETERS; \
  }
#define connect_check_codec_id(a, b) \
  if ((a) != (b)) { \
    error_message = mxsprintf("The CodecID of the two tracks is different: " \
                              "%s and %s", (a).c_str(), (b).c_str()); \
    return CAN_CONNECT_NO_PARAMETERS; \
  }

class generic_packetizer_c {
protected:
  deque<packet_t *> packet_queue, deferred_packets;

  int64_t initial_displacement;
  int64_t m_free_refs, m_next_free_refs, enqueued_bytes;
  int64_t safety_last_timecode, safety_last_duration;

  KaxTrackEntry *track_entry;

  // Header entries. Can be set via set_XXX and will be 'rendered'
  // by set_headers().
  int hserialno, htrack_type, htrack_min_cache, htrack_max_cache;
  int64_t htrack_default_duration;
  bool default_duration_forced;
  bool default_track_warning_printed;
  uint32_t huid;
  int htrack_max_add_block_ids;

  string hcodec_id;
  unsigned char *hcodec_private;
  int hcodec_private_length;

  float haudio_sampling_freq, haudio_output_sampling_freq;
  int haudio_channels, haudio_bit_depth;

  int hvideo_pixel_width, hvideo_pixel_height;
  int hvideo_display_width, hvideo_display_height;

  compression_method_e hcompression;
  compressor_ptr compressor;

  timecode_factory_c *timecode_factory;

  int64_t last_cue_timecode;

public:
  track_info_c ti;
  generic_reader_c *reader;
  int connected_to;
  int64_t correction_timecode_offset;
  int64_t append_timecode_offset, max_timecode_seen;
  bool relaxed_timecode_checking;

public:
  generic_packetizer_c(generic_reader_c *nreader, track_info_c &_ti)
    throw (error_c);
  virtual ~generic_packetizer_c();

  virtual bool contains_gap();

  virtual file_status_e read() {
    return reader->read(this);
  }

  virtual void add_packet(memory_c &mem, int64_t timecode,
                          int64_t duration, bool duration_mandatory = false,
                          int64_t bref = -1, int64_t fref = -1,
                          int ref_priority = -1);
  virtual void add_packet(memories_c &mems, int64_t timecode,
                          int64_t duration, bool duration_mandatory = false,
                          int64_t bref = -1, int64_t fref = -1,
                          int ref_priority = -1);
  virtual void add_packet2(packet_t *pack);
  virtual void process_deferred_packets();

  virtual packet_t *get_packet();
  virtual int packet_available() {
    return packet_queue.size();
  }
  virtual void flush() {
  }
  virtual int64_t get_smallest_timecode() {
    return (packet_queue.size() == 0) ? 0x0FFFFFFF :
      packet_queue.front()->timecode;
  }
  virtual int64_t get_queued_bytes() {
    return enqueued_bytes;
  }

  virtual void set_free_refs(int64_t free_refs) {
    m_free_refs = m_next_free_refs;
    m_next_free_refs = free_refs;
  }
  virtual int64_t get_free_refs() {
    return m_free_refs;
  }
  virtual void set_headers();
  virtual void fix_headers();
  virtual int process(memory_c &mem,
                      int64_t timecode = -1, int64_t length = -1,
                      int64_t bref = -1, int64_t fref = -1) = 0;

  virtual int process(memories_c &mems,
                      int64_t timecode = -1, int64_t length = -1,
                      int64_t bref = -1, int64_t fref = -1) {
    return process(*mems[0], timecode, length, bref, fref);
  }

  virtual void dump_debug_info() = 0;

  virtual void set_cue_creation(cue_strategy_e create_cue_data) {
    ti.cues = create_cue_data;
  }
  virtual cue_strategy_e get_cue_creation() {
    return ti.cues;
  }
  virtual int64_t get_last_cue_timecode() {
    return last_cue_timecode;
  }
  virtual void set_last_cue_timecode(int64_t timecode) {
    last_cue_timecode = timecode;
  }

  virtual KaxTrackEntry *get_track_entry() {
    return track_entry;
  }
  virtual int get_track_num() {
    return hserialno;
  }
  virtual int64_t get_source_track_num() {
    return ti.id;
  }

  virtual int set_uid(uint32_t uid);
  virtual int get_uid() {
    return huid;
  }
  virtual void set_track_type(int type);
  virtual int get_track_type() {
    return htrack_type;
  }
  virtual void set_language(const string &language);

  virtual void set_codec_id(const string &id);
  virtual void set_codec_private(const unsigned char *cp, int length);

  virtual void set_track_min_cache(int min_cache);
  virtual void set_track_max_cache(int max_cache);
  virtual void set_track_default_duration(int64_t default_duration);
  virtual void set_track_max_additionals(int max_add_block_ids);
  virtual int64_t get_track_default_duration();

  virtual void set_audio_sampling_freq(float freq);
  virtual float get_audio_sampling_freq() {
    return haudio_sampling_freq;
  }
  virtual void set_audio_output_sampling_freq(float freq);
  virtual void set_audio_channels(int channels);
  virtual void set_audio_bit_depth(int bit_depth);

  virtual void set_video_pixel_width(int width);
  virtual void set_video_pixel_height(int height);
  virtual void set_video_display_width(int width);
  virtual void set_video_display_height(int height);
  virtual void set_video_aspect_ratio(float ar) {
    ti.aspect_ratio = ar;
  }
  virtual void set_video_pixel_cropping(int left, int top, int right,
                                        int bottom);

  virtual void set_as_default_track(int type, int priority);

  virtual void set_tag_track_uid();

  virtual void set_track_name(const string &name);

  virtual void set_default_compression_method(compression_method_e method) {
    hcompression = method;
  }

  inline bool needs_negative_displacement(float) {
    return ((initial_displacement < 0) &&
            (ti.async.displacement > initial_displacement));
  }
  inline bool needs_positive_displacement(float duration) {
    return ((initial_displacement > 0) &&
            (iabs(ti.async.displacement - initial_displacement) >
             (duration / 2)));
  }
  virtual void displace(float by_ns);

  virtual void force_duration_on_last_packet();

  virtual const char *get_format_name() = 0;
  virtual connection_result_e can_connect_to(generic_packetizer_c *src,
                                             string &error_message) = 0;
  virtual void connect(generic_packetizer_c *src,
                       int64_t _append_timecode_offset = -1);

  virtual void enable_avi_audio_sync(bool enable) {
    ti.avi_audio_sync_enabled = enable;
  }
  virtual int64_t handle_avi_audio_sync(int64_t num_bytes, bool vbr);
  virtual void add_avi_block_size(int64_t block_size) {
    if (ti.avi_audio_sync_enabled)
      ti.avi_block_sizes.push_back(block_size);
  }

  virtual void set_displacement_maybe(int64_t displacement);
};

extern vector<generic_packetizer_c *> ptzrs_in_header_order;

#endif  // __PR_GENERIC_H
