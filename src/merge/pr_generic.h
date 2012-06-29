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

#include "common/common_pch.h"

#include <deque>

#include <matroska/KaxAttachments.h>
#include <matroska/KaxBlock.h>
#include <matroska/KaxChapters.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTags.h>

#include "common/chapters/chapters.h"
#include "common/compression.h"
#include "common/stereo_mode.h"
#include "common/strings/editing.h"
#include "common/tags/tags.h"
#include "merge/item_selector.h"
#include "merge/packet.h"
#include "merge/timecode_factory.h"
#include "merge/webm.h"

using namespace libmatroska;

namespace mtx {
  namespace input {
    class exception: public mtx::exception {
    public:
      virtual const char *what() const throw() {
        return "unspecified reader error";
      }
    };

    class open_x: public exception {
    public:
      virtual const char *what() const throw() {
        return "open error";
      }
    };

    class invalid_format_x: public exception {
    public:
      virtual const char *what() const throw() {
        return "invalid format";
      }
    };

    class header_parsing_x: public exception {
    public:
      virtual const char *what() const throw() {
        return "headers could not be parsed or were incomplete";
      }
    };

    class extended_x: public exception {
    protected:
      std::string m_message;
    public:
      extended_x(const std::string &message)  : m_message(message)       { }
      extended_x(const boost::format &message): m_message(message.str()) { }
      virtual ~extended_x() throw() { }

      virtual const char *what() const throw() {
        return m_message.c_str();
      }
    };
  }
}

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
#define ID_RESULT_CHAPTERS        "chapters"
#define ID_RESULT_TAGS            "tags"
#define ID_RESULT_GLOBAL_TAGS_ID  -1

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

enum parameter_source_e {
  PARAMETER_SOURCE_NONE      = 0,
  PARAMETER_SOURCE_BITSTREAM = 1,
  PARAMETER_SOURCE_CONTAINER = 2,
  PARAMETER_SOURCE_CMDLINE   = 3,
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

enum attach_mode_e {
  ATTACH_MODE_SKIP,
  ATTACH_MODE_TO_FIRST_FILE,
  ATTACH_MODE_TO_ALL_FILES,
};

class track_info_c {
protected:
  bool m_initialized;

public:
  // The track ID.
  int64_t m_id;

  // Options used by the readers.
  std::string m_fname;
  item_selector_c<bool> m_atracks, m_vtracks, m_stracks, m_btracks, m_track_tags;
  bool m_disable_multi_file;

  // Options used by the packetizers.
  unsigned char *m_private_data;
  size_t m_private_size;

  std::map<int64_t, std::string> m_all_fourccs;
  std::string m_fourcc;
  std::map<int64_t, display_properties_t> m_display_properties;
  float m_aspect_ratio;
  int m_display_width, m_display_height;
  bool m_aspect_ratio_given, m_aspect_ratio_is_factor, m_display_dimensions_given;
  parameter_source_e m_display_dimensions_source;

  std::map<int64_t, timecode_sync_t> m_timecode_syncs; // As given on the command line
  timecode_sync_t m_tcsync;                       // For this very track

  std::map<int64_t, bool> m_reset_timecodes_specs;
  bool m_reset_timecodes;

  std::map<int64_t, cue_strategy_e> m_cue_creations; // As given on the command line
  cue_strategy_e m_cues;          // For this very track

  std::map<int64_t, bool> m_default_track_flags; // As given on the command line
  boost::logic::tribool m_default_track;    // For this very track

  std::map<int64_t, bool> m_forced_track_flags; // As given on the command line
  boost::logic::tribool m_forced_track;    // For this very track

  std::map<int64_t, bool> m_enabled_track_flags; // As given on the command line
  boost::logic::tribool m_enabled_track;    // For this very track

  std::map<int64_t, std::string> m_languages; // As given on the command line
  std::string m_language;              // For this very track

  std::map<int64_t, std::string> m_sub_charsets; // As given on the command line
  std::string m_sub_charset;           // For this very track

  std::map<int64_t, std::string> m_all_tags;     // As given on the command line
  std::string m_tags_file_name;        // For this very track
  kax_tags_cptr m_tags;                // For this very track

  std::map<int64_t, bool> m_all_aac_is_sbr; // For AAC+/HE-AAC/SBR

  std::map<int64_t, compression_method_e> m_compression_list; // As given on the cmd line
  compression_method_e m_compression; // For this very track

