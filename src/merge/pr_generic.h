/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   class definition for the generic reader and packetizer

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Modified by Steve Lhomme <steve.lhomme@free.fr>.
*/

#ifndef __PR_GENERIC_H
#define __PR_GENERIC_H

#include "common/os.h"

#include <boost/logic/tribool.hpp>
#include <deque>
#include <map>
#include <vector>

#include <matroska/KaxAttachments.h>
#include <matroska/KaxBlock.h>
#include <matroska/KaxChapters.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTags.h>

#include "common/compression.h"
#include "common/error.h"
#include "common/memory.h"
#include "common/mm_io.h"
#include "common/smart_pointers.h"
#include "common/strings/editing.h"
#include "merge/packet.h"
#include "merge/timecode_factory.h"

using namespace libmatroska;

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

#define ID_RESULT_TRACK_AUDIO     "audio"
#define ID_RESULT_TRACK_VIDEO     "video"
#define ID_RESULT_TRACK_SUBTITLES "subtitles"
#define ID_RESULT_TRACK_BUTTONS   "buttons"
#define ID_RESULT_TRACK_UNKNOWN   "unknown"

struct id_result_t {
  int64_t id;
  std::string type, info, description;
  std::vector<std::string> verbose_info;
  int64_t size;

  id_result_t() {
  };

  id_result_t(int64_t p_id,
              const std::string &p_type,
              const std::string &p_info,
              const std::string &p_description,
              int64_t p_size)
    : id(p_id)
    , type(p_type)
    , info(p_info)
    , description(p_description)
    , size(p_size)
  {
  }

  id_result_t(const id_result_t &src)
    : id(src.id)
    , type(src.type)
    , info(src.info)
    , description(src.description)
    , verbose_info(src.verbose_info)
    , size(src.size)
  {
  }
};

class generic_packetizer_c;
class generic_reader_c;

enum file_status_e {
  FILE_STATUS_DONE         = 0,
  FILE_STATUS_DONE_AND_DRY,
  FILE_STATUS_HOLDING,
  FILE_STATUS_MOREDATA
};

struct timecode_sync_t {
  int64_t displacement;
  double numerator, denominator;

  timecode_sync_t()
  : displacement(0)
  , numerator(1.0)
  , denominator(1.0)
  {
  }

  double factor() {
    return numerator / denominator;
  }
};

enum default_track_priority_e {
  DEFAULT_TRACK_PRIOIRTY_NONE        =   0,
  DEFAULT_TRACK_PRIORITY_FROM_TYPE   =  10,
  DEFAULT_TRACK_PRIORITY_FROM_SOURCE =  50,
  DEFAULT_TRACK_PRIORITY_CMDLINE     = 255
};

struct display_properties_t {
  float aspect_ratio;
  bool ar_factor;
  int width, height;

  display_properties_t()
  : aspect_ratio(0)
  , ar_factor(false)
  , width(0)
  , height(0)
  {
  }
};

struct pixel_crop_t {
  int left, top, right, bottom;

  pixel_crop_t()
  : left(0)
  , top(0)
  , right(0)
  , bottom(0)
  {
  }
};

enum stereo_mode_e {
  STEREO_MODE_UNSPECIFIED = -1,
  STEREO_MODE_NONE        =  0,
  STEREO_MODE_RIGHT       =  1,
  STEREO_MODE_LEFT        =  2,
  STEREO_MODE_BOTH        =  3,
};

enum attach_mode_e {
  ATTACH_MODE_SKIP,
  ATTACH_MODE_TO_FIRST_FILE,
  ATTACH_MODE_TO_ALL_FILES,
};

class track_info_c {
protected:
  bool initialized;

public:
  // The track ID.
  int64_t id;

  // Options used by the readers.
  std::string fname;
  bool no_audio, no_video, no_subs, no_buttons;
  std::vector<int64_t> atracks, vtracks, stracks, btracks;

  // Options used by the packetizers.
  unsigned char *private_data;
  int private_size;

  std::map<int64_t, std::string> all_fourccs;
  std::string fourcc;
  std::map<int64_t, display_properties_t> display_properties;
  float aspect_ratio;
  int display_width, display_height;
  bool aspect_ratio_given, aspect_ratio_is_factor, display_dimensions_given;

