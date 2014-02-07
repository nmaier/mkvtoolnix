/*
   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   functions common for all readers/packetizers

   Written by Moritz Bunkus <moritz@bunkus.org>.
   Modified by Steve Lhomme <steve.lhomme@free.fr>.
*/

#include "common/common_pch.h"

#include <algorithm>
#include <unordered_map>

#include <matroska/KaxContentEncoding.h>
#include <matroska/KaxTag.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>

#include "common/base64.h"
#include "common/compression.h"
#include "common/ebml.h"
#include "common/hacks.h"
#include "common/math.h"
#include "common/mm_multi_file_io.h"
#include "common/strings/formatting.h"
#include "common/unique_numbers.h"
#include "common/xml/ebml_tags_converter.h"
#include "merge/output_control.h"
#include "merge/pr_generic.h"
#include "merge/webm.h"

#define TRACK_TYPE_TO_DEFTRACK_TYPE(track_type)      \
  (  track_audio == track_type ? DEFTRACK_TYPE_AUDIO \
   : track_video == track_type ? DEFTRACK_TYPE_VIDEO \
   :                             DEFTRACK_TYPE_SUBS)

#define LOOKUP_TRACK_ID(container)                  \
      map_has_key(container, m_ti.m_id) ? m_ti.m_id \
    : map_has_key(container, -1)        ? -1        \
    :                                     -2

// ---------------------------------------------------------------------

static std::unordered_map<std::string, bool> s_experimental_status_warning_shown;
std::vector<generic_packetizer_c *> ptzrs_in_header_order;

// Specs say that track numbers should start at 1.
int generic_packetizer_c::ms_track_number = 1;

generic_packetizer_c::generic_packetizer_c(generic_reader_c *reader,
                                           track_info_c &ti)
  : m_num_packets(0)
  , m_next_packet_wo_assigned_timecode(0)
  , m_free_refs(-1)
  , m_next_free_refs(-1)
  , m_enqueued_bytes(0)
  , m_safety_last_timecode(0)
  , m_safety_last_duration(0)
  , m_track_entry(nullptr)
  , m_hserialno(-1)
  , m_htrack_type(-1)
  , m_htrack_min_cache(0)
  , m_htrack_max_cache(-1)
  , m_htrack_default_duration(-1)
  , m_default_duration_forced(true)
  , m_default_track_warning_printed(false)
  , m_huid(0)
  , m_htrack_max_add_block_ids(-1)
  , m_haudio_sampling_freq(-1.0)
  , m_haudio_output_sampling_freq(-1.0)
  , m_haudio_channels(-1)
  , m_haudio_bit_depth(-1)
  , m_hvideo_interlaced_flag(-1)
  , m_hvideo_pixel_width(-1)
  , m_hvideo_pixel_height(-1)
  , m_hvideo_display_width(-1)
  , m_hvideo_display_height(-1)
  , m_hcompression(COMPRESSION_UNSPECIFIED)
  , m_timecode_factory_application_mode(TFA_AUTOMATIC)
  , m_last_cue_timecode(-1)
  , m_has_been_flushed(false)
  , m_ti(ti)
  , m_reader(reader)
  , m_connected_to(0)
  , m_correction_timecode_offset(0)
  , m_append_timecode_offset(0)
  , m_max_timecode_seen(0)
  , m_relaxed_timecode_checking(false)
{
  // Let's see if the user specified timecode sync for this track.
  if (map_has_key(m_ti.m_timecode_syncs, m_ti.m_id))
    m_ti.m_tcsync = m_ti.m_timecode_syncs[m_ti.m_id];
  else if (map_has_key(m_ti.m_timecode_syncs, -1))
    m_ti.m_tcsync = m_ti.m_timecode_syncs[-1];
  if (0 == m_ti.m_tcsync.numerator)
    m_ti.m_tcsync.numerator = 1;
  if (0 == m_ti.m_tcsync.denominator)
    m_ti.m_tcsync.denominator = 1;

  // Let's see if the user specified "reset timecodes" for this track.
  m_ti.m_reset_timecodes = map_has_key(m_ti.m_reset_timecodes_specs, m_ti.m_id) || map_has_key(m_ti.m_reset_timecodes_specs, -1);

  // Let's see if the user has specified which cues he wants for this track.
  if (map_has_key(m_ti.m_cue_creations, m_ti.m_id))
    m_ti.m_cues = m_ti.m_cue_creations[m_ti.m_id];
  else if (map_has_key(m_ti.m_cue_creations, -1))
    m_ti.m_cues = m_ti.m_cue_creations[-1];

  // Let's see if the user has given a default track flag for this track.
  if (map_has_key(m_ti.m_default_track_flags, m_ti.m_id))
    m_ti.m_default_track = m_ti.m_default_track_flags[m_ti.m_id];
  else if (map_has_key(m_ti.m_default_track_flags, -1))
    m_ti.m_default_track = m_ti.m_default_track_flags[-1];

  // Let's see if the user has given a fix avc fps flag for this track.
  if (map_has_key(m_ti.m_fix_bitstream_frame_rate_flags, m_ti.m_id))
    m_ti.m_fix_bitstream_frame_rate = m_ti.m_fix_bitstream_frame_rate_flags[m_ti.m_id];
  else if (map_has_key(m_ti.m_fix_bitstream_frame_rate_flags, -1))
    m_ti.m_fix_bitstream_frame_rate = m_ti.m_fix_bitstream_frame_rate_flags[-1];

  // Let's see if the user has given a forced track flag for this track.
  if (map_has_key(m_ti.m_forced_track_flags, m_ti.m_id))
    m_ti.m_forced_track = m_ti.m_forced_track_flags[m_ti.m_id];
  else if (map_has_key(m_ti.m_forced_track_flags, -1))
    m_ti.m_forced_track = m_ti.m_forced_track_flags[-1];

  // Let's see if the user has given a enabled track flag for this track.
  if (map_has_key(m_ti.m_enabled_track_flags, m_ti.m_id))
    m_ti.m_enabled_track = m_ti.m_enabled_track_flags[m_ti.m_id];
  else if (map_has_key(m_ti.m_enabled_track_flags, -1))
    m_ti.m_enabled_track = m_ti.m_enabled_track_flags[-1];

  // Let's see if the user has specified a language for this track.
  if (map_has_key(m_ti.m_languages, m_ti.m_id))
    m_ti.m_language = m_ti.m_languages[m_ti.m_id];
  else if (map_has_key(m_ti.m_languages, -1))
    m_ti.m_language = m_ti.m_languages[-1];

  // Let's see if the user has specified a sub charset for this track.
  if (map_has_key(m_ti.m_sub_charsets, m_ti.m_id))
    m_ti.m_sub_charset = m_ti.m_sub_charsets[m_ti.m_id];
  else if (map_has_key(m_ti.m_sub_charsets, -1))
    m_ti.m_sub_charset = m_ti.m_sub_charsets[-1];

  // Let's see if the user has specified a sub charset for this track.
  if (map_has_key(m_ti.m_all_tags, m_ti.m_id))
    m_ti.m_tags_file_name = m_ti.m_all_tags[m_ti.m_id];
  else if (map_has_key(m_ti.m_all_tags, -1))
    m_ti.m_tags_file_name = m_ti.m_all_tags[-1];
  if (!m_ti.m_tags_file_name.empty())
    m_ti.m_tags = mtx::xml::ebml_tags_converter_c::parse_file(m_ti.m_tags_file_name, false);

  // Let's see if the user has specified how this track should be compressed.
  if (map_has_key(m_ti.m_compression_list, m_ti.m_id))
    m_ti.m_compression = m_ti.m_compression_list[m_ti.m_id];
  else if (map_has_key(m_ti.m_compression_list, -1))
    m_ti.m_compression = m_ti.m_compression_list[-1];

  // Let's see if the user has specified a name for this track.
  if (map_has_key(m_ti.m_track_names, m_ti.m_id))
    m_ti.m_track_name = m_ti.m_track_names[m_ti.m_id];
  else if (map_has_key(m_ti.m_track_names, -1))
    m_ti.m_track_name = m_ti.m_track_names[-1];

  // Let's see if the user has specified external timecodes for this track.
  if (map_has_key(m_ti.m_all_ext_timecodes, m_ti.m_id))
    m_ti.m_ext_timecodes = m_ti.m_all_ext_timecodes[m_ti.m_id];
  else if (map_has_key(m_ti.m_all_ext_timecodes, -1))
    m_ti.m_ext_timecodes = m_ti.m_all_ext_timecodes[-1];

  // Let's see if the user has specified an aspect ratio or display dimensions
  // for this track.
  int i = LOOKUP_TRACK_ID(m_ti.m_display_properties);
  if (-2 != i) {
    display_properties_t &dprop = m_ti.m_display_properties[i];
    if (0 > dprop.aspect_ratio) {
      set_video_display_dimensions(dprop.width, dprop.height, OPTION_SOURCE_COMMAND_LINE);
    } else {
      set_video_aspect_ratio(dprop.aspect_ratio, dprop.ar_factor, OPTION_SOURCE_COMMAND_LINE);
      m_ti.m_aspect_ratio_given = true;
    }
  }

  if (m_ti.m_aspect_ratio_given && m_ti.m_display_dimensions_given) {
    if (m_ti.m_aspect_ratio_is_factor)
      mxerror_tid(m_ti.m_fname, m_ti.m_id, boost::format(Y("Both the aspect ratio factor and '--display-dimensions' were given.\n")));
    else
      mxerror_tid(m_ti.m_fname, m_ti.m_id, boost::format(Y("Both the aspect ratio and '--display-dimensions' were given.\n")));
  }

  // Let's see if the user has specified a FourCC for this track.
  if (map_has_key(m_ti.m_all_fourccs, m_ti.m_id))
    m_ti.m_fourcc = m_ti.m_all_fourccs[m_ti.m_id];
  else if (map_has_key(m_ti.m_all_fourccs, -1))
    m_ti.m_fourcc = m_ti.m_all_fourccs[-1];

  // Let's see if the user has specified a FourCC for this track.
  i = LOOKUP_TRACK_ID(m_ti.m_pixel_crop_list);
  if (-2 != i)
    set_video_pixel_cropping(m_ti.m_pixel_crop_list[i], OPTION_SOURCE_COMMAND_LINE);

  // Let's see if the user has specified a stereo mode for this track.
  i = LOOKUP_TRACK_ID(m_ti.m_stereo_mode_list);
  if (-2 != i)
    set_video_stereo_mode(m_ti.m_stereo_mode_list[m_ti.m_id], OPTION_SOURCE_COMMAND_LINE);

  // Let's see if the user has specified a default duration for this track.
  if (map_has_key(m_ti.m_default_durations, m_ti.m_id))
    m_htrack_default_duration = m_ti.m_default_durations[m_ti.m_id];
  else if (map_has_key(m_ti.m_default_durations, -1))
    m_htrack_default_duration = m_ti.m_default_durations[-1];
  else
    m_default_duration_forced = false;

  // Let's see if the user has set a max_block_add_id
  if (map_has_key(m_ti.m_max_blockadd_ids, m_ti.m_id))
    m_htrack_max_add_block_ids = m_ti.m_max_blockadd_ids[m_ti.m_id];
  else if (map_has_key(m_ti.m_max_blockadd_ids, -1))
    m_htrack_max_add_block_ids = m_ti.m_max_blockadd_ids[-1];

  // Let's see if the user has specified a NALU size length for this track.
  if (map_has_key(m_ti.m_nalu_size_lengths, m_ti.m_id))
    m_ti.m_nalu_size_length = m_ti.m_nalu_size_lengths[m_ti.m_id];
  else if (map_has_key(m_ti.m_nalu_size_lengths, -1))
    m_ti.m_nalu_size_length = m_ti.m_nalu_size_lengths[-1];

  // Let's see if the user has specified a compression scheme for this track.
  if (COMPRESSION_UNSPECIFIED != m_ti.m_compression)
    m_hcompression = m_ti.m_compression;

  // Set default header values to 'unset'.
  if (!m_reader->m_appending) {
    m_hserialno                             = create_track_number();
    g_packetizers_by_track_num[m_hserialno] = this;
  }

  m_timecode_factory = timecode_factory_c::create(m_ti.m_ext_timecodes, m_ti.m_fname, m_ti.m_id);

  // If no external timecode file but a default duration has been
  // given then create a simple timecode factory that generates the
  // timecodes for the given FPS.
  if (!m_timecode_factory && (-1 != m_htrack_default_duration))
    m_timecode_factory = timecode_factory_c::create_fps_factory(m_htrack_default_duration, m_ti.m_fname, m_ti.m_id);
}