  std::map<int64_t, std::string> m_track_names; // As given on the command line
  std::string m_track_name;            // For this very track

  std::map<int64_t, std::string> m_all_ext_timecodes; // As given on the command line
  std::string m_ext_timecodes;         // For this very track

  std::map<int64_t, pixel_crop_t> m_pixel_crop_list; // As given on the command line
  pixel_crop_t m_pixel_cropping;  // For this very track
  parameter_source_e m_pixel_cropping_source;

  std::map<int64_t, stereo_mode_c::mode> m_stereo_mode_list; // As given on the command line
  stereo_mode_c::mode m_stereo_mode;                    // For this very track
  parameter_source_e m_stereo_mode_source;

  std::map<int64_t, int64_t> m_default_durations; // As given on the command line
  std::map<int64_t, int> m_max_blockadd_ids; // As given on the command line

  std::map<int64_t, int> m_nalu_size_lengths;
  int m_nalu_size_length;

  item_selector_c<attach_mode_e> m_attach_mode_list; // As given on the command line

  bool m_no_chapters, m_no_global_tags;

  // Some file formats can contain chapters, but for some the charset
  // cannot be identified unambiguously (*cough* OGM *cough*).
  std::string m_chapter_charset, m_chapter_language;

  // The following variables are needed for the broken way of
  // syncing audio in AVIs: by prepending it with trash. Thanks to
  // the nandub author for this really, really sucky implementation.
  uint16_t m_avi_block_align, m_avi_samples_per_sec, m_avi_avg_bytes_per_sec, m_avi_samples_per_chunk,  m_avi_sample_scale;
  std::vector<int64_t> m_avi_block_sizes;
  bool m_avi_audio_sync_enabled;

public:
  track_info_c();
  track_info_c(const track_info_c &src)
    : m_initialized(false)
  {
    *this = src;
  }
  virtual ~track_info_c() {
    free_contents();
  }

  track_info_c &operator =(const track_info_c &src);
  virtual void free_contents();
  virtual bool display_dimensions_or_aspect_ratio_set();
};

#define PTZR(i) m_reader_packetizers[i]
#define PTZR0   PTZR(0)
#define NPTZR() m_reader_packetizers.size()

#define show_demuxer_info() \
  if (verbose) \
    mxinfo_fn(m_ti.m_fname, boost::format(Y("Using the demultiplexer for the format '%1%'.\n")) % get_format_name());
#define show_packetizer_info(track_id, packetizer) \
  mxinfo_tid(m_ti.m_fname, track_id, boost::format(Y("Using the output module for the format '%1%'.\n")) % packetizer->get_format_name());

class mm_multi_file_io_c;

class generic_reader_c {
public:
  track_info_c m_ti;
  mm_io_cptr m_in;
  uint64_t m_size;

  std::vector<generic_packetizer_c *> m_reader_packetizers;
  generic_packetizer_c *m_ptzr_first_packet;
  std::vector<int64_t> m_requested_track_ids, m_available_track_ids, m_used_track_ids;
  int64_t m_max_timecode_seen;
  kax_chapters_cptr m_chapters;
  bool m_appending;
  int m_num_video_tracks, m_num_audio_tracks, m_num_subtitle_tracks;

  int64_t m_reference_timecode_tolerance;

private:
  id_result_t m_id_results_container;
  std::vector<id_result_t> m_id_results_tracks, m_id_results_attachments, m_id_results_chapters, m_id_results_tags;

public:
  generic_reader_c(const track_info_c &ti, const mm_io_cptr &in);
  virtual ~generic_reader_c();

  virtual const std::string get_format_name(bool translate = true) = 0;

  virtual void read_headers() = 0;
  virtual file_status_e read(generic_packetizer_c *ptzr, bool force = false) = 0;
  virtual void read_all();
  virtual int get_progress();
  virtual void set_headers();
  virtual void set_headers_for_track(int64_t tid);
  virtual void identify() = 0;
  virtual void create_packetizer(int64_t tid) = 0;
  virtual void create_packetizers() {
    create_packetizer(0);
  }

  virtual int add_packetizer(generic_packetizer_c *ptzr);
  virtual size_t get_num_packetizers() const;
  virtual void set_timecode_offset(int64_t offset);

  virtual void check_track_ids_and_packetizers();
  virtual void add_requested_track_id(int64_t id);
  virtual void add_available_track_id(int64_t id);
  virtual void add_available_track_ids();
  virtual void add_available_track_id_range(int64_t start, int64_t end);
  virtual void add_available_track_id_range(int64_t num) {
    add_available_track_id_range(0, num - 1);
  }