  std::map<int64_t, timecode_sync_t> timecode_syncs; // As given on the command line
  timecode_sync_t tcsync;                       // For this very track

  std::map<int64_t, bool> reset_timecodes_specs;
  bool reset_timecodes;

  std::map<int64_t, cue_strategy_e> cue_creations; // As given on the command line
  cue_strategy_e cues;          // For this very track

  std::map<int64_t, bool> default_track_flags; // As given on the command line
  boost::logic::tribool default_track;    // For this very track

  std::map<int64_t, bool> forced_track_flags; // As given on the command line
  boost::logic::tribool forced_track;    // For this very track

  std::map<int64_t, std::string> languages; // As given on the command line
  std::string language;              // For this very track

  std::map<int64_t, std::string> sub_charsets; // As given on the command line
  std::string sub_charset;           // For this very track

  std::map<int64_t, std::string> all_tags;     // As given on the command line
  std::string tags_file_name;        // For this very track
  KaxTags *tags;                // For this very track

  std::map<int64_t, bool> all_aac_is_sbr; // For AAC+/HE-AAC/SBR

  std::map<int64_t, compression_method_e> compression_list; // As given on the cmd line
  compression_method_e compression; // For this very track

  std::map<int64_t, std::string> track_names; // As given on the command line
  std::string track_name;            // For this very track

  std::map<int64_t, std::string> all_ext_timecodes; // As given on the command line
  std::string ext_timecodes;         // For this very track

  std::map<int64_t, pixel_crop_t> pixel_crop_list; // As given on the command line
  pixel_crop_t pixel_cropping;  // For this very track
  bool pixel_cropping_specified;

  std::map<int64_t, stereo_mode_e> stereo_mode_list; // As given on the command line
  stereo_mode_e stereo_mode;                    // For this very track

  std::map<int64_t, int64_t> default_durations; // As given on the command line
  std::map<int64_t, int> max_blockadd_ids; // As given on the command line

  std::map<int64_t, int> nalu_size_lengths;
  int nalu_size_length;

  std::map<int64_t, attach_mode_e> attach_mode_list; // As given on the command line

  bool no_chapters, no_attachments, no_tags;

  // Some file formats can contain chapters, but for some the charset
  // cannot be identified unambiguously (*cough* OGM *cough*).
  std::string chapter_charset;

  // The following variables are needed for the broken way of
  // syncing audio in AVIs: by prepending it with trash. Thanks to
  // the nandub author for this really, really sucky implementation.
  uint16_t avi_block_align;
  uint32_t avi_samples_per_sec;
  uint32_t avi_avg_bytes_per_sec;
  uint32_t avi_samples_per_chunk;
  std::vector<int64_t> avi_block_sizes;
  bool avi_audio_sync_enabled;

public:
  track_info_c();
  track_info_c(const track_info_c &src)
    : initialized(false)
  {
    *this = src;
  }
  virtual ~track_info_c() {
    free_contents();
  }

  track_info_c &operator =(const track_info_c &src);
  virtual void free_contents();
};

#define PTZR(i) reader_packetizers[i]
#define PTZR0   PTZR(0)
#define NPTZR() reader_packetizers.size()

class generic_reader_c {
public:
  track_info_c ti;
  std::vector<generic_packetizer_c *> reader_packetizers;
  generic_packetizer_c *ptzr_first_packet;
  std::vector<int64_t> requested_track_ids, available_track_ids, used_track_ids;
  int64_t max_timecode_seen;
  KaxChapters *chapters;
  bool appending;
  int num_video_tracks, num_audio_tracks, num_subtitle_tracks;

  int64_t reference_timecode_tolerance;

private:
  id_result_t id_results_container;
  std::vector<id_result_t> id_results_tracks, id_results_attachments;

public:
  generic_reader_c(track_info_c &p_ti);
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

  virtual int add_packetizer(generic_packetizer_c *ptzr);
  virtual void set_timecode_offset(int64_t offset);

  virtual void check_track_ids_and_packetizers();
  virtual void add_requested_track_id(int64_t id);
  virtual void add_available_track_ids() {
    available_track_ids.push_back(0);
  }