generic_packetizer_c::~generic_packetizer_c() {
  if (!m_packet_queue.empty())
    mxerror_tid(m_ti.m_fname, m_ti.m_id, boost::format(Y("Packet queue not empty (flushed: %1%). Frames have been lost during remux. %2%\n")) % m_has_been_flushed % BUGMSG);
}

void
generic_packetizer_c::set_tag_track_uid() {
  if (!m_ti.m_tags)
    return;

  convert_old_tags(*m_ti.m_tags);
  size_t idx_tags;
  for (idx_tags = 0; m_ti.m_tags->ListSize() > idx_tags; ++idx_tags) {
    KaxTag *tag = (KaxTag *)(*m_ti.m_tags)[idx_tags];

    remove_track_uid_tag_targets(tag);

    GetChild<KaxTagTrackUID>(GetChild<KaxTagTargets>(tag)).SetValue(m_huid);

    fix_mandatory_tag_elements(tag);

    if (!tag->CheckMandatory())
      mxerror(boost::format(Y("The tags in '%1%' could not be parsed: some mandatory elements are missing.\n"))
              % (m_ti.m_tags_file_name != "" ? m_ti.m_tags_file_name : m_ti.m_fname));
  }
}

bool
generic_packetizer_c::set_uid(uint64_t uid) {
  if (!is_unique_number(uid, UNIQUE_TRACK_IDS))
    return false;

  add_unique_number(uid, UNIQUE_TRACK_IDS);
  m_huid = uid;
  if (m_track_entry)
    GetChild<KaxTrackUID>(m_track_entry).SetValue(m_huid);

  return true;
}

void
generic_packetizer_c::set_track_type(int type,
                                     timecode_factory_application_e tfa_mode) {
  m_htrack_type = type;

  if (CUE_STRATEGY_UNSPECIFIED == get_cue_creation())
    m_ti.m_cues = track_audio    == type ? CUE_STRATEGY_SPARSE
                : track_video    == type ? CUE_STRATEGY_IFRAMES
                : track_subtitle == type ? CUE_STRATEGY_IFRAMES
                :                          CUE_STRATEGY_UNSPECIFIED;

  if (track_audio == type)
    m_reader->m_num_audio_tracks++;

  else if (track_video == type) {
    m_reader->m_num_video_tracks++;
    if (!g_video_packetizer)
      g_video_packetizer = this;

  } else
    m_reader->m_num_subtitle_tracks++;

  if (   (TFA_AUTOMATIC == tfa_mode)
      && (TFA_AUTOMATIC == m_timecode_factory_application_mode))
    m_timecode_factory_application_mode
      = (track_video    == type) ? TFA_FULL_QUEUEING
      : (track_subtitle == type) ? TFA_IMMEDIATE
      : (track_buttons  == type) ? TFA_IMMEDIATE
      :                            TFA_FULL_QUEUEING;

  else if (TFA_AUTOMATIC != tfa_mode)
    m_timecode_factory_application_mode = tfa_mode;

  if (m_timecode_factory && (track_video != type) && (track_audio != type))
    m_timecode_factory->set_preserve_duration(true);
}

void
generic_packetizer_c::set_track_name(const std::string &name) {
  m_ti.m_track_name = name;
  if (m_track_entry && !name.empty())
    GetChild<KaxTrackName>(m_track_entry).SetValue(cstrutf8_to_UTFstring(m_ti.m_track_name));
}

void
generic_packetizer_c::set_codec_id(const std::string &id) {
  m_hcodec_id = id;
  if (m_track_entry && !id.empty())
    GetChild<KaxCodecID>(m_track_entry).SetValue(m_hcodec_id);
}

void
generic_packetizer_c::set_codec_private(memory_cptr const &buffer) {
  if (buffer && buffer->get_size()) {
    m_hcodec_private = buffer->clone();

    if (m_track_entry)
      GetChild<KaxCodecPrivate>(*m_track_entry).CopyBuffer(static_cast<binary *>(m_hcodec_private->get_buffer()), m_hcodec_private->get_size());

  } else
    m_hcodec_private.reset();
}

void
generic_packetizer_c::set_track_min_cache(int min_cache) {
  m_htrack_min_cache = min_cache;
  if (m_track_entry)
    GetChild<KaxTrackMinCache>(m_track_entry).SetValue(min_cache);
}

void
generic_packetizer_c::set_track_max_cache(int max_cache) {
  m_htrack_max_cache = max_cache;
  if (m_track_entry)
    GetChild<KaxTrackMaxCache>(m_track_entry).SetValue(max_cache);
}

void
generic_packetizer_c::set_track_default_duration(int64_t def_dur) {
  if (m_default_duration_forced)
    return;

  m_htrack_default_duration = (int64_t)(def_dur * m_ti.m_tcsync.numerator / m_ti.m_tcsync.denominator);

  if (m_track_entry)
    GetChild<KaxTrackDefaultDuration>(m_track_entry).SetValue(m_htrack_default_duration);
}

