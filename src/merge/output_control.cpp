/** \brief output handling

   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
   \author Modified by Steve Lhomme <steve.lhomme@free.fr>.
*/

#include "common/common_pch.h"

#include <errno.h>
#include <ctype.h>
#if defined(SYS_UNIX) || defined(COMP_CYGWIN) || defined(SYS_APPLE)
#include <signal.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#ifdef HAVE_UNISTD_H
#include <unistd.h>
#endif
#if defined(SYS_WINDOWS)
#include <windows.h>
#endif

#include <algorithm>
#include <iostream>
#include <typeinfo>

#include <boost/range/algorithm.hpp>
#include <boost/range/numeric.hpp>

#include <ebml/EbmlHead.h>
#include <ebml/EbmlSubHead.h>
#include <ebml/EbmlVersion.h>
#include <ebml/EbmlVoid.h>

#include <matroska/FileKax.h>
#include <matroska/KaxAttached.h>
#include <matroska/KaxAttachments.h>
#include <matroska/KaxBlock.h>
#include <matroska/KaxChapters.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxClusterData.h>
#include <matroska/KaxCues.h>
#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxSeekHead.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxTags.h>
#include <matroska/KaxTag.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>
#include <matroska/KaxVersion.h>

#include "common/chapters/chapters.h"
#include "common/ebml.h"
#include "common/fs_sys_helpers.h"
#include "common/hacks.h"
#include "common/math.h"
#include "common/mm_io.h"
#include "common/mm_read_cache_io.h"
#include "common/mm_write_cache_io.h"
#include "common/strings/formatting.h"
#include "common/tags/tags.h"
#include "common/translation.h"
#include "common/unique_numbers.h"
#include "common/version.h"
#include "common/xml/element_mapping.h"
#include "input/r_aac.h"
#include "input/r_aac_adif.h"
#include "input/r_ac3.h"
#include "input/r_asf.h"
#include "input/r_avc.h"
#include "input/r_avi.h"
#include "input/r_cdxa.h"
#include "input/r_corepicture.h"
#include "input/r_dirac.h"
#include "input/r_dts.h"
#include "input/r_dv.h"
#include "input/r_flac.h"
#include "input/r_flv.h"
#include "input/r_hdsub.h"
#include "input/r_ivf.h"
#include "input/r_matroska.h"
#include "input/r_mp3.h"
#include "input/r_mpeg_es.h"
#include "input/r_mpeg_ps.h"
#include "input/r_mpeg_ts.h"
#include "input/r_ogm.h"
#include "input/r_pgssup.h"
#include "input/r_qtmp4.h"
#include "input/r_real.h"
#include "input/r_srt.h"
#include "input/r_ssa.h"
#include "input/r_truehd.h"
#include "input/r_tta.h"
#include "input/r_usf.h"
#include "input/r_vc1.h"
#include "input/r_vobbtn.h"
#include "input/r_vobsub.h"
#include "input/r_wav.h"
#include "input/r_wavpack.h"
#include "merge/cluster_helper.h"
#include "merge/mkvmerge.h"
#include "merge/output_control.h"
#include "merge/debugging.h"
#include "merge/webm.h"

using namespace libmatroska;
namespace bfs = boost::filesystem;

namespace libmatroska {

  class KaxMyDuration: public KaxDuration {
  public:
    KaxMyDuration(const EbmlFloat::Precision prec): KaxDuration() {
      SetPrecision(prec);
    }
  };
}

std::vector<packetizer_t> g_packetizers;
std::vector<filelist_t> g_files;
std::vector<attachment_t> g_attachments;
std::vector<track_order_t> g_track_order;
std::vector<append_spec_t> g_append_mapping;
family_uids_c g_segfamily_uids;

int64_t g_attachment_sizes_first            = 0;
int64_t g_attachment_sizes_others           = 0;

KaxInfo *g_kax_info_chap                    = NULL;

// Variables set by the command line parser.
std::string g_outfile;
int64_t g_file_sizes                        = 0;
int g_max_blocks_per_cluster                = 65535;
int64_t g_max_ns_per_cluster                = 5000000000ll;
bool g_write_cues                           = true;
bool g_cue_writing_requested                = false;
generic_packetizer_c *g_video_packetizer    = NULL;
bool g_write_meta_seek_for_clusters         = false;
bool g_no_lacing                            = false;
bool g_no_linking                           = true;
bool g_use_durations                        = false;

double g_timecode_scale                     = TIMECODE_SCALE;
timecode_scale_mode_e g_timecode_scale_mode = TIMECODE_SCALE_MODE_NORMAL;

float g_video_fps                           = -1.0;
int g_default_tracks[3]                     = { 0, 0, 0, };
int g_default_tracks_priority[3]            = { 0, 0, 0, };

bool g_identifying                          = false;
bool g_identify_verbose                     = false;
bool g_identify_for_mmg                     = false;

KaxSegment *g_kax_segment                   = NULL;
KaxTracks *g_kax_tracks                     = NULL;
KaxTrackEntry *g_kax_last_entry             = NULL;
KaxCues *g_kax_cues                         = NULL;
KaxSeekHead *g_kax_sh_main                  = NULL;
KaxSeekHead *g_kax_sh_cues                  = NULL;
KaxChapters *g_kax_chapters                 = NULL;

KaxTags *g_tags_from_cue_chapters           = NULL;

std::string g_chapter_file_name;
std::string g_chapter_language;
std::string g_chapter_charset;

std::string g_segmentinfo_file_name;

std::string g_segment_title;
bool g_segment_title_set                    = false;

int64_t g_tags_size                         = 0;

int g_file_num = 1;

int g_split_max_num_files                   = 65535;

append_mode_e g_append_mode                 = APPEND_MODE_FILE_BASED;

bool g_stereo_mode_used                     = false;

std::string g_default_language              = "und";

bitvalue_cptr g_seguid_link_previous;
bitvalue_cptr g_seguid_link_next;
std::deque<bitvalue_cptr> g_forced_seguids;

static KaxInfo *s_kax_infos                 = NULL;
static KaxMyDuration *s_kax_duration        = NULL;

static KaxTags *s_kax_tags                  = NULL;
static KaxChapters *s_chapters_in_this_file = NULL;

static KaxAttachments *s_kax_as             = NULL;

static EbmlVoid *s_kax_sh_void              = NULL;
static EbmlVoid *s_kax_chapters_void        = NULL;
static int64_t s_max_chapter_size           = 0;
static EbmlVoid *s_void_after_track_headers = NULL;

static mm_io_cptr s_out;

static bitvalue_c s_seguid_prev(128), s_seguid_current(128), s_seguid_next(128);

static int s_display_files_done           = 0;
static int s_display_path_length          = 1;
static generic_reader_c *s_display_reader = NULL;

static EbmlHead *s_head                   = NULL;

/** \brief Add a segment family UID to the list if it doesn't exist already.

  \param family This segment family element is converted to a 128 bit
    element which is added to the list of segment family UIDs.
*/
bool
family_uids_c::add_family_uid(const KaxSegmentFamily &family) {
  bitvalue_c new_uid(family);

  // look for the same UID
  family_uids_c::const_iterator it;
  for (it = begin(); it != end(); it++)
    if (new_uid == *it)
      return false;

  push_back(new_uid);

  return true;
}

std::ostream &
operator<<(std::ostream &str,
           const append_spec_t &spec) {
  str << spec.src_file_id << ":" << spec.src_track_id << ":" << spec.dst_file_id << ":" << spec.dst_track_id;
  return str;
}

/** \brief Fix the file after mkvmerge has been interrupted

   On Unix like systems mkvmerge will install a signal handler. On \c SIGUSR1
   all debug information will be dumped to \c stdout if mkvmerge has been
   compiled with \c -DDEBUG.

   On \c SIGINT mkvmerge will try to sanitize the current output file
   by writing the cues, the meta seek information and by updating the
   segment duration and the segment length.
*/
#if defined(SYS_UNIX) || defined(COMP_CYGWIN) || defined(SYS_APPLE)
void
sighandler(int signum) {
  if (!s_out.is_set())
    mxerror(Y("mkvmerge was interrupted by a SIGINT (Ctrl+C?)\n"));

  mxwarn(Y("\nmkvmerge received a SIGINT (probably because the user pressed "
           "Ctrl+C). Trying to sanitize the file. If mkvmerge hangs during "
           "this process you'll have to kill it manually.\n"));

  mxinfo(Y("The file is being fixed, part 1/4..."));
  // Render the cues.
  if (g_write_cues && g_cue_writing_requested)
    g_kax_cues->Render(*s_out);
  mxinfo(Y(" done\n"));

  mxinfo(Y("The file is being fixed, part 2/4..."));
  // Now re-render the kax_duration and fill in the biggest timecode
  // as the file's duration.
  s_out->save_pos(s_kax_duration->GetElementPosition());
  *(static_cast<EbmlFloat *>(s_kax_duration)) = irnd((double)g_cluster_helper->get_duration() / (double)((int64_t)g_timecode_scale));
  s_kax_duration->Render(*s_out);
  s_out->restore_pos();
  mxinfo(Y(" done\n"));

  mxinfo(Y("The file is being fixed, part 3/4..."));
  // Write meta seek information if it is not disabled.
  if (g_cue_writing_requested)
    g_kax_sh_main->IndexThis(*g_kax_cues, *g_kax_segment);

  if ((g_kax_sh_main->ListSize() > 0) && !hack_engaged(ENGAGE_NO_META_SEEK)) {
    g_kax_sh_main->UpdateSize();
    if (s_kax_sh_void->ReplaceWith(*g_kax_sh_main, *s_out, true) == INVALID_FILEPOS_T)
      mxwarn(boost::format(Y("This should REALLY not have happened. The space reserved for the first meta seek element was too small. %1%\n")) % BUGMSG);
  }
  mxinfo(Y(" done\n"));

  mxinfo(Y("The file is being fixed, part 4/4..."));
  // Set the correct size for the segment.
  if (g_kax_segment->ForceSize(s_out->getFilePointer() - g_kax_segment->GetElementPosition() - g_kax_segment->HeadSize()))
    g_kax_segment->OverwriteHead(*s_out);

  mxinfo(Y(" done\n"));

  mxerror(Y("mkvmerge was interrupted by a SIGINT (Ctrl+C?)\n"));
}
#endif