  virtual int64_t get_queued_bytes();
  virtual bool is_simple_subtitle_container() {
    return false;
  }

  virtual file_status_e flush_packetizer(int num);
  virtual file_status_e flush_packetizer(generic_packetizer_c *ptzr);
  virtual file_status_e flush_packetizers();

  virtual attach_mode_e attachment_requested(int64_t id);

  virtual void display_identification_results();

protected:
  virtual bool demuxing_requested(char type, int64_t id);

  virtual void id_result_container(const std::string &verbose_info = empty_string);
  virtual void id_result_container(const std::vector<std::string> &verbose_info);
  virtual void id_result_track(int64_t track_id, const std::string &type, const std::string &info, const std::string &verbose_info = empty_string);
  virtual void id_result_track(int64_t track_id, const std::string &type, const std::string &info, const std::vector<std::string> &verbose_info);
  virtual void id_result_attachment(int64_t attachment_id, const std::string &type, int size, const std::string &file_name = empty_string, const std::string &description = empty_string);
  virtual void id_result_chapters(int num_entries);
  virtual void id_result_tags(int64_t track_id, int num_entries);

  virtual std::string id_escape_string(const std::string &s);

  virtual mm_multi_file_io_c *get_underlying_input_as_multi_file_io() const;
};

void id_result_container_unsupported(const std::string &filename, const std::string &info);

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
#define connect_check_codec_private(b)                                                                                                                                \
  if (   (!!this->m_ti.m_private_data != !!b->m_ti.m_private_data)                                                                                                    \
      || (  this->m_ti.m_private_size !=   b->m_ti.m_private_size)                                                                                                    \
      || (  this->m_ti.m_private_data &&   memcmp(this->m_ti.m_private_data, b->m_ti.m_private_data, this->m_ti.m_private_size))) {                                   \
    error_message = (boost::format(Y("The codec's private data does not match (lengths: %1% and %2%).")) % this->m_ti.m_private_size % b->m_ti.m_private_size).str(); \
    return CAN_CONNECT_MAYBE_CODECPRIVATE;                                                                                                                            \
  }

typedef std::deque<packet_cptr>::iterator packet_cptr_di;

class generic_packetizer_c {
protected:
  int m_num_packets;
  std::deque<packet_cptr> m_packet_queue, m_deferred_packets;
  int m_next_packet_wo_assigned_timecode;

  int64_t m_free_refs, m_next_free_refs, m_enqueued_bytes;
  int64_t m_safety_last_timecode, m_safety_last_duration;

  KaxTrackEntry *m_track_entry;

  // Header entries. Can be set via set_XXX and will be 'rendered'
  // by set_headers().
  int m_hserialno, m_htrack_type, m_htrack_min_cache, m_htrack_max_cache;
  int64_t m_htrack_default_duration;
  bool m_default_duration_forced;
  bool m_default_track_warning_printed;
  uint32_t m_huid;
  int m_htrack_max_add_block_ids;

  std::string m_hcodec_id;
  unsigned char *m_hcodec_private;
  size_t m_hcodec_private_length;

  float m_haudio_sampling_freq, m_haudio_output_sampling_freq;
  int m_haudio_channels, m_haudio_bit_depth;

  int m_hvideo_interlaced_flag, m_hvideo_pixel_width, m_hvideo_pixel_height, m_hvideo_display_width, m_hvideo_display_height;

  compression_method_e m_hcompression;
  compressor_ptr m_compressor;

  timecode_factory_cptr m_timecode_factory;
  timecode_factory_application_e m_timecode_factory_application_mode;

  int64_t m_last_cue_timecode;

  bool m_has_been_flushed;

public:
  track_info_c m_ti;
  generic_reader_c *m_reader;
  int m_connected_to;
  int64_t m_correction_timecode_offset;
  int64_t m_append_timecode_offset, m_max_timecode_seen;
  bool m_relaxed_timecode_checking;

public:
  generic_packetizer_c(generic_reader_c *reader, track_info_c &ti);
  virtual ~generic_packetizer_c();

  virtual bool contains_gap();

  virtual file_status_e read() {
    return m_reader->read(this);
  }

  inline void add_packet(packet_t *packet) {
    add_packet(packet_cptr(packet));
  }
  virtual void add_packet(packet_cptr packet);
  virtual void add_packet2(packet_cptr pack);
  virtual void process_deferred_packets();