void
generic_packetizer_c::set_track_max_additionals(int max_add_block_ids) {
  m_htrack_max_add_block_ids = max_add_block_ids;
  if (m_track_entry)
    GetChild<KaxMaxBlockAdditionID>(m_track_entry).SetValue(max_add_block_ids);
}

int64_t
generic_packetizer_c::get_track_default_duration()
  const {
  return m_htrack_default_duration;
}

void
generic_packetizer_c::set_track_forced_flag(bool forced_track) {
  m_ti.m_forced_track = forced_track;
  if (m_track_entry)
    GetChild<KaxTrackFlagForced>(m_track_entry).SetValue(forced_track ? 1 : 0);
}

void
generic_packetizer_c::set_track_enabled_flag(bool enabled_track) {
  m_ti.m_enabled_track = enabled_track;
  if (m_track_entry)
    GetChild<KaxTrackFlagEnabled>(m_track_entry).SetValue(enabled_track ? 1 : 0);
}

void
generic_packetizer_c::set_track_seek_pre_roll(timecode_c const &seek_pre_roll) {
  m_seek_pre_roll = seek_pre_roll;
  if (m_track_entry)
    GetChild<KaxSeekPreRoll>(m_track_entry).SetValue(seek_pre_roll.to_ns());
}

void
generic_packetizer_c::set_codec_delay(timecode_c const &codec_delay) {
  m_codec_delay = codec_delay;
  if (m_track_entry)
    GetChild<KaxCodecDelay>(m_track_entry).SetValue(codec_delay.to_ns());
}

void
generic_packetizer_c::set_audio_sampling_freq(float freq) {
  m_haudio_sampling_freq = freq;
  if (m_track_entry)
    GetChild<KaxAudioSamplingFreq>(GetChild<KaxTrackAudio>(m_track_entry)).SetValue(m_haudio_sampling_freq);
}

void
generic_packetizer_c::set_audio_output_sampling_freq(float freq) {
  m_haudio_output_sampling_freq = freq;
  if (m_track_entry)
    GetChild<KaxAudioOutputSamplingFreq>(GetChild<KaxTrackAudio>(m_track_entry)).SetValue(m_haudio_output_sampling_freq);
}

void
generic_packetizer_c::set_audio_channels(int channels) {
  m_haudio_channels = channels;
  if (m_track_entry)
    GetChild<KaxAudioChannels>(GetChild<KaxTrackAudio>(*m_track_entry)).SetValue(m_haudio_channels);
}

void
generic_packetizer_c::set_audio_bit_depth(int bit_depth) {
  m_haudio_bit_depth = bit_depth;
  if (m_track_entry)
    GetChild<KaxAudioBitDepth>(GetChild<KaxTrackAudio>(*m_track_entry)).SetValue(m_haudio_bit_depth);
}

void
generic_packetizer_c::set_video_interlaced_flag(bool interlaced) {
  m_hvideo_interlaced_flag = interlaced ? 1 : 0;
  if (m_track_entry)
    GetChild<KaxVideoFlagInterlaced>(GetChild<KaxTrackVideo>(*m_track_entry)).SetValue(m_hvideo_interlaced_flag);
}

void
generic_packetizer_c::set_video_pixel_width(int width) {
  m_hvideo_pixel_width = width;
  if (m_track_entry)
    GetChild<KaxVideoPixelWidth>(GetChild<KaxTrackVideo>(*m_track_entry)).SetValue(m_hvideo_pixel_width);
}

void
generic_packetizer_c::set_video_pixel_height(int height) {
  m_hvideo_pixel_height = height;
  if (m_track_entry)
    GetChild<KaxVideoPixelHeight>(GetChild<KaxTrackVideo>(*m_track_entry)).SetValue(m_hvideo_pixel_height);
}

void
generic_packetizer_c::set_video_pixel_dimensions(int width,
                                                 int height) {
  set_video_pixel_width(width);
  set_video_pixel_height(height);
}

void
generic_packetizer_c::set_video_display_width(int width) {
  m_hvideo_display_width = width;
  if (m_track_entry)
    GetChild<KaxVideoDisplayWidth>(GetChild<KaxTrackVideo>(*m_track_entry)).SetValue(m_hvideo_display_width);
}

void
generic_packetizer_c::set_video_display_height(int height) {
  m_hvideo_display_height = height;
  if (m_track_entry)
    GetChild<KaxVideoDisplayHeight>(GetChild<KaxTrackVideo>(*m_track_entry)).SetValue(m_hvideo_display_height);
}

void
generic_packetizer_c::set_video_display_dimensions(int width,
                                                   int height,
                                                   option_source_e source) {
  if (display_dimensions_or_aspect_ratio_set() && (m_ti.m_display_dimensions_source >= source))
    return;

  m_ti.m_display_width             = width;
  m_ti.m_display_height            = height;
  m_ti.m_display_dimensions_source = source;
  m_ti.m_display_dimensions_given  = true;
  m_ti.m_aspect_ratio_given        = false;

  set_video_display_width(width);
  set_video_display_height(height);

}

void
generic_packetizer_c::set_video_aspect_ratio(double aspect_ratio,
                                             bool is_factor,
                                             option_source_e source) {
  if (display_dimensions_or_aspect_ratio_set() && (m_ti.m_display_dimensions_source >= source))
    return;

  m_ti.m_aspect_ratio              = aspect_ratio;
  m_ti.m_aspect_ratio_is_factor    = is_factor;
  m_ti.m_display_dimensions_source = source;
  m_ti.m_display_dimensions_given  = false;
  m_ti.m_aspect_ratio_given        = true;
}

void
generic_packetizer_c::set_as_default_track(int type,
                                           int priority) {
  if (g_default_tracks_priority[type] < priority) {
    g_default_tracks_priority[type] = priority;
    g_default_tracks[type]          = m_hserialno;

  } else if (   (DEFAULT_TRACK_PRIORITY_CMDLINE == priority)
             && (g_default_tracks[type] != m_hserialno)
             && !m_default_track_warning_printed) {
    mxwarn(boost::format(Y("Another default track for %1% tracks has already been set. The 'default' flag for track %2% of '%3%' will not be set.\n"))
           % (DEFTRACK_TYPE_AUDIO == type ? "audio" : DEFTRACK_TYPE_VIDEO == type ? "video" : "subtitle") % m_ti.m_id % m_ti.m_fname);
    m_default_track_warning_printed = true;
  }
}

void
generic_packetizer_c::set_language(const std::string &language) {
  m_ti.m_language = language;
  if (m_track_entry)
    GetChild<KaxTrackLanguage>(m_track_entry).SetValue(m_ti.m_language);
}

void
generic_packetizer_c::set_video_pixel_cropping(int left,
                                               int top,
                                               int right,
                                               int bottom,
                                               option_source_e source) {
  m_ti.m_pixel_cropping.set(pixel_crop_t{left, top, right, bottom}, source);

  if (m_track_entry) {
    KaxTrackVideo &video = GetChild<KaxTrackVideo>(m_track_entry);
    auto crop            = m_ti.m_pixel_cropping.get();

    GetChild<KaxVideoPixelCropLeft  >(video).SetValue(crop.left);
    GetChild<KaxVideoPixelCropTop   >(video).SetValue(crop.top);
    GetChild<KaxVideoPixelCropRight >(video).SetValue(crop.right);
    GetChild<KaxVideoPixelCropBottom>(video).SetValue(crop.bottom);
  }
}

void
generic_packetizer_c::set_video_pixel_cropping(const pixel_crop_t &cropping,
                                               option_source_e source) {
  set_video_pixel_cropping(cropping.left, cropping.top, cropping.right, cropping.bottom, source);
}

void
generic_packetizer_c::set_video_stereo_mode(stereo_mode_c::mode stereo_mode,
                                            option_source_e source) {
  m_ti.m_stereo_mode.set(stereo_mode, source);

  if (m_track_entry && (stereo_mode_c::unspecified != m_ti.m_stereo_mode.get()))
    set_video_stereo_mode_impl(GetChild<KaxTrackVideo>(*m_track_entry), m_ti.m_stereo_mode.get());
}

void
generic_packetizer_c::set_video_stereo_mode_impl(EbmlMaster &video,
                                                 stereo_mode_c::mode stereo_mode) {
  GetChild<KaxVideoStereoMode>(video).SetValue(stereo_mode);
  bool previous_stereo_mode = g_stereo_mode_used;

  if (   (stereo_mode_c::mono        != stereo_mode)
      && (stereo_mode_c::unspecified != stereo_mode))
    g_stereo_mode_used = true;

  if (!previous_stereo_mode)
    rerender_ebml_head();
}