static std::vector<bfs::path>
file_names_to_paths(const std::vector<std::string> &file_names) {
  std::vector<bfs::path> paths;
  for (auto &file_name : file_names)
    paths.push_back(bfs::system_complete(bfs::path(file_name)));

  return paths;
}

static mm_io_cptr
open_input_file(filelist_t &file) {
  try {
    if (file.all_names.size() == 1)
      return mm_io_cptr(mm_probe_cache_io_c::open(file.name, 20 * 1024 * 1024));
    else {
      std::vector<bfs::path> paths = file_names_to_paths(file.all_names);
      return mm_io_cptr(new mm_probe_cache_io_c(new mm_multi_file_io_c(paths, file.name), 20 * 1024 * 1024));
    }
  } catch (...) {
    mxerror(boost::format(Y("The source file '%1%' could not be opened successfully, or retrieving its size by seeking to the end did not work.\n")) % file.name);
    return mm_io_cptr(NULL);
  }
}

/** \brief Probe the file type

   Opens the input file and calls the \c probe_file function for each known
   file reader class. Uses \c mm_text_io_c for subtitle probing.
*/
void
get_file_type(filelist_t &file) {
  mm_io_cptr af_io = open_input_file(file);
  mm_io_c *io      = af_io.get_object();
  int64_t size     = io->get_size();

  file_type_e type = FILE_TYPE_IS_UNKNOWN;
  // File types that can be detected unambiguously but are not supported
  if (aac_adif_reader_c::probe_file(io, size))
    type = FILE_TYPE_AAC;
  else if (asf_reader_c::probe_file(io, size))
    type = FILE_TYPE_ASF;
  else if (cdxa_reader_c::probe_file(io, size))
    type = FILE_TYPE_CDXA;
  else if (dv_reader_c::probe_file(io, size))
    type = FILE_TYPE_DV;
  else if (flv_reader_c::probe_file(io, size))
    type = FILE_TYPE_FLV;
  else if (hdsub_reader_c::probe_file(io, size))
    type = FILE_TYPE_HDSUB;
  // File types that can be detected unambiguously
  else if (avi_reader_c::probe_file(io, size))
    type = FILE_TYPE_AVI;
  else if (kax_reader_c::probe_file(io, size))
    type = FILE_TYPE_MATROSKA;
  else if (wav_reader_c::probe_file(io, size))
    type = FILE_TYPE_WAV;
  else if (ogm_reader_c::probe_file(io, size))
    type = FILE_TYPE_OGM;
  else if (flac_reader_c::probe_file(io, size))
    type = FILE_TYPE_FLAC;
  else if (pgssup_reader_c::probe_file(io, size))
    type = FILE_TYPE_PGSSUP;
  else if (real_reader_c::probe_file(io, size))
    type = FILE_TYPE_REAL;
  else if (qtmp4_reader_c::probe_file(io, size))
    type = FILE_TYPE_QTMP4;
  else if (tta_reader_c::probe_file(io, size))
    type = FILE_TYPE_TTA;
  else if (vc1_es_reader_c::probe_file(io, size))
    type = FILE_TYPE_VC1;
  else if (wavpack_reader_c::probe_file(io, size))
    type = FILE_TYPE_WAVPACK4;
  else if (ivf_reader_c::probe_file(io, size))
    type = FILE_TYPE_IVF;
  else if (dirac_es_reader_c::probe_file(io, size))
    type = FILE_TYPE_DIRAC;
  // File types that are misdetected sometimes
  else if (mpeg_ts_reader_c::probe_file(io, size))
    type = FILE_TYPE_MPEG_TS;
  else if (mpeg_ps_reader_c::probe_file(io, size))
    type = FILE_TYPE_MPEG_PS;
  else if (mpeg_es_reader_c::probe_file(io, size))
    type = FILE_TYPE_MPEG_ES;
  else {
    // File types which are the same in raw format and in other container formats.
    // Detection requires 20 or more consecutive packets.
    static const int s_probe_sizes[]                          = { 128 * 1024, 256 * 1024, 512 * 1024, 1024 * 1024, 0 };
    static const int s_probe_num_required_consecutive_packets = 64;

    int i;
    for (i = 0; (0 != s_probe_sizes[i]) && (FILE_TYPE_IS_UNKNOWN == type); ++i)
      if (mp3_reader_c::probe_file(io, size, s_probe_sizes[i], s_probe_num_required_consecutive_packets))
        type = FILE_TYPE_MP3;
      else if (ac3_reader_c::probe_file(io, size, s_probe_sizes[i], s_probe_num_required_consecutive_packets))
        type = FILE_TYPE_AC3;
      else if (aac_reader_c::probe_file(io, size, s_probe_sizes[i], s_probe_num_required_consecutive_packets))
        type = FILE_TYPE_AAC;
  }
  // More file types with detection issues.
  if (type != FILE_TYPE_IS_UNKNOWN)
    ;
  else if (truehd_reader_c::probe_file(io, size))
    type = FILE_TYPE_TRUEHD;
  else if (dts_reader_c::probe_file(io, size))
    type = FILE_TYPE_DTS;
  else if (vobbtn_reader_c::probe_file(io, size))
    type = FILE_TYPE_VOBBTN;
  else if (avc_es_reader_c::probe_file(io, size))
    type = FILE_TYPE_AVC_ES;
  else {
    // File types which are the same in raw format and in other container formats.
    // Detection requires 20 or more consecutive packets.
    static const int s_probe_sizes[]                          = { 32 * 1024, 64 * 1024, 128 * 1024, 256 * 1024, 512 * 1024, 1024 * 1024, 0 };
    static const int s_probe_num_required_consecutive_packets = 20;

    int i;
    for (i = 0; (0 != s_probe_sizes[i]) && (FILE_TYPE_IS_UNKNOWN == type); ++i)
      if (mp3_reader_c::probe_file(io, size, s_probe_sizes[i], s_probe_num_required_consecutive_packets))
        type = FILE_TYPE_MP3;
      else if (ac3_reader_c::probe_file(io, size, s_probe_sizes[i], s_probe_num_required_consecutive_packets))
        type = FILE_TYPE_AC3;
      else if (aac_reader_c::probe_file(io, size, s_probe_sizes[i], s_probe_num_required_consecutive_packets))
        type = FILE_TYPE_AAC;
  }
  if (FILE_TYPE_IS_UNKNOWN == type) {
    // All text file types (subtitles).
    mm_text_io_c *text_io = NULL;
    try {
      text_io = new mm_text_io_c(new mm_file_io_c(file.name));
      size    = text_io->get_size();
    } catch (...) {
      mxerror(boost::format(Y("The source file '%1%' could not be opened successfully, or retrieving its size by seeking to the end did not work.\n")) % file.name);
    }

    if (srt_reader_c::probe_file(text_io, size))
      type = FILE_TYPE_SRT;
    else if (ssa_reader_c::probe_file(text_io, size))
      type = FILE_TYPE_SSA;
    else if (vobsub_reader_c::probe_file(text_io, size))
      type = FILE_TYPE_VOBSUB;
    else if (corepicture_reader_c::probe_file(text_io, size))
      type = FILE_TYPE_COREPICTURE;
    else if (usf_reader_c::probe_file(text_io, size))
      type = FILE_TYPE_USF;
    else
      type = FILE_TYPE_IS_UNKNOWN;

    delete text_io;
  }

  g_file_sizes += size;

  file.size     = size;
  file.type     = type;
}

/** \brief Selects a reader for displaying its progress information
*/
static void
display_progress() {
  static int64_t s_previous_progress_on = 0;
  static int s_previous_percentage      = -1;

  if (NULL == s_display_reader) {
    std::vector<filelist_t>::const_iterator i;

    const filelist_t *winner = NULL;
    for (auto &current : g_files)
      if (!current.appending && (0 != current.reader->get_num_packetizers()) && ((NULL == winner) || (current.size > winner->size)))
        winner = &current;

    if (NULL == winner) {
      for (auto &current : g_files)
        if (!current.appending && ((NULL == winner) || (current.size > winner->size)))
          winner = &current;
    }

    s_display_reader = winner->reader;
  }

  bool display_progress  = false;
  int current_percentage = (s_display_reader->get_progress() + s_display_files_done * 100) / s_display_path_length;
  int64_t current_time   = get_current_time_millis();

  if (   (-1 == s_previous_percentage)
      || ((100 == current_percentage) && (100 > s_previous_percentage))
      || ((current_percentage != s_previous_percentage) && ((current_time - s_previous_progress_on) >= 500)))
    display_progress = true;

  if (!display_progress)
    return;

  mxinfo(boost::format(Y("Progress: %1%%%%2%")) % current_percentage % "\r");

  s_previous_percentage  = current_percentage;
  s_previous_progress_on = current_time;
}