  virtual int64_t get_queued_bytes();

  virtual void flush_packetizers();

  virtual attach_mode_e attachment_requested(int64_t id);

  virtual void display_identification_results();

protected:
  virtual bool demuxing_requested(char type, int64_t id);

  virtual void id_result_container(const std::string &info, const std::string &verbose_info = empty_string);
  virtual void id_result_container(const std::string &info, const std::vector<std::string> &verbose_info);
  virtual void id_result_track(int64_t track_id, const std::string &type, const std::string &info, const std::string &verbose_info = empty_string);
  virtual void id_result_track(int64_t track_id, const std::string &type, const std::string &info, const std::vector<std::string> &verbose_info);
  virtual void id_result_attachment(int64_t attachment_id, const std::string &type, int size, const std::string &file_name = empty_string, const std::string &description = empty_string);

  virtual std::string id_escape_string(const std::string &s);

  static void id_result_container_unsupported(const std::string &filename, const std::string &info);
};

enum connection_result_e {
  CAN_CONNECT_YES,
  CAN_CONNECT_NO_FORMAT,
  CAN_CONNECT_NO_PARAMETERS,
  CAN_CONNECT_MAYBE_CODECPRIVATE
};

#define connect_check_a_samplerate(a, b)                                                                                       \
  if ((a) != (b)) {                                                                                                            \
    error_message = (boost::format(Y("The sample rate of the two audio tracks is different: %1% and %2%")) % (a) % (b)).str(); \
    return CAN_CONNECT_NO_PARAMETERS;                                                                                          \
  }
#define connect_check_a_channels(a, b)                                                                                                \
  if ((a) != (b)) {                                                                                                                   \
    error_message = (boost::format(Y("The number of channels of the two audio tracks is different: %1% and %2%")) % (a) % (b)).str(); \
    return CAN_CONNECT_NO_PARAMETERS;                                                                                                 \
  }
#define connect_check_a_bitdepth(a, b)                                                                                                       \
  if ((a) != (b)) {                                                                                                                          \
    error_message = (boost::format(Y("The number of bits per sample of the two audio tracks is different: %1% and %2%")) % (a) % (b)).str(); \
    return CAN_CONNECT_NO_PARAMETERS;                                                                                                        \
  }
#define connect_check_v_width(a, b)                                                                                \
  if ((a) != (b)) {                                                                                                \
    error_message = (boost::format(Y("The width of the two tracks is different: %1% and %2%")) % (a) % (b)).str(); \
    return CAN_CONNECT_NO_PARAMETERS;                                                                              \
  }
#define connect_check_v_height(a, b)                                                                                \
  if ((a) != (b)) {                                                                                                 \
    error_message = (boost::format(Y("The height of the two tracks is different: %1% and %2%")) % (a) % (b)).str(); \
    return CAN_CONNECT_NO_PARAMETERS;                                                                               \
  }
#define connect_check_v_dwidth(a, b)                                                                                       \
  if ((a) != (b)) {                                                                                                        \
    error_message = (boost::format(Y("The display width of the two tracks is different: %1% and %2%")) % (a) % (b)).str(); \
    return CAN_CONNECT_NO_PARAMETERS;                                                                                      \
  }
#define connect_check_v_dheight(a, b)                                                                                       \
  if ((a) != (b)) {                                                                                                         \
    error_message = (boost::format(Y("The display height of the two tracks is different: %1% and %2%")) % (a) % (b)).str(); \
    return CAN_CONNECT_NO_PARAMETERS;                                                                                       \
  }
#define connect_check_codec_id(a, b)                                                                                 \
  if ((a) != (b)) {                                                                                                  \
    error_message = (boost::format(Y("The CodecID of the two tracks is different: %1% and %2%")) % (a) % (b)).str(); \
    return CAN_CONNECT_NO_PARAMETERS;                                                                                \
  }

typedef std::deque<packet_cptr>::iterator packet_cptr_di;

class generic_packetizer_c {
protected:
  int m_num_packets;
  std::deque<packet_cptr> packet_queue, deferred_packets;
  int next_packet_wo_assigned_timecode;

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

  std::string hcodec_id;
  unsigned char *hcodec_private;
  int hcodec_private_length;