void
generic_packetizer_c::set_headers() {
  if (0 < m_connected_to) {
    mxerror(boost::format("generic_packetizer_c::set_headers(): connected_to > 0 (type: %1%). %2%\n") % typeid(*this).name() % BUGMSG);
    return;
  }

  bool found = false;
  size_t idx;
  for (idx = 0; ptzrs_in_header_order.size() > idx; ++idx)
    if (this == ptzrs_in_header_order[idx]) {
      found = true;
      break;
    }

  if (!found)
    ptzrs_in_header_order.push_back(this);

  if (!m_track_entry) {
    m_track_entry    = !g_kax_last_entry ? &GetChild<KaxTrackEntry>(g_kax_tracks) : &GetNextChild<KaxTrackEntry>(*g_kax_tracks, *g_kax_last_entry);
    g_kax_last_entry = m_track_entry;
    m_track_entry->SetGlobalTimecodeScale((int64_t)g_timecode_scale);
  }

  GetChild<KaxTrackNumber>(m_track_entry).SetValue(m_hserialno);

  if (0 == m_huid)
    m_huid = create_unique_number(UNIQUE_TRACK_IDS);

  GetChild<KaxTrackUID>(m_track_entry).SetValue(m_huid);

  if (-1 != m_htrack_type)
    GetChild<KaxTrackType>(m_track_entry).SetValue(m_htrack_type);

  if (!m_hcodec_id.empty())
    GetChild<KaxCodecID>(m_track_entry).SetValue(m_hcodec_id);

  if (m_hcodec_private)
    GetChild<KaxCodecPrivate>(*m_track_entry).CopyBuffer(static_cast<binary *>(m_hcodec_private->get_buffer()), m_hcodec_private->get_size());

  if (!outputting_webm()) {
    if (-1 != m_htrack_min_cache)
      GetChild<KaxTrackMinCache>(m_track_entry).SetValue(m_htrack_min_cache);

    if (-1 != m_htrack_max_cache)
      GetChild<KaxTrackMaxCache>(m_track_entry).SetValue(m_htrack_max_cache);

    if (-1 != m_htrack_max_add_block_ids)
      GetChild<KaxMaxBlockAdditionID>(m_track_entry).SetValue(m_htrack_max_add_block_ids);
  }

  if (m_timecode_factory)
    m_htrack_default_duration = (int64_t)m_timecode_factory->get_default_duration(m_htrack_default_duration);
  if (-1.0 != m_htrack_default_duration)
    GetChild<KaxTrackDefaultDuration>(m_track_entry).SetValue(m_htrack_default_duration);

  idx = TRACK_TYPE_TO_DEFTRACK_TYPE(m_htrack_type);

  if (boost::logic::indeterminate(m_ti.m_default_track))
    set_as_default_track(idx, DEFAULT_TRACK_PRIORITY_FROM_TYPE);
  else if (m_ti.m_default_track)
    set_as_default_track(idx, DEFAULT_TRACK_PRIORITY_CMDLINE);
  else if (g_default_tracks[idx] == m_hserialno)
    g_default_tracks[idx] = 0;

  GetChild<KaxTrackLanguage>(m_track_entry).SetValue(m_ti.m_language != "" ? m_ti.m_language : g_default_language.c_str());

  if (!m_ti.m_track_name.empty())
    GetChild<KaxTrackName>(m_track_entry).SetValue(cstrutf8_to_UTFstring(m_ti.m_track_name));

  if (!boost::logic::indeterminate(m_ti.m_forced_track))
    GetChild<KaxTrackFlagForced>(m_track_entry).SetValue(m_ti.m_forced_track ? 1 : 0);

  if (!boost::logic::indeterminate(m_ti.m_enabled_track))
    GetChild<KaxTrackFlagEnabled>(m_track_entry).SetValue(m_ti.m_enabled_track ? 1 : 0);

  if (m_seek_pre_roll.valid())
    GetChild<KaxSeekPreRoll>(m_track_entry).SetValue(m_seek_pre_roll.to_ns());

  if (m_codec_delay.valid())
    GetChild<KaxCodecDelay>(m_track_entry).SetValue(m_codec_delay.to_ns());

  if (track_video == m_htrack_type) {
    KaxTrackVideo &video = GetChild<KaxTrackVideo>(m_track_entry);

    if (-1 != m_hvideo_interlaced_flag)
      GetChild<KaxVideoFlagInterlaced>(GetChild<KaxTrackVideo>(*m_track_entry)).SetValue(m_hvideo_interlaced_flag);

    if ((-1 != m_hvideo_pixel_height) && (-1 != m_hvideo_pixel_width)) {
      if ((-1 == m_hvideo_display_width) || (-1 == m_hvideo_display_height) || m_ti.m_aspect_ratio_given || m_ti.m_display_dimensions_given) {
        if (m_ti.m_display_dimensions_given) {
          m_hvideo_display_width  = m_ti.m_display_width;
          m_hvideo_display_height = m_ti.m_display_height;

        } else {
          if (!m_ti.m_aspect_ratio_given)
            m_ti.m_aspect_ratio = (float)m_hvideo_pixel_width                       / (float)m_hvideo_pixel_height;

          else if (m_ti.m_aspect_ratio_is_factor)
            m_ti.m_aspect_ratio = (float)m_hvideo_pixel_width * m_ti.m_aspect_ratio / (float)m_hvideo_pixel_height;

          if (m_ti.m_aspect_ratio > ((float)m_hvideo_pixel_width / (float)m_hvideo_pixel_height)) {
            m_hvideo_display_width  = irnd(m_hvideo_pixel_height * m_ti.m_aspect_ratio);
            m_hvideo_display_height = m_hvideo_pixel_height;

          } else {
            m_hvideo_display_width  = m_hvideo_pixel_width;
            m_hvideo_display_height = irnd(m_hvideo_pixel_width / m_ti.m_aspect_ratio);
          }
        }
      }

      GetChild<KaxVideoPixelWidth   >(video).SetValue(m_hvideo_pixel_width);
      GetChild<KaxVideoPixelHeight  >(video).SetValue(m_hvideo_pixel_height);

      GetChild<KaxVideoDisplayWidth >(video).SetValue(m_hvideo_display_width);
      GetChild<KaxVideoDisplayHeight>(video).SetValue(m_hvideo_display_height);

      GetChild<KaxVideoDisplayWidth >(video).SetDefaultSize(4);
      GetChild<KaxVideoDisplayHeight>(video).SetDefaultSize(4);

      if (m_ti.m_pixel_cropping) {
        auto crop = m_ti.m_pixel_cropping.get();
        GetChild<KaxVideoPixelCropLeft  >(video).SetValue(crop.left);
        GetChild<KaxVideoPixelCropTop   >(video).SetValue(crop.top);
        GetChild<KaxVideoPixelCropRight >(video).SetValue(crop.right);
        GetChild<KaxVideoPixelCropBottom>(video).SetValue(crop.bottom);
      }

      if (m_ti.m_stereo_mode && (stereo_mode_c::unspecified != m_ti.m_stereo_mode.get()))
        set_video_stereo_mode_impl(video, m_ti.m_stereo_mode.get());
    }

  } else if (track_audio == m_htrack_type) {
    KaxTrackAudio &audio = GetChild<KaxTrackAudio>(m_track_entry);

    if (-1   != m_haudio_sampling_freq)
      GetChild<KaxAudioSamplingFreq>(audio).SetValue(m_haudio_sampling_freq);

    if (-1.0 != m_haudio_output_sampling_freq)
      GetChild<KaxAudioOutputSamplingFreq>(audio).SetValue(m_haudio_output_sampling_freq);

    if (-1   != m_haudio_channels)
      GetChild<KaxAudioChannels>(audio).SetValue(m_haudio_channels);

    if (-1   != m_haudio_bit_depth)
      GetChild<KaxAudioBitDepth>(audio).SetValue(m_haudio_bit_depth);

  } else if (track_buttons == m_htrack_type) {
    if ((-1 != m_hvideo_pixel_height) && (-1 != m_hvideo_pixel_width)) {
      KaxTrackVideo &video = GetChild<KaxTrackVideo>(m_track_entry);

      GetChild<KaxVideoPixelWidth >(video).SetValue(m_hvideo_pixel_width);
      GetChild<KaxVideoPixelHeight>(video).SetValue(m_hvideo_pixel_height);
    }

  }

  if ((COMPRESSION_UNSPECIFIED != m_hcompression) && (COMPRESSION_NONE != m_hcompression)) {
    KaxContentEncoding &c_encoding = GetChild<KaxContentEncoding>(GetChild<KaxContentEncodings>(m_track_entry));

    GetChild<KaxContentEncodingOrder>(c_encoding).SetValue(0); // First modification.
    GetChild<KaxContentEncodingType >(c_encoding).SetValue(0); // It's a compression.
    GetChild<KaxContentEncodingScope>(c_encoding).SetValue(1); // Only the frame contents have been compresed.

    m_compressor = compressor_c::create(m_hcompression);
    m_compressor->set_track_headers(c_encoding);
  }

  if (g_no_lacing)
    m_track_entry->EnableLacing(false);

  set_tag_track_uid();
  if (m_ti.m_tags) {
    while (m_ti.m_tags->ListSize() != 0) {
      KaxTag *tag = (KaxTag *)(*m_ti.m_tags)[0];
      add_tags(tag);
      m_ti.m_tags->Remove(0);
    }
  }
}