/** \brief Add some tags to the list of all tags
*/
void
add_tags(KaxTag *tags) {
  if (tags->ListSize() == 0)
    return;

  if (NULL == s_kax_tags)
    s_kax_tags = new KaxTags;

  s_kax_tags->PushElement(*tags);
}

/** \brief Add an attachment

   \param attachment The attachment specification to add
   \return The attachment UID created for this attachment.
*/
int64_t
add_attachment(attachment_t attachment) {
  // If the attachment is coming from an existing file then we should
  // check if we already have another attachment stored. This can happen
  // if we're concatenating files.
  if (0 != attachment.id) {
    for (auto &ex_attachment : g_attachments)
      if ((   (ex_attachment.id == attachment.id)
           && !hack_engaged(ENGAGE_NO_VARIABLE_DATA))
          ||
          (   (ex_attachment.name             == attachment.name)
           && (ex_attachment.description      == attachment.description)
           && (ex_attachment.data->get_size() == attachment.data->get_size())))
        return attachment.id;

    add_unique_uint32(attachment.id, UNIQUE_ATTACHMENT_IDS);

  } else
    // No ID yet. Let's assign one.
    attachment.id = create_unique_uint32(UNIQUE_ATTACHMENT_IDS);

  g_attachments.push_back(attachment);

  return attachment.id;
}

/** \brief Add a packetizer to the list of packetizers
*/
void
add_packetizer_globally(generic_packetizer_c *packetizer) {
  packetizer_t pack;
  pack.packetizer      = packetizer;
  pack.orig_packetizer = packetizer;
  pack.status          = FILE_STATUS_MOREDATA;
  pack.old_status      = pack.status;
  pack.file            = -1;

  int idx = 0;
  for (auto &file : g_files)
    if (file.reader == packetizer->m_reader) {
      pack.file      = idx;
      pack.orig_file = pack.file;
      break;
    } else
      ++idx;

  if (-1 == pack.file)
    mxerror(boost::format(Y("filelist_t not found for generic_packetizer_c. %1%\n")) % BUGMSG);

  g_packetizers.push_back(pack);
}

static void
set_timecode_scale() {
  bool video_present          = false;
  bool audio_present          = false;
  int64_t highest_sample_rate = 0;

  for (auto &ptzr : g_packetizers)
    if (ptzr.packetizer->get_track_type() == track_video)
      video_present = true;

    else if (ptzr.packetizer->get_track_type() == track_audio) {
      audio_present       = true;
      highest_sample_rate = std::max(static_cast<int64_t>(ptzr.packetizer->get_audio_sampling_freq()), highest_sample_rate);
    }

  if (   (TIMECODE_SCALE_MODE_FIXED != g_timecode_scale_mode)
      && audio_present
      && (0 < highest_sample_rate)
      && (   !video_present
          || (TIMECODE_SCALE_MODE_AUTO == g_timecode_scale_mode)))
    g_timecode_scale = (double)1000000000.0 / (double)highest_sample_rate - 1.0;

  g_max_ns_per_cluster                                    = std::min((int64_t)(32700 * g_timecode_scale), g_max_ns_per_cluster);
  GetChildAs<KaxTimecodeScale, EbmlUInteger>(s_kax_infos) = (int64_t)g_timecode_scale;

  g_kax_cues->SetGlobalTimecodeScale((int64_t)g_timecode_scale);
}

static void
render_ebml_head(mm_io_c *out) {
  if (NULL == s_head)
    s_head = new EbmlHead;

  unsigned int doc_type_read_version                     = hack_engaged(ENGAGE_NO_SIMPLE_BLOCKS) ? 1 : 2;

  GetChildAs<EDocType, EbmlString>(*s_head)              = outputting_webm() ? "webm" : "matroska";

  GetChildAs<EDocTypeVersion,     EbmlUInteger>(*s_head) = g_stereo_mode_used ? 3 : doc_type_read_version;
  GetChildAs<EDocTypeReadVersion, EbmlUInteger>(*s_head) = doc_type_read_version;

  s_head->Render(*out, true);
}

void
rerender_ebml_head() {
  mm_io_c *out = g_cluster_helper->get_output();
  out->save_pos(s_head->GetElementPosition());
  render_ebml_head(out);
  out->restore_pos();
}

/** \brief Render the basic EBML and Matroska headers

   Renders the segment information and track headers. Also reserves
   some space with EBML Void elements so that the headers can be
   overwritten safely by the rerender_headers function.
*/
static void
render_headers(mm_io_c *out) {
  try {
    render_ebml_head(out);

    s_kax_infos = &GetChild<KaxInfo>(*g_kax_segment);

    if ((NULL == g_video_packetizer) || (TIMECODE_SCALE_MODE_AUTO == g_timecode_scale_mode))
      s_kax_duration = new KaxMyDuration(EbmlFloat::FLOAT_64);
    else
      s_kax_duration = new KaxMyDuration(EbmlFloat::FLOAT_32);

    *(static_cast<EbmlFloat *>(s_kax_duration)) = 0.0;
    s_kax_infos->PushElement(*s_kax_duration);

    if (!hack_engaged(ENGAGE_NO_VARIABLE_DATA)) {
      std::string muxing_app                                    = std::string("libebml v") + EbmlCodeVersion + std::string(" + libmatroska v") + KaxCodeVersion;
      GetChildAs<KaxMuxingApp, EbmlUnicodeString>(s_kax_infos)  = cstrutf8_to_UTFstring(muxing_app);
      GetChildAs<KaxWritingApp, EbmlUnicodeString>(s_kax_infos) = cstrutf8_to_UTFstring(get_version_info("mkvmerge", static_cast<version_info_flags_e>(vif_full | vif_untranslated)));
      GetChild<KaxDateUTC>(*s_kax_infos).SetEpochDate(time(NULL));

    } else {
      GetChildAs<KaxMuxingApp, EbmlUnicodeString>(s_kax_infos)  = cstrutf8_to_UTFstring("no_variable_data");
      GetChildAs<KaxWritingApp, EbmlUnicodeString>(s_kax_infos) = cstrutf8_to_UTFstring("no_variable_data");
      GetChild<KaxDateUTC>(*s_kax_infos).SetEpochDate(0);
    }

    if (!g_segment_title.empty())
      GetChildAs<KaxTitle, EbmlUnicodeString>(s_kax_infos)      = cstrutf8_to_UTFstring(g_segment_title.c_str());

    bool first_file = (1 == g_file_num);

    // Generate the segment UIDs.
    if (!hack_engaged(ENGAGE_NO_VARIABLE_DATA)) {
      if (first_file) {
        if (g_forced_seguids.empty())
          s_seguid_current.generate_random();
        else {
          s_seguid_current = *g_forced_seguids.front();
          g_forced_seguids.pop_front();
        }
        s_seguid_next.generate_random();

      } else {                  // first_file?
        s_seguid_prev = s_seguid_current;
        if (g_forced_seguids.empty())
          s_seguid_current = s_seguid_next;
        else {
          s_seguid_current = *g_forced_seguids.front();
          g_forced_seguids.pop_front();
        }
        s_seguid_next.generate_random();
      }

    } else {
      memset(s_seguid_current.data(), 0, 128 / 8);
      memset(s_seguid_prev.data(),    0, 128 / 8);
      memset(s_seguid_next.data(),    0, 128 / 8);
    }

    if (!outputting_webm()) {
      // Set the segment UIDs.
      GetChild<KaxSegmentUID>(*s_kax_infos).CopyBuffer(s_seguid_current.data(), 128 / 8);

      // Set the segment family
      if (!g_segfamily_uids.empty()) {
        size_t i;
        for (i = 0; i < g_segfamily_uids.size(); i++)
          AddNewChild<KaxSegmentFamily>(*s_kax_infos).CopyBuffer(g_segfamily_uids[i].data(), 128 / 8);
      }

      // Set the chaptertranslate elements
      if (NULL != g_kax_info_chap) {
        // copy the KaxChapterTranslates in the current KaxInfo
        KaxChapterTranslate *chapter_translate = FINDFIRST(g_kax_info_chap, KaxChapterTranslate);
        while (NULL != chapter_translate) {
          s_kax_infos->PushElement(*new KaxChapterTranslate(*chapter_translate));
          chapter_translate = FINDNEXT(g_kax_info_chap, KaxChapterTranslate, chapter_translate);
        }
      }

      if (first_file && g_seguid_link_previous.is_set())
        GetChild<KaxPrevUID>(*s_kax_infos).CopyBuffer(g_seguid_link_previous->data(), 128 / 8);

      // The next segment UID is also set in finish_file(). This is not
      // redundant! It is set here as well in order to reserve enough space
      // for the KaxInfo structure in the file. If it is removed later then
      // an EbmlVoid element will be used for the freed space.
      if (g_seguid_link_next.is_set())
        GetChild<KaxNextUID>(*s_kax_infos).CopyBuffer(g_seguid_link_next->data(), 128 / 8);

      if (!g_no_linking && g_cluster_helper->splitting()) {
        GetChild<KaxNextUID>(*s_kax_infos).CopyBuffer(s_seguid_next.data(), 128 / 8);

        if (!first_file)
          GetChild<KaxPrevUID>(*s_kax_infos).CopyBuffer(s_seguid_prev.data(), 128 / 8);
      }
    }

    g_kax_segment->WriteHead(*out, 8);

    // Reserve some space for the meta seek stuff.
    g_kax_sh_main = new KaxSeekHead();
    s_kax_sh_void = new EbmlVoid();
    s_kax_sh_void->SetSize(4096);
    s_kax_sh_void->Render(*out);

    if (g_write_meta_seek_for_clusters)
      g_kax_sh_cues = new KaxSeekHead();

    if (first_file) {
      g_kax_last_entry = NULL;

      size_t i;
      for (i = 0; i < g_track_order.size(); i++)
        if ((g_track_order[i].file_id >= 0) && (g_track_order[i].file_id < static_cast<int>(g_files.size())) && !g_files[g_track_order[i].file_id].appending)
          g_files[g_track_order[i].file_id].reader->set_headers_for_track(g_track_order[i].track_id);

      for (i = 0; i < g_files.size(); i++)
        if (!g_files[i].appending)
          g_files[i].reader->set_headers();

      set_timecode_scale();

      for (i = 0; i < g_packetizers.size(); i++)
        if (NULL != g_packetizers[i].packetizer)
          g_packetizers[i].packetizer->fix_headers();

    } else
      set_timecode_scale();

    s_kax_infos->Render(*out, true);
    g_kax_sh_main->IndexThis(*s_kax_infos, *g_kax_segment);

    if (!g_packetizers.empty()) {
      g_kax_tracks->UpdateSize(true);
      uint64_t full_header_size = g_kax_tracks->ElementSize(true);
      g_kax_tracks->UpdateSize(false);

      g_kax_segment->PushElement(*g_kax_tracks);
      g_kax_tracks->Render(*out, false);
      g_kax_sh_main->IndexThis(*g_kax_tracks, *g_kax_segment);

      // Reserve some small amount of space for header changes by the
      // packetizers.
      s_void_after_track_headers = new EbmlVoid;
      s_void_after_track_headers->SetSize(1024 + full_header_size - g_kax_tracks->ElementSize(false));
      s_void_after_track_headers->Render(*out);
    }

  } catch (...) {
    mxerror(boost::format(Y("The track headers could not be rendered correctly. %1%\n")) % BUGMSG);
  }
}