  float haudio_sampling_freq, haudio_output_sampling_freq;
  int haudio_channels, haudio_bit_depth;

  int hvideo_pixel_width, hvideo_pixel_height;
  int hvideo_display_width, hvideo_display_height;

  compression_method_e hcompression;
  compressor_ptr compressor;

  timecode_factory_cptr timecode_factory;
  timecode_factory_application_e timecode_factory_application_mode;

  int64_t last_cue_timecode;

  bool has_been_flushed;

public:
  track_info_c ti;
  generic_reader_c *reader;
  int connected_to;
  int64_t correction_timecode_offset;
  int64_t append_timecode_offset, max_timecode_seen;
  bool relaxed_timecode_checking;

public:
  generic_packetizer_c(generic_reader_c *p_reader, track_info_c &p_ti)
    throw (error_c);
  virtual ~generic_packetizer_c();

  virtual bool contains_gap();

  virtual file_status_e read() {
    return reader->read(this);
  }

  inline void add_packet(packet_t *packet) {
    add_packet(packet_cptr(packet));
  }
  virtual void add_packet(packet_cptr packet);
  virtual void add_packet2(packet_cptr pack);
  virtual void process_deferred_packets();

  virtual packet_cptr get_packet();
  inline bool packet_available() {
    return !packet_queue.empty() && packet_queue.front()->factory_applied;
  }
  virtual void flush();
  virtual int64_t get_smallest_timecode() {
    return packet_queue.empty() ? 0x0FFFFFFF : packet_queue.front()->timecode;
  }
  inline int64_t get_queued_bytes() {
    return enqueued_bytes;
  }

  inline void set_free_refs(int64_t free_refs) {
    m_free_refs = m_next_free_refs;
    m_next_free_refs = free_refs;
  }
  inline int64_t get_free_refs() {
    return m_free_refs;
  }
  virtual void set_headers();
  virtual void fix_headers();
  inline int process(packet_t *packet) {
    return process(packet_cptr(packet));
  }
  virtual int process(packet_cptr packet) = 0;

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
  virtual void set_track_type(int type,
                              timecode_factory_application_e tfa_mode =
                              TFA_AUTOMATIC);
  virtual int get_track_type() {
    return htrack_type;
  }
  virtual void set_language(const std::string &language);

  virtual void set_codec_id(const std::string &id);
  virtual void set_codec_private(const unsigned char *cp, int length);

  virtual void set_track_min_cache(int min_cache);
  virtual void set_track_max_cache(int max_cache);
  virtual void set_track_default_duration(int64_t default_duration);
  virtual void set_track_max_additionals(int max_add_block_ids);
  virtual int64_t get_track_default_duration();
  virtual void set_track_forced_flag(bool forced_track);

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
  virtual void set_stereo_mode(stereo_mode_e stereo_mode);

  virtual void set_as_default_track(int type, int priority);

  virtual void set_tag_track_uid();

  virtual void set_track_name(const std::string &name);

  virtual void set_default_compression_method(compression_method_e method) {
    hcompression = method;
  }

  virtual void force_duration_on_last_packet();

  virtual const char *get_format_name() = 0;
  virtual connection_result_e can_connect_to(generic_packetizer_c *src,
                                             std::string &error_message) = 0;
  virtual void connect(generic_packetizer_c *src,
                       int64_t p_append_timecode_offset = -1);

  virtual void enable_avi_audio_sync(bool enable) {
    ti.avi_audio_sync_enabled = enable;
  }
  virtual int64_t handle_avi_audio_sync(int64_t num_bytes, bool vbr);
  virtual void add_avi_block_size(int64_t block_size) {
    if (ti.avi_audio_sync_enabled)
      ti.avi_block_sizes.push_back(block_size);
  }

  virtual void set_displacement_maybe(int64_t displacement);

  virtual void apply_factory();
  virtual void apply_factory_once(packet_cptr &packet);
  virtual void apply_factory_short_queueing(packet_cptr_di &p_start);
  virtual void apply_factory_full_queueing(packet_cptr_di &p_start);
};

extern std::vector<generic_packetizer_c *> ptzrs_in_header_order;

#endif  // __PR_GENERIC_H