void
generic_packetizer_c::fix_headers() {
  GetChild<KaxTrackFlagDefault>(m_track_entry).SetValue(g_default_tracks[TRACK_TYPE_TO_DEFTRACK_TYPE(m_htrack_type)] == m_hserialno ? 1 : 0);

  m_track_entry->SetGlobalTimecodeScale((int64_t)g_timecode_scale);
}

void
generic_packetizer_c::add_packet(packet_cptr pack) {
  if ((0 == m_num_packets) && m_ti.m_reset_timecodes)
    m_ti.m_tcsync.displacement = -pack->timecode;

  ++m_num_packets;

  if (!m_reader->m_ptzr_first_packet)
    m_reader->m_ptzr_first_packet = this;

  // strip elements to be removed
  if (   (-1                     != m_htrack_max_add_block_ids)
      && (pack->data_adds.size()  > static_cast<size_t>(m_htrack_max_add_block_ids)))
    pack->data_adds.resize(m_htrack_max_add_block_ids);

  if (m_compressor) {
    try {
      pack->data = m_compressor->compress(pack->data);
      size_t i;
      for (i = 0; pack->data_adds.size() > i; ++i)
        pack->data_adds[i] = m_compressor->compress(pack->data_adds[i]);

    } catch (mtx::compression_x &e) {
      mxerror_tid(m_ti.m_fname, m_ti.m_id, boost::format(Y("Compression failed: %1%\n")) % e.error());
    }
  }

  pack->data->grab();
  for (auto &data_add : pack->data_adds)
    data_add->grab();

  pack->source = this;

  m_enqueued_bytes += pack->data->get_size();

  if ((0 > pack->bref) && (0 <= pack->fref))
    std::swap(pack->bref, pack->fref);

  if (1 != m_connected_to)
    add_packet2(pack);
  else
    m_deferred_packets.push_back(pack);
}

#define ADJUST_TIMECODE(x) (int64_t)((x + m_correction_timecode_offset + m_append_timecode_offset) * m_ti.m_tcsync.numerator / m_ti.m_tcsync.denominator) + m_ti.m_tcsync.displacement

void
generic_packetizer_c::add_packet2(packet_cptr pack) {
  pack->timecode   = ADJUST_TIMECODE(pack->timecode);
  if (pack->has_bref())
    pack->bref     = ADJUST_TIMECODE(pack->bref);
  if (pack->has_fref())
    pack->fref     = ADJUST_TIMECODE(pack->fref);
  if (pack->has_duration()) {
    pack->duration = static_cast<int64_t>(pack->duration * m_ti.m_tcsync.numerator / m_ti.m_tcsync.denominator);
    if (pack->has_discard_padding())
      pack->duration -= std::min(pack->duration, pack->discard_padding.to_ns());
  }

  if ((2 > m_htrack_min_cache) && pack->has_fref()) {
    set_track_min_cache(2);
    rerender_track_headers();

  } else if ((1 > m_htrack_min_cache) && pack->has_bref()) {
    set_track_min_cache(1);
    rerender_track_headers();
  }

  if (0 > pack->timecode)
    return;

  // 'timecode < safety_last_timecode' may only occur for B frames. In this
  // case we have the coding order, e.g. IPB1B2 and the timecodes
  // I: 0, P: 120, B1: 40, B2: 80.
  if (!m_relaxed_timecode_checking && (pack->timecode < m_safety_last_timecode) && (0 > pack->fref) && hack_engaged(ENGAGE_ENABLE_TIMECODE_WARNING)) {
    if (track_audio == m_htrack_type) {
      int64_t needed_timecode_offset  = m_safety_last_timecode + m_safety_last_duration - pack->timecode;
      m_correction_timecode_offset   += needed_timecode_offset;
      pack->timecode                 += needed_timecode_offset;
      if (pack->has_bref())
        pack->bref += needed_timecode_offset;
      if (pack->has_fref())
        pack->fref += needed_timecode_offset;

      mxwarn_tid(m_ti.m_fname, m_ti.m_id,
                 boost::format(Y("The current packet's timecode is smaller than that of the previous packet. "
                                 "This usually means that the source file is a Matroska file that has not been created 100%% correctly. "
                                 "The timecodes of all packets will be adjusted by %1%ms in order not to lose any data. "
                                 "This may throw audio/video synchronization off, but that can be corrected with mkvmerge's \"--sync\" option. "
                                 "If you already use \"--sync\" and you still get this warning then do NOT worry -- this is normal. "
                                 "If this error happens more than once and you get this message more than once for a particular track "
                                 "then either is the source file badly mastered, or mkvmerge contains a bug. "
                                 "In this case you should contact the author Moritz Bunkus <moritz@bunkus.org>.\n"))
                 % ((needed_timecode_offset + 500000) / 1000000));

    } else
      mxwarn_tid(m_ti.m_fname, m_ti.m_id,
                 boost::format(Y("pr_generic.cpp/generic_packetizer_c::add_packet(): timecode < last_timecode (%1% < %2%). %3%\n"))
                 % format_timecode(pack->timecode) % format_timecode(m_safety_last_timecode) % BUGMSG);
  }

  m_safety_last_timecode        = pack->timecode;
  m_safety_last_duration        = pack->duration;
  pack->timecode_before_factory = pack->timecode;

  m_packet_queue.push_back(pack);
  if (!m_timecode_factory || (TFA_IMMEDIATE == m_timecode_factory_application_mode))
    apply_factory_once(pack);
  else
    apply_factory();
}

void
generic_packetizer_c::process_deferred_packets() {
  for (auto &packet : m_deferred_packets)
    add_packet2(packet);
  m_deferred_packets.clear();
}

packet_cptr
generic_packetizer_c::get_packet() {
  if (m_packet_queue.empty() || !m_packet_queue.front()->factory_applied)
    return packet_cptr{};

  packet_cptr pack = m_packet_queue.front();
  m_packet_queue.pop_front();

  pack->output_order_timecode = timecode_c::ns(pack->assigned_timecode - std::max(m_codec_delay.to_ns(0), m_seek_pre_roll.to_ns(0)));

  m_enqueued_bytes -= pack->data->get_size();

  --m_next_packet_wo_assigned_timecode;
  if (0 > m_next_packet_wo_assigned_timecode)
    m_next_packet_wo_assigned_timecode = 0;

  return pack;
}

void
generic_packetizer_c::apply_factory_once(packet_cptr &packet) {
  if (!m_timecode_factory) {
    packet->assigned_timecode = packet->timecode;
    packet->gap_following     = false;
  } else
    packet->gap_following     = m_timecode_factory->get_next(packet);

  packet->factory_applied     = true;

  mxverb(4,
         boost::format("apply_factory_once(): source %1% t %2% tbf %3% at %4%\n")
         % packet->source->get_source_track_num() % packet->timecode % packet->timecode_before_factory % packet->assigned_timecode);

  m_max_timecode_seen           = std::max(m_max_timecode_seen, packet->assigned_timecode + packet->duration);
  m_reader->m_max_timecode_seen = std::max(m_max_timecode_seen, m_reader->m_max_timecode_seen);

  ++m_next_packet_wo_assigned_timecode;
}

void
generic_packetizer_c::apply_factory() {
  if (m_packet_queue.empty())
    return;

  // Find the first packet to which the factory hasn't been applied yet.
  packet_cptr_di p_start = m_packet_queue.begin() + m_next_packet_wo_assigned_timecode;

  while ((m_packet_queue.end() != p_start) && (*p_start)->factory_applied)
    ++p_start;

  if (m_packet_queue.end() == p_start)
    return;

  if (TFA_SHORT_QUEUEING == m_timecode_factory_application_mode)
    apply_factory_short_queueing(p_start);

  else
    apply_factory_full_queueing(p_start);
}

void
generic_packetizer_c::apply_factory_short_queueing(packet_cptr_di &p_start) {
  while (m_packet_queue.end() != p_start) {
    // Find the next packet with a timecode bigger than the start packet's
    // timecode. All packets between those two including the start packet
    // and excluding the end packet can be timestamped.
    packet_cptr_di p_end = p_start + 1;
    while ((m_packet_queue.end() != p_end) && ((*p_end)->timecode_before_factory < (*p_start)->timecode_before_factory))
      ++p_end;

    // Abort if no such packet was found, but keep on assigning if the
    // packetizer has been flushed already.
    if (!m_has_been_flushed && (m_packet_queue.end() == p_end))
      return;

    // Now assign timecodes to the ones between p_start and p_end...
    packet_cptr_di p_current;
    for (p_current = p_start + 1; p_current != p_end; ++p_current)
      apply_factory_once(*p_current);
    // ...and to p_start itself.
    apply_factory_once(*p_start);

    p_start = p_end;
  }
}