  virtual packet_cptr get_packet();
  inline bool packet_available() {
    return !m_packet_queue.empty() && m_packet_queue.front()->factory_applied;
  }
  void flush();
  virtual int64_t get_smallest_timecode() {
    return m_packet_queue.empty() ? 0x0FFFFFFF : m_packet_queue.front()->timecode;
  }
  inline int64_t get_queued_bytes() {
    return m_enqueued_bytes;
  }

  inline void set_free_refs(int64_t free_refs) {
    m_free_refs      = m_next_free_refs;
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
    m_ti.m_cues = create_cue_data;
  }
  virtual cue_strategy_e get_cue_creation() {
    return m_ti.m_cues;
  }
  virtual int64_t get_last_cue_timecode() {
    return m_last_cue_timecode;
  }
  virtual void set_last_cue_timecode(int64_t timecode) {
    m_last_cue_timecode = timecode;
  }

  virtual KaxTrackEntry *get_track_entry() {
    return m_track_entry;
  }
  virtual int get_track_num() {
    return m_hserialno;
  }
  virtual int64_t get_source_track_num() {
    return m_ti.m_id;
  }

  virtual int set_uid(uint32_t uid);
  virtual int get_uid() {
    return m_huid;
  }
  virtual void set_track_type(int type, timecode_factory_application_e tfa_mode = TFA_AUTOMATIC);
  virtual int get_track_type() {
    return m_htrack_type;
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
  virtual void set_track_enabled_flag(bool enabled_track);

  virtual void set_audio_sampling_freq(float freq);
  virtual float get_audio_sampling_freq() {
    return m_haudio_sampling_freq;
  }
  virtual void set_audio_output_sampling_freq(float freq);
  virtual void set_audio_channels(int channels);
  virtual void set_audio_bit_depth(int bit_depth);

  virtual void set_video_interlaced_flag(bool interlaced);
  virtual void set_video_pixel_width(int width);
  virtual void set_video_pixel_height(int height);
  virtual void set_video_pixel_dimensions(int width, int height);
  virtual void set_video_display_width(int width);
  virtual void set_video_display_height(int height);
  virtual void set_video_display_dimensions(int width, int height, parameter_source_e source);
  virtual void set_video_aspect_ratio(double aspect_ratio, bool is_factor, parameter_source_e source);
  virtual void set_video_pixel_cropping(int left, int top, int right, int bottom, parameter_source_e source);
  virtual void set_video_pixel_cropping(const pixel_crop_t &cropping, parameter_source_e source);
  virtual void set_video_stereo_mode(stereo_mode_c::mode stereo_mode, parameter_source_e source);
  virtual void set_video_stereo_mode_impl(EbmlMaster &video, stereo_mode_c::mode stereo_mode);

  virtual void set_as_default_track(int type, int priority);

  virtual void set_tag_track_uid();

  virtual void set_track_name(const std::string &name);

  virtual void set_default_compression_method(compression_method_e method) {
    if (COMPRESSION_UNSPECIFIED == m_hcompression)
      m_hcompression = method;
  }

  virtual void force_duration_on_last_packet();

  virtual const std::string get_format_name(bool translate = true) = 0;
  virtual connection_result_e can_connect_to(generic_packetizer_c *src, std::string &error_message) = 0;
  virtual void connect(generic_packetizer_c *src, int64_t append_timecode_offset = -1);

  virtual void enable_avi_audio_sync(bool enable) {
    m_ti.m_avi_audio_sync_enabled = enable;
  }
  virtual int64_t handle_avi_audio_sync(int64_t num_bytes, bool vbr);
  virtual void add_avi_block_size(int64_t block_size) {
    if (m_ti.m_avi_audio_sync_enabled)
      m_ti.m_avi_block_sizes.push_back(block_size);
  }

  virtual void set_displacement_maybe(int64_t displacement);

  virtual void apply_factory();
  virtual void apply_factory_once(packet_cptr &packet);
  virtual void apply_factory_short_queueing(packet_cptr_di &p_start);
  virtual void apply_factory_full_queueing(packet_cptr_di &p_start);

  virtual bool display_dimensions_or_aspect_ratio_set();

  virtual bool is_compatible_with(output_compatibility_e compatibility);

protected:
  virtual void flush_impl() {
  };
};

extern std::vector<generic_packetizer_c *> ptzrs_in_header_order;

#endif  // __PR_GENERIC_H