/** \brief Overwrites the track headers with current values

   Can be used by packetizers that have to modify their headers
   depending on the track contents.
*/
void
rerender_track_headers() {
  g_kax_tracks->UpdateSize(false);

  int64_t new_void_size = s_void_after_track_headers->GetElementPosition() + s_void_after_track_headers->GetSize()
    - g_kax_tracks->GetElementPosition() - g_kax_tracks->ElementSize();

  s_out->save_pos(g_kax_tracks->GetElementPosition());
  g_kax_tracks->Render(*s_out, false);

  delete s_void_after_track_headers;
  s_void_after_track_headers = new EbmlVoid;
  s_void_after_track_headers->SetSize(new_void_size);
  s_void_after_track_headers->Render(*s_out);

  s_out->restore_pos();
}

/** \brief Render all attachments into the output file at the current position

   This function also makes sure that no duplicates are output. This might
   happen when appending files.
*/
static void
render_attachments(IOCallback *out) {
  if (NULL != s_kax_as)
    delete s_kax_as;

  s_kax_as           = new KaxAttachments();
  KaxAttached *kax_a = NULL;

  for (auto &attch : g_attachments) {
    if ((1 == g_file_num) || attch.to_all_files) {
      kax_a = NULL == kax_a ? &GetChild<KaxAttached>(*s_kax_as) : &GetNextChild<KaxAttached>(*s_kax_as, *kax_a);

      if (attch.description != "")
        GetChildAs<KaxFileDescription, EbmlUnicodeString>(kax_a) = cstrutf8_to_UTFstring(attch.description);

      if (attch.mime_type != "")
        GetChildAs<KaxMimeType, EbmlString>(kax_a)               = attch.mime_type;

      std::string name;
      if (attch.stored_name == "") {
        int path_sep_idx = attch.name.rfind(PATHSEP);
        if (-1 != path_sep_idx)
          name = attch.name.substr(path_sep_idx + 1);
        else
          name = attch.name;

      } else
        name = attch.stored_name;

      GetChildAs<KaxFileName, EbmlUnicodeString>(kax_a) = cstrutf8_to_UTFstring(name);
      GetChildAs<KaxFileUID, EbmlUInteger>(kax_a)       = attch.id;

      GetChild<KaxFileData>(*kax_a).CopyBuffer(attch.data->get_buffer(), attch.data->get_size());
    }
  }

  if (s_kax_as->ListSize() != 0)
    s_kax_as->Render(*out);
  else {
    // Delete the kax_as pointer so that it won't be referenced in a seek head.
    delete s_kax_as;
    s_kax_as = NULL;
  }
}

/** \brief Check the complete append mapping mechanism

   Each entry given with '--append-to' has to be checked for validity.
   For files that aren't managed with '--append-to' default entries have
   to be created.
*/
static void
check_append_mapping() {
  std::vector<int64_t>::iterator id;
  std::vector<filelist_t>::iterator src_file, dst_file;

  for (auto &amap : g_append_mapping) {
    // Check each mapping entry for validity.

    // 1. Is there a file with the src_file_id?
    if ((0 > amap.src_file_id) || (g_files.size() <= static_cast<size_t>(amap.src_file_id)))
      mxerror(boost::format(Y("There is no file with the ID '%1%'. The argument for '--append-to' was invalid.\n")) % amap.src_file_id);

    // 2. Is the "source" file in "append mode", meaning does its file name
    // start with a '+'?
    src_file = g_files.begin() + amap.src_file_id;
    if (!src_file->appending)
      mxerror(boost::format(Y("The file no. %1% ('%2%') is not being appended. The argument for '--append-to' was invalid.\n")) % amap.src_file_id % src_file->name);

    // 3. Is there a file with the dst_file_id?
    if ((0 > amap.dst_file_id) || (g_files.size() <= static_cast<size_t>(amap.dst_file_id)))
      mxerror(boost::format(Y("There is no file with the ID '%1%'. The argument for '--append-to' was invalid.\n")) % amap.dst_file_id);

    // 4. G_Files cannot be appended to itself.
    if (amap.src_file_id == amap.dst_file_id)
      mxerror(Y("Files cannot be appended to themselves. The argument for '--append-to' was invalid.\n"));
  }

  // Now let's check each appended file if there are NO append to mappings
  // available (in which case we fill in default ones) or if there are fewer
  // mappings than tracks that are to be copied (which is an error).
  int file_id = -1;
  for (auto &src_file : g_files) {
    ++file_id;

    if (!src_file.appending)
      continue;

    size_t count = boost::count_if(g_append_mapping, [=](const append_spec_t &e) { return e.src_file_id == file_id; });

    if ((0 < count) && (src_file.reader->m_used_track_ids.size() > count))
      mxerror(boost::format(Y("Only partial append mappings were given for the file no. %1% ('%2%'). Either don't specify any mapping (in which case the "
                              "default mapping will be used) or specify a mapping for all tracks that are to be copied.\n")) % file_id % src_file.name);
    else if (0 == count) {
      std::string missing_mappings;

      // Default mapping.
      for (auto id : src_file.reader->m_used_track_ids) {
        append_spec_t new_amap;

        new_amap.src_file_id  = file_id;
        new_amap.src_track_id = id;
        new_amap.dst_file_id  = file_id - 1;
        new_amap.dst_track_id = id;
        g_append_mapping.push_back(new_amap);

        if (!missing_mappings.empty())
          missing_mappings += ",";
        missing_mappings += (boost::format("%1%:%2%:%3%:%4%") % new_amap.src_file_id % new_amap.src_track_id % new_amap.dst_file_id % new_amap.dst_track_id).str();
      }
      mxinfo(boost::format(Y("No append mapping was given for the file no. %1% ('%2%'). A default mapping of %3% will be used instead. "
                             "Please keep that in mind if mkvmerge aborts with an error message regarding invalid '--append-to' options.\n"))
             % file_id % src_file.name % missing_mappings);
    }
  }

  // Some more checks.
  for (auto &amap : g_append_mapping) {
    src_file = g_files.begin() + amap.src_file_id;
    dst_file = g_files.begin() + amap.dst_file_id;

    // 5. Does the "source" file have a track with the src_track_id, and is
    // that track selected for copying?
    if (!mxfind2(id, amap.src_track_id, src_file->reader->m_used_track_ids))
      mxerror(boost::format(Y("The file no. %1% ('%2%') does not contain a track with the ID %3%, or that track is not to be copied. "
                              "The argument for '--append-to' was invalid.\n")) % amap.src_file_id % src_file->name % amap.src_track_id);

    // 6. Does the "destination" file have a track with the dst_track_id, and
    // that track selected for copying?
    if (!mxfind2(id, amap.dst_track_id, dst_file->reader->m_used_track_ids))
      mxerror(boost::format(Y("The file no. %1% ('%2%') does not contain a track with the ID %3%, or that track is not to be copied. Therefore no "
                              "track can be appended to it. The argument for '--append-to' was invalid.\n")) % amap.dst_file_id % dst_file->name % amap.dst_track_id);

    // 7. Is this track already mapped to somewhere else?
    for (auto &cmp_amap : g_append_mapping) {
      if (cmp_amap == amap)
        continue;

      if (   (cmp_amap.src_file_id  == amap.src_file_id)
          && (cmp_amap.src_track_id == amap.src_track_id))
        mxerror(boost::format(Y("The track %1% from file no. %2% ('%3%') is to be appended more than once. The argument for '--append-to' was invalid.\n"))
                % amap.src_track_id % amap.src_file_id % src_file->name);
    }

    // 8. Is there another track that is being appended to the dst_track_id?
    for (auto &cmp_amap : g_append_mapping) {
      if (cmp_amap == amap)
        continue;

      if (   (cmp_amap.dst_file_id  == amap.dst_file_id)
          && (cmp_amap.dst_track_id == amap.dst_track_id))
        mxerror(boost::format(Y("More than one track is to be appended to the track %1% from file no. %2% ('%3%'). The argument for '--append-to' was invalid.\n"))
                % amap.dst_track_id % amap.dst_file_id % dst_file->name);
    }
  }

  // Finally see if the packetizers can be connected and connect them if they
  // can.
  for (auto &amap : g_append_mapping) {
    std::vector<generic_packetizer_c *>::const_iterator gptzr;
    generic_packetizer_c *src_ptzr, *dst_ptzr;
    std::string error_message;
    int result;

    src_file = g_files.begin() + amap.src_file_id;
    src_ptzr = NULL;
    mxforeach(gptzr, src_file->reader->m_reader_packetizers)
      if ((*gptzr)->m_ti.m_id == amap.src_track_id) {
        src_ptzr = (*gptzr);
        break;
      }

    dst_file = g_files.begin() + amap.dst_file_id;
    dst_ptzr = NULL;
    mxforeach(gptzr, dst_file->reader->m_reader_packetizers)
      if ((*gptzr)->m_ti.m_id == amap.dst_track_id) {
        dst_ptzr = (*gptzr);
        break;
      }

    if ((NULL == src_ptzr) || (NULL == dst_ptzr))
      mxerror(boost::format("((NULL == src_ptzr) || (NULL == dst_ptzr)). %1%\n") % BUGMSG);

    // And now try to connect the packetizers.
    result = src_ptzr->can_connect_to(dst_ptzr, error_message);
    if (CAN_CONNECT_MAYBE_CODECPRIVATE == result)
      mxwarn(boost::format(Y("The track number %1% from the file '%2%' can probably not be appended correctly to the track number %3% from the file '%4%': %5% "
                             "Please make sure that the resulting file plays correctly the whole time. "
                             "The author of this program will probably not give support for playback issues with the resulting file.\n"))
             % amap.src_track_id % g_files[amap.src_file_id].name
             % amap.dst_track_id % g_files[amap.dst_file_id].name
             % error_message);

    else if (CAN_CONNECT_YES != result) {
      std::string reason(  result == CAN_CONNECT_NO_FORMAT     ? Y("The formats do not match.")
                         : result == CAN_CONNECT_NO_PARAMETERS ? Y("The track parameters do not match.")
                         :                                       Y("The reason is unknown."));
      mxerror(boost::format(Y("The track number %1% from the file '%2%' cannot be appended to the track number %3% from the file '%4%'. %5%\n"))
              % amap.src_track_id % g_files[amap.src_file_id].name
              % amap.dst_track_id % g_files[amap.dst_file_id].name
              % reason);
    }

    src_ptzr->connect(dst_ptzr);
    dst_file->appended_to = true;
  }

  // Calculate the "longest path" -- meaning the maximum number of
  // concatenated files. This is needed for displaying the progress.
  std::vector<append_spec_t>::iterator amap;
  mxforeach(amap, g_append_mapping) {
    // Is this the first in a chain?
    auto cmp_amap = boost::find_if(g_append_mapping, [&](const append_spec_t &e) {
      return (*amap              != e)
          && (amap->dst_file_id  == e.src_file_id)
          && (amap->dst_track_id == e.src_track_id);
      });

    if (cmp_amap != g_append_mapping.end())
      continue;

    // Find consecutive mappings.
    auto trav_amap  = amap;
    int path_length = 2;
    do {
      mxforeach(cmp_amap, g_append_mapping)
        if (   (trav_amap->src_file_id  == cmp_amap->dst_file_id)
            && (trav_amap->src_track_id == cmp_amap->dst_track_id)) {
          trav_amap = cmp_amap;
          path_length++;
          break;
        }
    } while (cmp_amap != g_append_mapping.end());

    if (path_length > s_display_path_length)
      s_display_path_length = path_length;
  }
}