struct packet_sorter_t {
  int m_index;
  static std::deque<packet_cptr> *m_packet_queue;

  packet_sorter_t(int index)
    : m_index(index)
  {
  }

  bool operator <(const packet_sorter_t &cmp) const {
    return (*m_packet_queue)[m_index]->timecode < (*m_packet_queue)[cmp.m_index]->timecode;
  }
};

std::deque<packet_cptr> *packet_sorter_t::m_packet_queue = nullptr;

void
generic_packetizer_c::apply_factory_full_queueing(packet_cptr_di &p_start) {
  packet_sorter_t::m_packet_queue = &m_packet_queue;

  while (m_packet_queue.end() != p_start) {
    // Find the next I frame packet.
    packet_cptr_di p_end = p_start + 1;
    while ((m_packet_queue.end() != p_end) && !(*p_end)->is_key_frame())
      ++p_end;

    // Abort if no such packet was found, but keep on assigning if the
    // packetizer has been flushed already.
    if (!m_has_been_flushed && (m_packet_queue.end() == p_end))
      return;

    // Now sort the frames by their timecode as the factory has to be
    // applied to the packets in the same order as they're timestamped.
    std::vector<packet_sorter_t> sorter;
    bool needs_sorting        = false;
    int64_t previous_timecode = 0;
    size_t i                  = distance(m_packet_queue.begin(), p_start);

    packet_cptr_di p_current;
    for (p_current = p_start; p_current != p_end; ++i, ++p_current) {
      sorter.push_back(packet_sorter_t(i));
      if (m_packet_queue[i]->timecode < previous_timecode)
        needs_sorting = true;
      previous_timecode = m_packet_queue[i]->timecode;
    }

    if (needs_sorting)
      std::sort(sorter.begin(), sorter.end());

    // Finally apply the factory.
    for (i = 0; sorter.size() > i; ++i)
      apply_factory_once(m_packet_queue[sorter[i].m_index]);

    p_start = p_end;
  }
}

void
generic_packetizer_c::force_duration_on_last_packet() {
  if (m_packet_queue.empty()) {
    mxverb_tid(3, m_ti.m_fname, m_ti.m_id, "force_duration_on_last_packet: packet queue is empty\n");
    return;
  }
  packet_cptr &packet        = m_packet_queue.back();
  packet->duration_mandatory = true;
  mxverb_tid(3, m_ti.m_fname, m_ti.m_id,
             boost::format("force_duration_on_last_packet: forcing at %1% with %|2$.3f|ms\n") % format_timecode(packet->timecode) % (packet->duration / 1000.0));
}

int64_t
generic_packetizer_c::handle_avi_audio_sync(int64_t num_bytes,
                                            bool vbr) {
  if ((0 == m_ti.m_avi_samples_per_sec) || (0 == m_ti.m_avi_block_align) || (0 == m_ti.m_avi_avg_bytes_per_sec) || !m_ti.m_avi_audio_sync_enabled) {
    enable_avi_audio_sync(false);
    return -1;
  }

  int64_t duration;
  if (!vbr)
     duration = num_bytes * 1000000000 / m_ti.m_avi_avg_bytes_per_sec;

  else {
    unsigned int num_blocks = 0;
    size_t i;
    for (i = 0; (m_ti.m_avi_block_sizes.size() > i) && (0 < num_bytes); ++i) {
      int64_t block_size  = m_ti.m_avi_block_sizes[i];
      num_blocks         += (block_size + m_ti.m_avi_block_align - 1) / m_ti.m_avi_block_align;
      num_bytes          -= std::min(num_bytes, block_size);
    }

    duration = static_cast<int64_t>(num_blocks * 1000000000ll * static_cast<double>(m_ti.m_avi_samples_per_chunk) / static_cast<double>(m_ti.m_avi_sample_scale));
  }

  enable_avi_audio_sync(false);

  return duration;
}

void
generic_packetizer_c::connect(generic_packetizer_c *src,
                              int64_t append_timecode_offset) {
  m_free_refs                  = src->m_free_refs;
  m_next_free_refs             = src->m_next_free_refs;
  m_track_entry                = src->m_track_entry;
  m_hserialno                  = src->m_hserialno;
  m_htrack_type                = src->m_htrack_type;
  m_htrack_default_duration    = src->m_htrack_default_duration;
  m_huid                       = src->m_huid;
  m_hcompression               = src->m_hcompression;
  m_compressor                 = compressor_c::create(m_hcompression);
  m_last_cue_timecode          = src->m_last_cue_timecode;
  m_timecode_factory           = src->m_timecode_factory;
  m_correction_timecode_offset = 0;

  if (-1 == append_timecode_offset)
    m_append_timecode_offset   = src->m_max_timecode_seen;
  else
    m_append_timecode_offset   = append_timecode_offset;

  m_connected_to++;
  if (2 == m_connected_to)
    process_deferred_packets();
}

void
generic_packetizer_c::set_displacement_maybe(int64_t displacement) {
  if ((1 == m_ti.m_tcsync.numerator) && (1 == m_ti.m_tcsync.denominator) && (0 == m_ti.m_tcsync.displacement))
    m_ti.m_tcsync.displacement = displacement;
}

bool
generic_packetizer_c::contains_gap() {
  return m_timecode_factory ? m_timecode_factory->contains_gap() : false;
}

void
generic_packetizer_c::flush() {
  flush_impl();

  m_has_been_flushed = true;
  apply_factory();
}

bool
generic_packetizer_c::display_dimensions_or_aspect_ratio_set() {
  return m_ti.display_dimensions_or_aspect_ratio_set();
}

bool
generic_packetizer_c::is_compatible_with(output_compatibility_e compatibility) {
  return OC_MATROSKA == compatibility;
}

void
generic_packetizer_c::discard_queued_packets() {
  m_packet_queue.clear();
}

bool
generic_packetizer_c::wants_cue_duration()
  const {
  return get_track_type() == track_subtitle;
}

void
generic_packetizer_c::show_experimental_status_version(std::string const &codec_id) {
  auto idx = get_format_name().get_untranslated();
  if (s_experimental_status_warning_shown[idx])
    return;

  s_experimental_status_warning_shown[idx] = true;
  mxwarn(boost::format(Y("Note that the Matroska specifications regarding the storage of '%1%' have not been finalized yet. "
                         "mkvmerge's support for it is therefore subject to change and uses the CodecID '%2%/EXPERIMENTAL' instead of '%2%'. "
                         "This warning will be removed once the specifications have been finalized and mkvmerge has been updated accordingly.\n"))
         % get_format_name().get_translated() % codec_id);
}

int64_t
generic_packetizer_c::create_track_number() {
  bool found   = false;
  int file_num = -1;
  size_t i;
  for (i = 0; i < g_files.size(); i++)
    if (g_files[i].reader == this->m_reader) {
      found = true;
      file_num = i;
      break;
    }

  if (!found)
    mxerror(boost::format(Y("create_track_number: file_num not found. %1%\n")) % BUGMSG);

  int64_t tnum = -1;
  found        = false;
  for (i = 0; i < g_track_order.size(); i++)
    if ((g_track_order[i].file_id == file_num) &&
        (g_track_order[i].track_id == m_ti.m_id)) {
      found = true;
      tnum = i + 1;
      break;
    }
  if (found) {
    found = false;
    for (i = 0; i < g_packetizers.size(); i++)
      if (g_packetizers[i].packetizer && (g_packetizers[i].packetizer->get_track_num() == tnum)) {
        tnum = ms_track_number;
        break;
      }
  } else
    tnum = ms_track_number;

  if (tnum >= ms_track_number)
    ms_track_number = tnum + 1;

  return tnum;
}

//--------------------------------------------------------------------

#define add_all_requested_track_ids(type, container)                                                       \
  for (std::map<int64_t, type>::const_iterator i = m_ti.container.begin(); m_ti.container.end() != i; ++i) \
    add_requested_track_id(i->first);

#define add_all_requested_track_ids2(container)                                                                            \
  for (std::map<int64_t, bool>::const_iterator i = m_ti.container.m_items.begin(); m_ti.container.m_items.end() != i; ++i) \
    add_requested_track_id(i->first);