/** \brief Add chapters from the readers and calculate the max size

   The reader do not add their chapters to the global chapter pool.
   This has to be done after creating the readers. Only the chapters
   of readers that aren't appended are put into the pool right away.
   The other chapters are added when a packetizer is appended because
   the chapter timecodes have to be adjusted by the length of the file
   the packetizer is appended to.
   This function also calculates the sum of all chapter sizes so that
   enough space can be allocated at the start of each output file.
*/
void
calc_max_chapter_size() {
  // Step 1: Add all chapters from files that are not being appended.
  for (auto &file : g_files) {
    if (file.appending)
      continue;

    KaxChapters *chapters = file.reader->m_chapters;
    if (NULL == chapters)
      continue;

    if (NULL == g_kax_chapters)
      g_kax_chapters = new KaxChapters;

    move_chapters_by_edition(*g_kax_chapters, *chapters);
    delete chapters;
    file.reader->m_chapters = NULL;
  }

  // Step 2: Fix the mandatory elements and count the size of all chapters.
  s_max_chapter_size = 0;
  if (NULL != g_kax_chapters) {
    fix_mandatory_chapter_elements(g_kax_chapters);
    g_kax_chapters->UpdateSize(true);
    s_max_chapter_size += g_kax_chapters->ElementSize();
  }

  for (auto &file : g_files) {
    KaxChapters *chapters = file.reader->m_chapters;
    if (NULL == chapters)
      continue;

    fix_mandatory_chapter_elements(chapters);
    chapters->UpdateSize(true);
    s_max_chapter_size += chapters->ElementSize();
  }
}

/** \brief Creates the file readers

   For each file the appropriate file reader class is instantiated.
   The newly created class must read all track information in its
   contrsuctor and throw an exception in case of an error. Otherwise
   it is assumed that the file can be hanlded.
*/
void
create_readers() {
  for (auto &file : g_files) {
    try {
      mm_io_cptr input_file = open_input_file(file);

      switch (file.type) {
        case FILE_TYPE_AAC:
          file.reader = new aac_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_AC3:
          file.reader = new ac3_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_AVC_ES:
          file.reader = new avc_es_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_AVI:
          file.reader = new avi_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_DIRAC:
          file.reader = new dirac_es_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_DTS:
          file.reader = new dts_reader_c(*file.ti, input_file);
          break;
#if defined(HAVE_FLAC_FORMAT_H)
        case FILE_TYPE_FLAC:
          file.reader = new flac_reader_c(*file.ti, input_file);
          break;
#endif
        case FILE_TYPE_IVF:
          file.reader = new ivf_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_MATROSKA:
          file.reader = new kax_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_MP3:
          file.reader = new mp3_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_MPEG_ES:
          file.reader = new mpeg_es_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_MPEG_PS:
          file.reader = new mpeg_ps_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_MPEG_TS:
          file.reader = new mpeg_ts_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_OGM:
          file.reader = new ogm_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_PGSSUP:
          file.reader = new pgssup_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_QTMP4:
          file.reader = new qtmp4_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_REAL:
          file.reader = new real_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_SSA:
          file.reader = new ssa_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_SRT:
          file.reader = new srt_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_TRUEHD:
          file.reader = new truehd_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_TTA:
          file.reader = new tta_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_USF:
          file.reader = new usf_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_COREPICTURE:
          file.reader = new corepicture_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_VC1:
          file.reader = new vc1_es_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_VOBBTN:
          file.reader = new vobbtn_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_VOBSUB:
          file.reader = new vobsub_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_WAV:
          file.reader = new wav_reader_c(*file.ti, input_file);
          break;
        case FILE_TYPE_WAVPACK4:
          file.reader = new wavpack_reader_c(*file.ti, input_file);
          break;
        default:
          mxerror(boost::format(Y("EVIL internal bug! (unknown file type). %1%\n")) % BUGMSG);
          break;
      }

      file.reader->read_headers();

    } catch (mtx::mm_io::open_x &error) {
      mxerror(boost::format(Y("The demultiplexer for the file '%1%' failed to initialize:\n%2%\n")) % file.ti->m_fname % Y("The file could not be opened for reading, or there was not enough data to parse its headers."));

    } catch (mtx::input::open_x &error) {
      mxerror(boost::format(Y("The demultiplexer for the file '%1%' failed to initialize:\n%2%\n")) % file.ti->m_fname % Y("The file could not be opened for reading, or there was not enough data to parse its headers."));

    } catch (mtx::input::invalid_format_x &error) {
      mxerror(boost::format(Y("The demultiplexer for the file '%1%' failed to initialize:\n%2%\n")) % file.ti->m_fname % Y("The file content does not match its format type and was not recognized."));

    } catch (mtx::input::header_parsing_x &error) {
      mxerror(boost::format(Y("The demultiplexer for the file '%1%' failed to initialize:\n%2%\n")) % file.ti->m_fname % Y("The file headers could not be parsed, e.g. because they're incomplete, invalid or damaged."));

    } catch (mtx::input::exception &error) {
      mxerror(boost::format(Y("The demultiplexer for the file '%1%' failed to initialize:\n%2%\n")) % file.ti->m_fname % error.error());
    }
  }

  if (!g_identifying) {
    // Create the packetizers.
    for (auto &file : g_files) {
      file.reader->m_appending = file.appending;
      file.reader->create_packetizers();
    }
    // Check if all track IDs given on the command line are actually
    // present.
    for (auto &file : g_files) {
      file.reader->check_track_ids_and_packetizers();
      file.num_unfinished_packetizers     = file.reader->m_reader_packetizers.size();
      file.old_num_unfinished_packetizers = file.num_unfinished_packetizers;
    }

    // Check if the append mappings are ok.
    check_append_mapping();

    // Calculate the size of all attachments for split control.
    for (auto &att : g_attachments) {
      g_attachment_sizes_first += att.data->get_size();
      if (att.to_all_files)
        g_attachment_sizes_others += att.data->get_size();
    }

    calc_max_chapter_size();
  }
}