generic_reader_c::generic_reader_c(const track_info_c &ti,
                                   const mm_io_cptr &in)
  : m_ti(ti)
  , m_in(in)
  , m_size(in->get_size())
  , m_ptzr_first_packet(nullptr)
  , m_max_timecode_seen(0)
  , m_appending(false)
  , m_num_video_tracks(0)
  , m_num_audio_tracks(0)
  , m_num_subtitle_tracks(0)
  , m_reference_timecode_tolerance(0)
{
  add_all_requested_track_ids2(m_atracks);
  add_all_requested_track_ids2(m_vtracks);
  add_all_requested_track_ids2(m_stracks);
  add_all_requested_track_ids2(m_btracks);
  add_all_requested_track_ids2(m_track_tags);
  add_all_requested_track_ids(std::string,          m_all_fourccs);
  add_all_requested_track_ids(display_properties_t, m_display_properties);
  add_all_requested_track_ids(timecode_sync_t,      m_timecode_syncs);
  add_all_requested_track_ids(cue_strategy_e,       m_cue_creations);
  add_all_requested_track_ids(bool,                 m_default_track_flags);
  add_all_requested_track_ids(bool,                 m_fix_bitstream_frame_rate_flags);
  add_all_requested_track_ids(std::string,          m_languages);
  add_all_requested_track_ids(std::string,          m_sub_charsets);
  add_all_requested_track_ids(std::string,          m_all_tags);
  add_all_requested_track_ids(bool,                 m_all_aac_is_sbr);
  add_all_requested_track_ids(compression_method_e, m_compression_list);
  add_all_requested_track_ids(std::string,          m_track_names);
  add_all_requested_track_ids(std::string,          m_all_ext_timecodes);
  add_all_requested_track_ids(pixel_crop_t,         m_pixel_crop_list);
}

generic_reader_c::~generic_reader_c() {
  size_t i;

  for (i = 0; i < m_reader_packetizers.size(); i++)
    delete m_reader_packetizers[i];
}

void
generic_reader_c::set_timecode_restrictions(timecode_c const &min,
                                            timecode_c const &max) {
  m_restricted_timecodes_min = min;
  m_restricted_timecodes_max = max;
}

timecode_c const &
generic_reader_c::get_timecode_restriction_min()
  const {
  return m_restricted_timecodes_min;
}

timecode_c const &
generic_reader_c::get_timecode_restriction_max()
  const {
  return m_restricted_timecodes_max;
}

void
generic_reader_c::read_all() {
  for (auto &packetizer : m_reader_packetizers)
    while (read(packetizer, true) != FILE_STATUS_DONE)
      ;
}

bool
generic_reader_c::demuxing_requested(char type,
                                     int64_t id) {
  item_selector_c<bool> *tracks = 'v' == type ? &m_ti.m_vtracks
                                : 'a' == type ? &m_ti.m_atracks
                                : 's' == type ? &m_ti.m_stracks
                                : 'b' == type ? &m_ti.m_btracks
                                : 'T' == type ? &m_ti.m_track_tags
                                :               nullptr;

  if (!tracks)
    mxerror(boost::format(Y("pr_generic.cpp/generic_reader_c::demuxing_requested(): Invalid track type %1%.")) % type);

  return tracks->selected(id);
}

attach_mode_e
generic_reader_c::attachment_requested(int64_t id) {
  if (m_ti.m_attach_mode_list.none())
    return ATTACH_MODE_SKIP;

  if (m_ti.m_attach_mode_list.empty())
    return ATTACH_MODE_TO_ALL_FILES;

  if (m_ti.m_attach_mode_list.selected(id))
    return m_ti.m_attach_mode_list.get(id);

  if (m_ti.m_attach_mode_list.selected(-1))
    return m_ti.m_attach_mode_list.get(-1);

  return ATTACH_MODE_SKIP;
}

int
generic_reader_c::add_packetizer(generic_packetizer_c *ptzr) {
  if (outputting_webm() && !ptzr->is_compatible_with(OC_WEBM))
    mxerror(boost::format(Y("The codec type '%1%' cannot be used in a WebM compliant file.\n")) % ptzr->get_format_name());

  m_reader_packetizers.push_back(ptzr);
  m_used_track_ids.push_back(ptzr->m_ti.m_id);
  if (!m_appending)
    add_packetizer_globally(ptzr);

  return m_reader_packetizers.size() - 1;
}

size_t
generic_reader_c::get_num_packetizers()
  const
{
  return m_reader_packetizers.size();
}

void
generic_reader_c::set_timecode_offset(int64_t offset) {
  m_max_timecode_seen = offset;

  for (auto ptzr : m_reader_packetizers)
    ptzr->m_correction_timecode_offset = offset;
}

void
generic_reader_c::set_headers() {
  for (auto ptzr : m_reader_packetizers)
    ptzr->set_headers();
}

void
generic_reader_c::set_headers_for_track(int64_t tid) {
  for (auto ptzr : m_reader_packetizers)
    if (ptzr->m_ti.m_id == tid) {
      ptzr->set_headers();
      break;
    }
}

void
generic_reader_c::check_track_ids_and_packetizers() {
  add_available_track_ids();

  size_t r;
  for (r = 0; m_requested_track_ids.size() > r; ++r) {
    bool found = false;
    size_t a;
    for (a = 0; m_available_track_ids.size() > a; ++a)
      if (m_requested_track_ids[r] == m_available_track_ids[a]) {
        found = true;
        break;
      }

    if (!found)
      mxwarn_fn(m_ti.m_fname,
                boost::format(Y("A track with the ID %1% was requested but not found in the file. The corresponding option will be ignored.\n"))
                % m_requested_track_ids[r]);
  }
}

void
generic_reader_c::add_requested_track_id(int64_t id) {
  if (-1 == id)
    return;

  bool found = false;
  size_t i;
  for (i = 0; i < m_requested_track_ids.size(); i++)
    if (m_requested_track_ids[i] == id) {
      found = true;
      break;
    }

  if (!found)
    m_requested_track_ids.push_back(id);
}

int64_t
generic_reader_c::get_queued_bytes()
  const {
  int64_t bytes = 0;

  for (auto ptzr : m_reader_packetizers)
    bytes += ptzr->get_queued_bytes();

  return bytes;
}

file_status_e
generic_reader_c::flush_packetizer(int num) {
  return flush_packetizer(PTZR(num));
}

file_status_e
generic_reader_c::flush_packetizer(generic_packetizer_c *ptzr) {
  ptzr->flush();

  return FILE_STATUS_DONE;
}

file_status_e
generic_reader_c::flush_packetizers() {
  for (auto ptzr : m_reader_packetizers)
    ptzr->flush();

  return FILE_STATUS_DONE;
}

void
generic_reader_c::id_result_container(const std::string &verbose_info) {
  std::vector<std::string> verbose_info_list;
  if (!verbose_info.empty())
    verbose_info_list.push_back(verbose_info);
  id_result_container(verbose_info_list);
}

void
generic_reader_c::id_result_container(const std::vector<std::string> &verbose_info) {
  m_id_results_container.info         = get_format_name().get_translated();
  m_id_results_container.verbose_info = verbose_info;
  m_id_results_container.verbose_info.push_back((boost::format("is_providing_timecodes:%1%") % (is_providing_timecodes() ? 1 : 0)).str());
}

void
generic_reader_c::id_result_track(int64_t track_id,
                                  const std::string &type,
                                  const std::string &info,
                                  const std::string &verbose_info) {
  id_result_t result(track_id, type, info, empty_string, 0);
  if (!verbose_info.empty())
    result.verbose_info.push_back(verbose_info);
  m_id_results_tracks.push_back(result);
}

void
generic_reader_c::id_result_track(int64_t track_id,
                                  const std::string &type,
                                  const std::string &info,
                                  const std::vector<std::string> &verbose_info) {
  id_result_t result(track_id, type, info, empty_string, 0);
  result.verbose_info = verbose_info;
  m_id_results_tracks.push_back(result);
}

void
generic_reader_c::id_result_attachment(int64_t attachment_id,
                                       const std::string &type,
                                       int size,
                                       const std::string &file_name,
                                       const std::string &description) {
  id_result_t result(attachment_id, type, file_name, description, size);
  m_id_results_attachments.push_back(result);
}

void
generic_reader_c::id_result_chapters(int num_entries) {
  id_result_t result(0, ID_RESULT_CHAPTERS, empty_string, empty_string, num_entries);
  m_id_results_chapters.push_back(result);
}

void
generic_reader_c::id_result_tags(int64_t track_id,
                                 int num_entries) {
  id_result_t result(track_id, ID_RESULT_TAGS, empty_string, empty_string, num_entries);
  m_id_results_tags.push_back(result);
}