/** \brief Transform the output filename and insert the current file number

   Rules and search order:
   \arg %d
   \arg %[0-9]+d
   \arg . ("-%03d" will be inserted before the .)
   \arg "-%03d" will be appended
*/
std::string
create_output_name() {
  std::string s = g_outfile;
  int p2   = 0;
  // First possibility: %d
  int p    = s.find("%d");
  if (0 <= p) {
    s.replace(p, 2, to_string(g_file_num));

    return s;
  }

  // Now search for something like %02d
  p = s.find("%");
  if (0 <= p) {
    p2 = s.find("d", p + 1);
    if (0 <= p2) {
      int i;
      for (i = p + 1; i < p2; i++)
        if (!isdigit(s[i]))
          break;

      std::string format(&s.c_str()[p]);
      format.erase(p2 - p + 1);
      s.replace(p, format.size(), (boost::format(format) % g_file_num).str());

      return s;
    }
  }

  std::string buffer = (boost::format("-%|1$03d|") % g_file_num).str();

  // See if we can find a '.'.
  p = s.rfind(".");
  if (0 <= p)
    s.insert(p, buffer);
  else
    s.append(buffer);

  return s;
}

void
add_tags_from_cue_chapters() {
  if ((NULL == g_tags_from_cue_chapters) || ptzrs_in_header_order.empty())
    return;

  bool found = false;
  int tuid   = 0;
  size_t i;
  for (i = 0; ptzrs_in_header_order.size() > i; ++i)
    if (ptzrs_in_header_order[i]->get_track_type() == 'v') {
      found = true;
      tuid  = ptzrs_in_header_order[i]->get_uid();
      break;
    }

  if (!found)
    for (i = 0; ptzrs_in_header_order.size() > i; ++i)
      if (ptzrs_in_header_order[i]->get_track_type() == 'a') {
        found = true;
        tuid  = ptzrs_in_header_order[i]->get_uid();
        break;
      }

  if (!found)
    tuid = ptzrs_in_header_order[0]->get_uid();

  for (i = 0; g_tags_from_cue_chapters->ListSize() > i; ++i)
    GetChildAs<KaxTagTrackUID, EbmlUInteger>(GetChild<KaxTagTargets>(*static_cast<KaxTag *>((*g_tags_from_cue_chapters)[i]))) = tuid;

  if (NULL == s_kax_tags)
    s_kax_tags = g_tags_from_cue_chapters;
  else {
    while (g_tags_from_cue_chapters->ListSize() > 0) {
      s_kax_tags->PushElement(*(*g_tags_from_cue_chapters)[0]);
      g_tags_from_cue_chapters->Remove(0);
    }
    delete g_tags_from_cue_chapters;
  }

  g_tags_from_cue_chapters = NULL;
}

/** \brief Render an EbmlVoid element as a placeholder for chapters

    Chapters cannot be rendered at the start of the file until the
    file's actual length is known during \c finish_file(). However,
    the maximum size of chapters is know. So we reserve space at the
    beginning of the file for all of the chapters.

    WebM compliant files must not contain chapters. This function
    issues a warning and invalidates the chapters if this is the case.
 */
static void
render_chapter_void_placeholder() {
  if (0 >= s_max_chapter_size)
    return;

  if (outputting_webm()) {
    mxwarn(boost::format(Y("Chapters are not allowed in WebM compliant files. No chapters will be written into any output file.\n")));

    delete g_kax_chapters;
    g_kax_chapters     = NULL;
    s_max_chapter_size = 0;

    return;
  }

  s_kax_chapters_void = new EbmlVoid;
  s_kax_chapters_void->SetSize(s_max_chapter_size + 100);
  s_kax_chapters_void->Render(*s_out);
}

/** \brief Prepare tag elements for rendering

    Adds missing mandatory elements to the tag structures and sorts
    them. Also determines the maximum size needed for rendering the
    tags.

    WebM compliant files must not contain tags. This function
    issues a warning and invalidates the tags if this is the case.
 */
static void
prepare_tags_for_rendering() {
  if (NULL == s_kax_tags)
    return;

  if (outputting_webm()) {
    mxwarn(boost::format(Y("Tags are not allowed in WebM compliant files. No tags will be written into any output file.\n")));

    delete s_kax_tags;
    s_kax_tags  = NULL;
    g_tags_size = 0;

    return;
  }

  fix_mandatory_tag_elements(s_kax_tags);
  sort_ebml_master(s_kax_tags);
  if (!s_kax_tags->CheckMandatory())
    mxerror(boost::format(Y("Some tag elements are missing (this error should not have occured - another similar error should have occured earlier). %1%\n")) % BUGMSG);

  s_kax_tags->UpdateSize();
  g_tags_size = s_kax_tags->ElementSize();
}

/** \brief Creates the next output file

   Creates a new file name depending on the split settings. Opens that
   file for writing and calls \c render_headers(). Also renders
   attachments if they exist and the chapters if no splitting is used.
*/
void
create_next_output_file() {
  std::string this_outfile = g_cluster_helper->splitting() ? create_output_name() : g_outfile;

  g_kax_segment       = new KaxSegment();
  g_kax_cues          = new KaxCues();
  g_kax_cues->SetGlobalTimecodeScale((int64_t)g_timecode_scale);

  // Open the output file.
  try {
    s_out = mm_write_cache_io_c::open(this_outfile, 20 * 1024 * 1024);
  } catch (...) {
    mxerror(boost::format(Y("The output file '%1%' could not be opened for writing (%2%).\n")) % this_outfile % strerror(errno));
  }

  if (verbose)
    mxinfo(boost::format(Y("The file '%1%' has been opened for writing.\n")) % this_outfile);

  g_cluster_helper->set_output(s_out.get_object());
  render_headers(s_out.get_object());
  render_attachments(s_out.get_object());
  render_chapter_void_placeholder();
  add_tags_from_cue_chapters();
  prepare_tags_for_rendering();

  g_file_num++;
}

/** \brief Finishes and closes the current file

   Renders the data that is generated during the muxing run. The cues
   and meta seek information are rendered at the end. If splitting is
   active the chapters are stripped to those that actually lie in this
   file and rendered at the front.  The segment duration and the
   segment size are set to their actual values.
*/
int64_t
finish_file(bool last_file) {
  mxinfo("\n");

  // Render the track headers a second time if the user has requested that.
  if (hack_engaged(ENGAGE_WRITE_HEADERS_TWICE)) {
    EbmlElement *second_tracks = g_kax_tracks->Clone();
    second_tracks->Render(*s_out);
    g_kax_sh_main->IndexThis(*second_tracks, *g_kax_segment);
    delete second_tracks;
  }

  // Render the cues.
  if (g_write_cues && g_cue_writing_requested) {
    if (1 <= verbose)
      mxinfo(Y("The cue entries (the index) are being written..."));
    g_kax_cues->Render(*s_out);
    if (1 <= verbose)
      mxinfo("\n");
  }

  // Now re-render the s_kax_duration and fill in the biggest timecode
  // as the file's duration.
  s_out->save_pos(s_kax_duration->GetElementPosition());
  mxverb(3,
         boost::format("mkvmerge: s_kax_duration: gdur %1% tcs %2% du %3%\n")
         % g_cluster_helper->get_duration() % g_timecode_scale
         % irnd((double)g_cluster_helper->get_duration() / (double)((int64_t)g_timecode_scale)));

  *(static_cast<EbmlFloat *>(s_kax_duration)) = irnd((double)g_cluster_helper->get_duration() / (double)((int64_t)g_timecode_scale));
  s_kax_duration->Render(*s_out);

  // If splitting is active and this is the last part then handle the
  // 'next segment UID'. If it was given on the command line then set it here.
  // Otherwise remove an existing one (e.g. from file linking during
  // splitting).

  s_kax_infos->UpdateSize(true);
  int64_t info_size = s_kax_infos->ElementSize();
  int changed       = 0;

  if (last_file && g_seguid_link_next.is_set()) {
    GetChild<KaxNextUID>(*s_kax_infos).CopyBuffer(g_seguid_link_next->data(), 128 / 8);
    changed = 1;

  } else if (!last_file && g_no_linking) {
    size_t i;
    for (i = 0; s_kax_infos->ListSize() > i; ++i)
      if (EbmlId(*(*s_kax_infos)[i]) == EBML_ID(KaxNextUID)) {
        delete (*s_kax_infos)[i];
        s_kax_infos->Remove(i);
        changed = 2;
        break;
      }
  }

  if (0 != changed) {
    s_out->setFilePointer(s_kax_infos->GetElementPosition());
    s_kax_infos->UpdateSize(true);
    info_size -= s_kax_infos->ElementSize();
    s_kax_infos->Render(*s_out, true);
    if (2 == changed) {
      if (2 < info_size) {
        EbmlVoid void_after_infos;
        void_after_infos.SetSize(info_size);
        void_after_infos.UpdateSize();
        void_after_infos.SetSize(info_size - void_after_infos.HeadSize());
        void_after_infos.Render(*s_out);

      } else if (0 < info_size) {
        char zero[2] = {0, 0};
        s_out->write(zero, info_size);
      }
    }
  }
  s_out->restore_pos();

  // Render the segment info a second time if the user has requested that.
  if (hack_engaged(ENGAGE_WRITE_HEADERS_TWICE)) {
    s_kax_infos->Render(*s_out);
    g_kax_sh_main->IndexThis(*s_kax_infos, *g_kax_segment);
  }

  // Select the chapters that lie in this file and render them in the space
  // that was resesrved at the beginning.
  KaxChapters *chapters_here = NULL;

  if (NULL != g_kax_chapters) {
    int64_t offset = g_no_linking ? g_cluster_helper->get_first_timecode_in_file() : 0;
    int64_t start  = g_cluster_helper->get_first_timecode_in_file();
    int64_t end    = start + g_cluster_helper->get_duration();

    chapters_here  = copy_chapters(g_kax_chapters);

    if (g_cluster_helper->splitting())
      chapters_here = select_chapters_in_timeframe(chapters_here, start, end, offset);

    if (NULL != chapters_here) {
      merge_chapter_entries(*chapters_here);
      sort_ebml_master(chapters_here);
      s_kax_chapters_void->ReplaceWith(*chapters_here, *s_out, true, true);
      s_chapters_in_this_file = static_cast<KaxChapters *>(chapters_here->Clone());
    }

    delete s_kax_chapters_void;
    s_kax_chapters_void = NULL;

  }

  // Render the meta seek information with the cues
  if (g_write_meta_seek_for_clusters && (g_kax_sh_cues->ListSize() > 0) && !hack_engaged(ENGAGE_NO_META_SEEK)) {
    g_kax_sh_cues->UpdateSize();
    g_kax_sh_cues->Render(*s_out);
    g_kax_sh_main->IndexThis(*g_kax_sh_cues, *g_kax_segment);
  }

  // Render the tags if we have any.
  KaxTags *tags_here = NULL;
  if (NULL != s_kax_tags) {
    if (NULL == s_chapters_in_this_file) {
      KaxChapters temp_chapters;
      tags_here = select_tags_for_chapters(*s_kax_tags, temp_chapters);
    } else
      tags_here = select_tags_for_chapters(*s_kax_tags, *s_chapters_in_this_file);

    if (NULL != tags_here) {
      fix_mandatory_tag_elements(tags_here);
      tags_here->UpdateSize();
      tags_here->Render(*s_out, true);
    }

  }

  // Write meta seek information if it is not disabled.
  if (g_cue_writing_requested)
    g_kax_sh_main->IndexThis(*g_kax_cues, *g_kax_segment);

  if (NULL != tags_here) {
    g_kax_sh_main->IndexThis(*tags_here, *g_kax_segment);
    delete tags_here;
  }

  if (NULL != s_chapters_in_this_file) {
    delete s_chapters_in_this_file;
    s_chapters_in_this_file = NULL;
  }

  if (NULL != chapters_here) {
    if (!hack_engaged(ENGAGE_NO_CHAPTERS_IN_META_SEEK))
      g_kax_sh_main->IndexThis(*chapters_here, *g_kax_segment);
    delete chapters_here;

  } else if (!g_cluster_helper->splitting() && (NULL != g_kax_chapters))
    if (!hack_engaged(ENGAGE_NO_CHAPTERS_IN_META_SEEK))
      g_kax_sh_main->IndexThis(*g_kax_chapters, *g_kax_segment);

  if (NULL != s_kax_as) {
    g_kax_sh_main->IndexThis(*s_kax_as, *g_kax_segment);
    delete s_kax_as;
    s_kax_as = NULL;
  }

  if ((g_kax_sh_main->ListSize() > 0) && !hack_engaged(ENGAGE_NO_META_SEEK)) {
    g_kax_sh_main->UpdateSize();
    if (s_kax_sh_void->ReplaceWith(*g_kax_sh_main, *s_out, true) == INVALID_FILEPOS_T)
      mxwarn(boost::format(Y("This should REALLY not have happened. The space reserved for the first meta seek element was too small. Size needed: %1%. %2%\n"))
             % g_kax_sh_main->ElementSize() % BUGMSG);
  }

  // Set the correct size for the segment.
  int64_t final_file_size = s_out->getFilePointer();
  if (g_kax_segment->ForceSize(final_file_size - g_kax_segment->GetElementPosition() - g_kax_segment->HeadSize()))
    g_kax_segment->OverwriteHead(*s_out);

  s_out.clear();

  // The tracks element must not be deleted.
  size_t i;
  for (i = 0; i < g_kax_segment->ListSize(); ++i)
    if (NULL == dynamic_cast<KaxTracks *>((*g_kax_segment)[i]))
      delete (*g_kax_segment)[i];
  g_kax_segment->RemoveAll();

  delete g_kax_segment;
  delete g_kax_cues;
  delete s_kax_sh_void;
  delete g_kax_sh_main;
  delete s_void_after_track_headers;
  if (NULL != g_kax_sh_cues)
    delete g_kax_sh_cues;
  delete s_head;

  s_head = NULL;

  return final_file_size;
}

static void establish_deferred_connections(filelist_t &file);

/** \brief Append a packetizer to another one

   Appends a packetizer to another one. Finds the packetizer that is
   to replace the current one, informs the user about the action,
   connects the two packetizers and changes the structs to reflect
   the switch.

   \param ptzr The packetizer that is to be replaced.
   \param amap The append specification the replacement is based upon.
*/
void
append_track(packetizer_t &ptzr,
             const append_spec_t &amap,
             filelist_t *deferred_file = NULL) {
  filelist_t &src_file = g_files[amap.src_file_id];
  filelist_t &dst_file = g_files[amap.dst_file_id];

  if (NULL != deferred_file)
    src_file.deferred_max_timecode_seen = deferred_file->reader->m_max_timecode_seen;

  // Find the generic_packetizer_c that we will be appending to the one
  // stored in ptzr.
  std::vector<generic_packetizer_c *>::const_iterator gptzr;
  mxforeach(gptzr, src_file.reader->m_reader_packetizers)
    if (amap.src_track_id == (*gptzr)->m_ti.m_id)
      break;

  if (src_file.reader->m_reader_packetizers.end() == gptzr)
    mxerror(boost::format(Y("Could not find gptzr when appending. %1%\n")) % BUGMSG);

  // If we're dealing with a subtitle track or if the appending file contains
  // chapters then we have to suck the previous file dry. See below for the
  // reason (short version: we need all max_timecode_seen values).
  if (   !dst_file.done
      && (   (APPEND_MODE_FILE_BASED     == g_append_mode)
          || ((*gptzr)->get_track_type() == track_subtitle)
          || (NULL                       != src_file.reader->m_chapters))) {
    dst_file.reader->read_all();
    dst_file.num_unfinished_packetizers     = 0;
    dst_file.old_num_unfinished_packetizers = 0;
    dst_file.done                           = true;
    establish_deferred_connections(dst_file);
  }

  if (   !ptzr.deferred
      && (track_subtitle == (*gptzr)->get_track_type())
      && (             0 == dst_file.reader->m_num_video_tracks)
      && (            -1 == src_file.deferred_max_timecode_seen)
      && (          NULL != g_video_packetizer)) {

    for (auto &file : g_files) {
      if (file.done)
        continue;

      std::vector<generic_packetizer_c *>::const_iterator vptzr;
      mxforeach(vptzr, file.reader->m_reader_packetizers)
        if ((*vptzr)->get_track_type() == track_video)
          break;

      if (vptzr == file.reader->m_reader_packetizers.end())
        continue;

      deferred_connection_t new_def_con;

      ptzr.deferred    = true;
      new_def_con.amap = amap;
      new_def_con.ptzr = &ptzr;
      file.deferred_connections.push_back(new_def_con);

      return;
    }
  }

  mxinfo(boost::format(Y("Appending track %1% from file no. %2% ('%3%') to track %4% from file no. %5% ('%6%').\n"))
         % (*gptzr)->m_ti.m_id % amap.src_file_id % (*gptzr)->m_ti.m_fname % ptzr.packetizer->m_ti.m_id % amap.dst_file_id % ptzr.packetizer->m_ti.m_fname);

  // Is the current file currently used for displaying the progress? If yes
  // then replace it with the next one.
  if (s_display_reader == dst_file.reader) {
    s_display_files_done++;
    s_display_reader = src_file.reader;
  }

  // Also fix the ptzr structure and reset the ptzr's state to "I want more".
  generic_packetizer_c *old_packetizer = ptzr.packetizer;
  ptzr.packetizer                      = *gptzr;
  ptzr.file                            = amap.src_file_id;
  ptzr.status                          = FILE_STATUS_MOREDATA;

  // If we're dealing with a subtitle track or if the appending file contains
  // chapters then we have to do some magic. During splitting timecodes are
  // offset by a certain amount. This amount is NOT the duration of the
  // previous file! That's why we cannot use
  // dst_file.reader->max_timecode_seen. Instead we have to find the first
  // packet in the appending file because its original timecode during the
  // split phase was the offset. If we have that we can find the corresponding
  // packetizer and use its max_timecode_seen.
  //
  // All this only applies to gapless tracks. Good luck with other files.
  // Some files types also allow access to arbitrary tracks and packets
  // (e.g. AVI and Quicktime). Those files will not work correctly for this.
  // But then again I don't expect that people will try to concatenate such
  // files if they've been split before.
  int64_t timecode_adjustment = dst_file.reader->m_max_timecode_seen;
  if ((APPEND_MODE_FILE_BASED == g_append_mode)
      && (   (track_subtitle != ptzr.packetizer->get_track_type())
          || !dst_file.reader->is_simple_subtitle_container()))
    // Intentionally left empty.
    ;

  else if (ptzr.deferred && (NULL != deferred_file))
    timecode_adjustment = src_file.deferred_max_timecode_seen;

  else if (   (track_subtitle == ptzr.packetizer->get_track_type())
           && (-1 < src_file.deferred_max_timecode_seen))
    timecode_adjustment = src_file.deferred_max_timecode_seen;

  else if (   (ptzr.packetizer->get_track_type() == track_subtitle)
           || (NULL != src_file.reader->m_chapters)) {
    if (NULL == src_file.reader->m_ptzr_first_packet)
      ptzr.status = ptzr.packetizer->read();

    if (NULL != src_file.reader->m_ptzr_first_packet) {
      std::vector<append_spec_t>::const_iterator cmp_amap;
      mxforeach(cmp_amap, g_append_mapping)
        if (   (cmp_amap->src_file_id  == amap.src_file_id)
            && (cmp_amap->src_track_id == src_file.reader->m_ptzr_first_packet->m_ti.m_id)
            && (cmp_amap->dst_file_id  == amap.dst_file_id))
          break;

      if (g_append_mapping.end() != cmp_amap)
        mxforeach(gptzr, dst_file.reader->m_reader_packetizers)
          if ((*gptzr)->m_ti.m_id == cmp_amap->dst_track_id) {
            timecode_adjustment = (*gptzr)->m_max_timecode_seen;
            break;
          }
    }
  }

  if ((APPEND_MODE_FILE_BASED == g_append_mode) || (ptzr.packetizer->get_track_type() == track_subtitle)) {
    mxverb(2, boost::format("append_track: new timecode_adjustment for append_mode == FILE_BASED or subtitle track: %1% for %2%\n")
           % format_timecode(timecode_adjustment) % ptzr.packetizer->m_ti.m_id);
    // The actual connection.
    ptzr.packetizer->connect(old_packetizer, timecode_adjustment);

  } else {
    mxverb(2, boost::format("append_track: new timecode_adjustment for append_mode == TRACK_BASED and NON subtitle track: %1% for %2%\n")
           % format_timecode(timecode_adjustment) % ptzr.packetizer->m_ti.m_id);
    // The actual connection.
    ptzr.packetizer->connect(old_packetizer);
  }

  // Append some more chapters and adjust their timecodes by the highest
  // timecode seen in the previous file/the track that we've been searching
  // for above.
  KaxChapters *chapters = src_file.reader->m_chapters;
  if (NULL != chapters) {
    if (NULL == g_kax_chapters)
      g_kax_chapters = new KaxChapters;
    else
      align_chapter_edition_uids(*g_kax_chapters, *chapters);
    adjust_chapter_timecodes(*chapters, timecode_adjustment);
    move_chapters_by_edition(*g_kax_chapters, *chapters);
    delete chapters;
    src_file.reader->m_chapters = NULL;
  }

  ptzr.deferred = false;
}