void
generic_reader_c::display_identification_results() {
  std::string format_file, format_track, format_attachment, format_att_description, format_att_file_name, format_chapters, format_tags_global, format_tags_track;

  if (g_identify_for_mmg) {
    format_file            =   "File '%1%': container: %2%";
    format_track           =   "Track ID %1%: %2% (%3%)";
    format_attachment      =   "Attachment ID %1%: type \"%2%\", size %3% bytes";
    format_att_description =   ", description \"%1%\"";
    format_att_file_name   =   ", file name \"%1%\"";
    format_chapters        =   "Chapters: %1% entries";
    format_tags_global     =   "Global tags: %1% entries";
    format_tags_track      =   "Tags for track ID %1%: %2% entries";

  } else {
    format_file            = Y("File '%1%': container: %2%");
    format_track           = Y("Track ID %1%: %2% (%3%)");
    format_attachment      = Y("Attachment ID %1%: type '%2%', size %3% bytes");
    format_att_description = Y(", description '%1%'");
    format_att_file_name   = Y(", file name '%1%'");
    format_chapters        = Y("Chapters: %1% entries");
    format_tags_global     = Y("Global tags: %1% entries");
    format_tags_track      = Y("Tags for track ID %1%: %2% entries");
  }

  mxinfo(boost::format(format_file) % m_ti.m_fname % m_id_results_container.info);

  if (g_identify_verbose && !m_id_results_container.verbose_info.empty())
    mxinfo(boost::format(" [%1%]") % join(" ", m_id_results_container.verbose_info));

  mxinfo("\n");

  for (auto &result : m_id_results_tracks) {
    mxinfo(boost::format(format_track) % result.id % result.type % result.info);

    if (g_identify_verbose && !result.verbose_info.empty())
      mxinfo(boost::format(" [%1%]") % join(" ", result.verbose_info));

    mxinfo("\n");
  }

  for (auto &result : m_id_results_attachments) {
    mxinfo(boost::format(format_attachment) % result.id % id_escape_string(result.type) % result.size);

    if (!result.description.empty())
      mxinfo(boost::format(format_att_description) % id_escape_string(result.description));

    if (!result.info.empty())
      mxinfo(boost::format(format_att_file_name) % id_escape_string(result.info));

    mxinfo("\n");
  }

  for (auto &result : m_id_results_chapters) {
    mxinfo(boost::format(format_chapters) % result.size);
    mxinfo("\n");
  }

  for (auto &result : m_id_results_tags) {
    if (ID_RESULT_GLOBAL_TAGS_ID == result.id)
      mxinfo(boost::format(format_tags_global) % result.size);
    else
      mxinfo(boost::format(format_tags_track) % result.id % result.size);

    mxinfo("\n");
  }
}

std::string
generic_reader_c::id_escape_string(const std::string &s) {
  return g_identify_for_mmg ? escape(s) : s;
}

void
generic_reader_c::add_available_track_id(int64_t id) {
  m_available_track_ids.push_back(id);
}

void
generic_reader_c::add_available_track_ids() {
  add_available_track_id(0);
}

void
generic_reader_c::add_available_track_id_range(int64_t start,
                                               int64_t end) {
  for (int64_t id = start; id <= end; ++id)
    add_available_track_id(id);
}

int
generic_reader_c::get_progress() {
  return 100 * m_in->getFilePointer() / m_size;
}

mm_io_c *
generic_reader_c::get_underlying_input()
  const {
  mm_io_c *actual_in = m_in.get();
  while (dynamic_cast<mm_proxy_io_c *>(actual_in))
    actual_in = static_cast<mm_proxy_io_c *>(actual_in)->get_proxied();
  return actual_in;
}

//
//--------------------------------------------------------------------

track_info_c::track_info_c()
  : m_id(0)
  , m_disable_multi_file(false)
  , m_aspect_ratio(0.0)
  , m_display_width(0)
  , m_display_height(0)
  , m_aspect_ratio_given(false)
  , m_aspect_ratio_is_factor(false)
  , m_display_dimensions_given(false)
  , m_display_dimensions_source(OPTION_SOURCE_NONE)
  , m_reset_timecodes(false)
  , m_cues(CUE_STRATEGY_UNSPECIFIED)
  , m_default_track(boost::logic::indeterminate)
  , m_fix_bitstream_frame_rate(boost::logic::indeterminate)
  , m_forced_track(boost::logic::indeterminate)
  , m_enabled_track(boost::logic::indeterminate)
  , m_compression(COMPRESSION_UNSPECIFIED)
  , m_nalu_size_length(0)
  , m_no_chapters(false)
  , m_no_global_tags(false)
  , m_avi_block_align(0)
  , m_avi_samples_per_sec(0)
  , m_avi_avg_bytes_per_sec(0)
  , m_avi_samples_per_chunk(0)
  , m_avi_sample_scale(0)
  , m_avi_audio_sync_enabled(false)
{
}

track_info_c &
track_info_c::operator =(const track_info_c &src) {
  m_id                         = src.m_id;
  m_fname                      = src.m_fname;

  m_atracks                    = src.m_atracks;
  m_btracks                    = src.m_btracks;
  m_stracks                    = src.m_stracks;
  m_vtracks                    = src.m_vtracks;
  m_track_tags                 = src.m_track_tags;
  m_disable_multi_file         = src.m_disable_multi_file;

  m_private_data               = src.m_private_data ? src.m_private_data->clone() : src.m_private_data;

  m_all_fourccs                = src.m_all_fourccs;

  m_display_properties         = src.m_display_properties;
  m_aspect_ratio               = src.m_aspect_ratio;
  m_aspect_ratio_given         = false;
  m_aspect_ratio_is_factor     = false;
  m_display_dimensions_given   = false;
  m_display_dimensions_source  = src.m_display_dimensions_source;

  m_timecode_syncs             = src.m_timecode_syncs;
  m_tcsync                     = src.m_tcsync;

  m_reset_timecodes_specs      = src.m_reset_timecodes_specs;
  m_reset_timecodes            = src.m_reset_timecodes;

  m_cue_creations              = src.m_cue_creations;
  m_cues                       = src.m_cues;

  m_default_track_flags        = src.m_default_track_flags;
  m_default_track              = src.m_default_track;

  m_fix_bitstream_frame_rate_flags = src.m_fix_bitstream_frame_rate_flags;
  m_fix_bitstream_frame_rate       = src.m_fix_bitstream_frame_rate;

  m_forced_track_flags         = src.m_forced_track_flags;
  m_forced_track               = src.m_forced_track;

  m_enabled_track_flags        = src.m_enabled_track_flags;
  m_enabled_track              = src.m_enabled_track;

  m_languages                  = src.m_languages;
  m_language                   = src.m_language;

  m_sub_charsets               = src.m_sub_charsets;
  m_sub_charset                = src.m_sub_charset;

  m_all_tags                   = src.m_all_tags;
  m_tags_file_name             = src.m_tags_file_name;
  m_tags                       = src.m_tags ? clone(src.m_tags) : kax_tags_cptr{};

  m_all_aac_is_sbr             = src.m_all_aac_is_sbr;

  m_compression_list           = src.m_compression_list;
  m_compression                = src.m_compression;

  m_track_names                = src.m_track_names;
  m_track_name                 = src.m_track_name;

  m_all_ext_timecodes          = src.m_all_ext_timecodes;
  m_ext_timecodes              = src.m_ext_timecodes;

  m_pixel_crop_list            = src.m_pixel_crop_list;
  m_pixel_cropping             = src.m_pixel_cropping;

  m_stereo_mode_list           = src.m_stereo_mode_list;
  m_stereo_mode                = src.m_stereo_mode;

  m_nalu_size_lengths          = src.m_nalu_size_lengths;
  m_nalu_size_length           = src.m_nalu_size_length;

  m_attach_mode_list           = src.m_attach_mode_list;

  m_no_chapters                = src.m_no_chapters;
  m_no_global_tags             = src.m_no_global_tags;

  m_chapter_charset            = src.m_chapter_charset;
  m_chapter_language           = src.m_chapter_language;

  m_avi_block_align            = src.m_avi_block_align;
  m_avi_samples_per_sec        = src.m_avi_samples_per_sec;
  m_avi_avg_bytes_per_sec      = src.m_avi_avg_bytes_per_sec;
  m_avi_samples_per_chunk      = src.m_avi_samples_per_chunk;
  m_avi_sample_scale           = src.m_avi_sample_scale;
  m_avi_block_sizes.clear();
  m_avi_audio_sync_enabled     = false;

  m_default_durations          = src.m_default_durations;
  m_max_blockadd_ids           = src.m_max_blockadd_ids;

  return *this;
}

bool
track_info_c::display_dimensions_or_aspect_ratio_set() {
  return OPTION_SOURCE_NONE != m_display_dimensions_source;
}

//--------------------------------------------------------------------

void
id_result_container_unsupported(const std::string &filename,
                                const std::string &info) {
  if (g_identifying) {
    if (g_identify_for_mmg)
      mxinfo(boost::format("File '%1%': unsupported container: %2%\n") % filename % info);
    else
      mxinfo(boost::format(Y("File '%1%': unsupported container: %2%\n")) % filename % info);
    mxexit(3);

  } else
    mxerror(boost::format(Y("The file '%1%' is a non-supported file type (%2%).\n")) % filename % info);
}