/** \brief Decide if packetizers have to be appended

   Iterates over all current packetizers and decides if the next one
   should be appended now. This is the case if the current packetizer
   has finished and there is another packetizer waiting to be appended.

   \return true if at least one track has been appended to another one.
*/
bool
append_tracks_maybe() {
  bool appended_a_track = false;

  for (auto &ptzr : g_packetizers) {
    if (ptzr.deferred)
      continue;

    if (!g_files[ptzr.orig_file].appended_to)
      continue;

    if (FILE_STATUS_DONE_AND_DRY != ptzr.status)
      continue;

    append_spec_t *amap = NULL;
    for (auto &amap_idx : g_append_mapping)
      if ((amap_idx.dst_file_id == ptzr.file) && (amap_idx.dst_track_id == ptzr.packetizer->m_ti.m_id)) {
        amap = &amap_idx;
        break;
      }

    if (NULL == amap)
      continue;

    append_track(ptzr, *amap);
    appended_a_track = true;
  }

  return appended_a_track;
}

/** \brief Establish deferred packetizer connections

   In some cases (e.g. subtitle only files being appended) establishing the
   connections is deferred until a file containing a video track has
   finished, too. This is necessary because the subtitle files themselves
   are usually "shorter" than the movie they belong to. This is not the
   case if the subs are already embedded with a movie in a single file.

   This function iterates over all deferred connections and establishes
   them.

   \param file All connections that have been deferred until this file has
     finished are established.
*/
static void
establish_deferred_connections(filelist_t &file) {
  std::vector<deferred_connection_t> def_cons = file.deferred_connections;
  file.deferred_connections.clear();

  for (auto &def_con : def_cons)
    append_track(*def_con.ptzr, def_con.amap, &file);

  // \todo Select a new file that the subs will defer to.
}

/** \brief Request packets and handle the next one

   Requests packets from each packetizer, selects the packet with the
   lowest timecode and hands it over to the cluster helper for
   rendering.  Also displays the progress.
*/
void
main_loop() {
  // Let's go!
  while (1) {
    debug_run_main_loop_hooks();

    // Step 1: Make sure a packet is available for each output
    // as long we haven't already processed the last one.
    for (auto &ptzr : g_packetizers) {
      if (FILE_STATUS_HOLDING == ptzr.status)
        ptzr.status = FILE_STATUS_MOREDATA;

      ptzr.old_status = ptzr.status;

      while (   !ptzr.pack.is_set()
             && (FILE_STATUS_MOREDATA == ptzr.status)
             && !ptzr.packetizer->packet_available())
        ptzr.status = ptzr.packetizer->read();

      if (   (FILE_STATUS_MOREDATA != ptzr.status)
          && (FILE_STATUS_MOREDATA == ptzr.old_status))
        ptzr.packetizer->force_duration_on_last_packet();

      if (!ptzr.pack.is_set())
        ptzr.pack = ptzr.packetizer->get_packet();

      if (!ptzr.pack.is_set() && (FILE_STATUS_DONE == ptzr.status))
        ptzr.status = FILE_STATUS_DONE_AND_DRY;

      // Has this packetizer changed its status from "data available" to
      // "file done" during this loop? If so then decrease the number of
      // unfinished packetizers in the corresponding file structure.
      if (   (FILE_STATUS_DONE_AND_DRY == ptzr.status)
          && (ptzr.old_status != ptzr.status)) {
        filelist_t &file = g_files[ptzr.file];
        file.num_unfinished_packetizers--;

        // If all packetizers for a file have finished then establish the
        // deferred connections.
        if ((0 >= file.num_unfinished_packetizers) && (0 < file.old_num_unfinished_packetizers)) {
          establish_deferred_connections(file);
          file.done = true;
        }
        file.old_num_unfinished_packetizers = file.num_unfinished_packetizers;
      }
    }

    // Step 2: Pick the packet with the lowest timecode and
    // stuff it into the Matroska file.
    packetizer_t * winner = NULL;
    for (auto &ptzr : g_packetizers) {
      if (ptzr.pack.is_set()) {
        if ((NULL == winner) || !winner->pack.is_set())
          winner = &ptzr;

        else if (ptzr.pack.is_set() && (ptzr.pack->assigned_timecode < winner->pack->assigned_timecode))
          winner = &ptzr;
      }
    }

    // Append the next track if appending is wanted.
    bool appended_a_track = append_tracks_maybe();

    if ((NULL != winner) && winner->pack.is_set()) {
      packet_cptr pack = winner->pack;

      // Step 3: Add the winning packet to a cluster. Full clusters will be
      // rendered automatically.
      g_cluster_helper->add_packet(pack);

      winner->pack = packet_cptr(NULL);

      // display some progress information
      if (1 <= verbose)
        display_progress();

    } else if (!appended_a_track) // exit if there are no more packets
      break;
  }

  // Render all remaining packets (if there are any).
  if ((NULL != g_cluster_helper) && (0 < g_cluster_helper->get_packet_count()))
    g_cluster_helper->render();

  if (1 <= verbose)
    mxinfo(boost::format(Y("Progress: 100%%%1%")) % "\r");
}

/** \brief Deletes the file readers and other associated objects
*/
static void
destroy_readers() {
  for (auto &file : g_files) {
    delete file.reader;
    delete file.ti;
  }

  g_files.clear();
  g_packetizers.clear();
}

/** \brief Uninitialization

   Frees memory and shuts down the readers.
*/
void
cleanup() {
  delete g_cluster_helper;
  g_cluster_helper = NULL;

  destroy_readers();
  g_attachments.clear();

  delete s_kax_tags;
  s_kax_tags = NULL;

  delete g_tags_from_cue_chapters;
  g_tags_from_cue_chapters = NULL;

  delete g_kax_chapters;
  g_kax_chapters = NULL;

  delete s_kax_as;
  s_kax_as = NULL;

  delete g_kax_info_chap;
  g_kax_info_chap = NULL;

  g_forced_seguids.clear();

  delete g_kax_tracks;
  g_kax_tracks = NULL;

}
