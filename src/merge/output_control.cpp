/** \brief output handling

   mkvmerge -- utility for splicing together matroska files
   from component media subtypes

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   \file
   \version $Id$

   \author Written by Moritz Bunkus <moritz@bunkus.org>.
   \author Modified by Steve Lhomme <steve.lhomme@free.fr>.
*/

#include "os.h"

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
#include <string>
#include <typeinfo>
#include <vector>

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

#include "chapters.h"
#include "cluster_helper.h"
#include "common.h"
#include "commonebml.h"
#include "hacks.h"
#include "mkvmerge.h"
#include "mm_io.h"
#include "output_control.h"
#include "r_aac.h"
#include "r_ac3.h"
#include "r_avi.h"
#include "r_dts.h"
#include "r_flac.h"
#include "r_matroska.h"
#include "r_mp3.h"
#include "r_mpeg.h"
#include "r_ogm.h"
#include "r_qtmp4.h"
#include "r_real.h"
#include "r_srt.h"
#include "r_ssa.h"
#include "r_tta.h"
#include "r_vobbtn.h"
#include "r_vobsub.h"
#include "r_wav.h"
#include "r_wavpack.h"
#include "tag_common.h"
#include "xml_element_mapping.h"

using namespace libmatroska;
using namespace std;

namespace libmatroska {

  class KaxMyDuration: public KaxDuration {
  public:
    KaxMyDuration(const EbmlFloat::Precision prec): KaxDuration() {
      SetPrecision(prec);
    }
  };
}

vector<packetizer_t> packetizers;
vector<filelist_t> files;
vector<attachment_t> attachments;
vector<track_order_t> track_order;
vector<append_spec_t> append_mapping;
vector<bitvalue_c> segfamily_uids;
int64_t attachment_sizes_first = 0, attachment_sizes_others = 0;

// Variables set by the command line parser.
string outfile;
int64_t file_sizes = 0;
int max_blocks_per_cluster = 65535;
int64_t max_ns_per_cluster = 2000000000;
bool write_cues = true, cue_writing_requested = false;
generic_packetizer_c *video_packetizer = NULL;
bool write_meta_seek_for_clusters = true;
bool no_lacing = false, no_linking = true;
int64_t split_after = -1;
bool split_by_time = false;
int split_max_num_files = 65535;
bool use_durations = false;

double timecode_scale = TIMECODE_SCALE;
timecode_scale_mode_e timecode_scale_mode = TIMECODE_SCALE_MODE_NORMAL;

float video_fps = -1.0;
int default_tracks[3], default_tracks_priority[3];

bool identifying = false, identify_verbose = false;

cluster_helper_c *cluster_helper = NULL;
KaxSegment *kax_segment;
KaxTracks *kax_tracks;
KaxTrackEntry *kax_last_entry;
KaxCues *kax_cues;
KaxSeekHead *kax_sh_main = NULL, *kax_sh_cues = NULL;
KaxChapters *kax_chapters = NULL;

static KaxInfo *kax_infos;
static KaxMyDuration *kax_duration;

static KaxTags *kax_tags = NULL;
KaxTags *tags_from_cue_chapters = NULL;
static KaxChapters *chapters_in_this_file = NULL;

static KaxAttachments *kax_as = NULL;

static EbmlVoid *kax_sh_void = NULL;
static EbmlVoid *kax_chapters_void = NULL;
static int64_t max_chapter_size = 0;
static EbmlVoid *void_after_track_headers = NULL;

string chapter_file_name;
string chapter_language;
string chapter_charset;

string segmentinfo_file_name;

string segment_title;
bool segment_title_set = false;

int64_t tags_size = 0;
static bool accept_tags = true;

int file_num = 1;
bool splitting = false;

string default_language = "und";

static mm_io_c *out = NULL;

static bitvalue_c seguid_prev(128), seguid_current(128), seguid_next(128);
bitvalue_c *seguid_link_previous = NULL, *seguid_link_next = NULL;

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
#ifdef DEBUG
  if (signum == SIGUSR1)
    debug_facility.dump_info();
#endif // DEBUG

  if (out == NULL)
    mxerror(_("mkvmerge was interrupted by a SIGINT (Ctrl+C?)\n"));

  mxwarn(_("\nmkvmerge received a SIGINT (probably because the user pressed "
           "Ctrl+C). Trying to sanitize the file. If mkvmerge hangs during "
           "this process you'll have to kill it manually.\n"));

  mxinfo(_("The file is being fixed, part 1/4..."));
  // Render the cues.
  if (write_cues && cue_writing_requested)
    kax_cues->Render(*out);
  mxinfo(_(" done\n"));

  mxinfo(_("The file is being fixed, part 2/4..."));
  // Now re-render the kax_duration and fill in the biggest timecode
  // as the file's duration.
  out->save_pos(kax_duration->GetElementPosition());
  *(static_cast<EbmlFloat *>(kax_duration)) =
    irnd((double)cluster_helper->get_duration() /
         (double)((int64_t)timecode_scale));
  kax_duration->Render(*out);
  out->restore_pos();
  mxinfo(_(" done\n"));

  mxinfo(_("The file is being fixed, part 3/4..."));
  // Write meta seek information if it is not disabled.
  if (cue_writing_requested)
    kax_sh_main->IndexThis(*kax_cues, *kax_segment);

  if ((kax_sh_main->ListSize() > 0) && !hack_engaged(ENGAGE_NO_META_SEEK)) {
    kax_sh_main->UpdateSize();
    if (kax_sh_void->ReplaceWith(*kax_sh_main, *out, true) == 0)
      mxwarn(_("This should REALLY not have happened. The space reserved for "
               "the first meta seek element was too small. %s\n"), BUGMSG);
  }
  mxinfo(_(" done\n"));

  mxinfo(_("The file is being fixed, part 4/4..."));
  // Set the correct size for the segment.
  if (kax_segment->ForceSize(out->getFilePointer() -
                             kax_segment->GetElementPosition() -
                             kax_segment->HeadSize()))
    kax_segment->OverwriteHead(*out);
  mxinfo(_(" done\n"));

  mxerror(_("mkvmerge was interrupted by a SIGINT (Ctrl+C?)\n"));
}
#endif

/** \brief Probe the file type

   Opens the input file and calls the \c probe_file function for each known
   file reader class. Uses \c mm_text_io_c for subtitle probing.
*/
void
get_file_type(filelist_t &file) {
  mm_io_c *mm_io;
  mm_text_io_c *mm_text_io;
  file_type_e type;
  int64_t size;
  int i;
  const int probe_sizes[] = {16 * 1024, 32 * 1024, 64 * 1024, 128 * 1024,
                             256 * 1024, 0};

  mm_io = NULL;
  mm_text_io = NULL;
  size = 0;
  try {
    mm_io = new mm_file_io_c(file.name);
    mm_io->setFilePointer(0, seek_end);
    size = mm_io->getFilePointer();
    mm_io->setFilePointer(0, seek_current);
  } catch (exception &ex) {
    mxerror(_("The source file '%s' could not be opened successfully, or "
              "retrieving its size by seeking to the end did not work.\n"),
            file.name.c_str());
  }

  type = FILE_TYPE_IS_UNKNOWN;
  if (avi_reader_c::probe_file(mm_io, size))
    type = FILE_TYPE_AVI;
  else if (kax_reader_c::probe_file(mm_io, size))
    type = FILE_TYPE_MATROSKA;
  else if (wav_reader_c::probe_file(mm_io, size))
    type = FILE_TYPE_WAV;
  else if (ogm_reader_c::probe_file(mm_io, size))
    type = FILE_TYPE_OGM;
  else if (flac_reader_c::probe_file(mm_io, size))
    type = FILE_TYPE_FLAC;
  else if (real_reader_c::probe_file(mm_io, size))
    type = FILE_TYPE_REAL;
  else if (qtmp4_reader_c::probe_file(mm_io, size))
    type = FILE_TYPE_QTMP4;
  else if (tta_reader_c::probe_file(mm_io, size))
    type = FILE_TYPE_TTA;
  else if (wavpack_reader_c::probe_file(mm_io, size))
    type = FILE_TYPE_WAVPACK4;
  else if (mpeg_ps_reader_c::probe_file(mm_io, size))
    type = FILE_TYPE_MPEG_PS;
  else {
    for (i = 0; (probe_sizes[i] != 0) && (type == FILE_TYPE_IS_UNKNOWN); i++)
      if (mp3_reader_c::probe_file(mm_io, size, probe_sizes[i], 5))
        type = FILE_TYPE_MP3;
      else if (ac3_reader_c::probe_file(mm_io, size, probe_sizes[i], 5))
        type = FILE_TYPE_AC3;
  }
  if (type != FILE_TYPE_IS_UNKNOWN)
    ;
  else if (mp3_reader_c::probe_file(mm_io, size, 2 * 1024 * 1024, 10))
    type = FILE_TYPE_MP3;
  else if (dts_reader_c::probe_file(mm_io, size))
    type = FILE_TYPE_DTS;
  else if (aac_reader_c::probe_file(mm_io, size))
    type = FILE_TYPE_AAC;
  else if (vobbtn_reader_c::probe_file(mm_io, size))
    type = FILE_TYPE_VOBBTN;
  else if (mpeg_es_reader_c::probe_file(mm_io, size))
    type = FILE_TYPE_MPEG_ES;
  else {
    delete mm_io;

    try {
      mm_text_io = new mm_text_io_c(new mm_file_io_c(file.name));
      mm_text_io->setFilePointer(0, seek_end);
      size = mm_text_io->getFilePointer();
      mm_text_io->setFilePointer(0, seek_current);
    } catch (exception &ex) {
      mxerror(_("The source file '%s' could not be opened successfully, or "
                "retrieving its size by seeking to the end did not work.\n"),
              file.name.c_str());
    }

    if (srt_reader_c::probe_file(mm_text_io, size))
      type = FILE_TYPE_SRT;
    else if (ssa_reader_c::probe_file(mm_text_io, size))
      type = FILE_TYPE_SSA;
    else if (vobsub_reader_c::probe_file(mm_text_io, size))
      type = FILE_TYPE_VOBSUB;
    else
      type = FILE_TYPE_IS_UNKNOWN;

    mm_io = mm_text_io;
  }

  delete mm_io;

  file_sizes += size;

  file.size = size;
  file.type = type;
}

static int display_counter = 0;
static int display_files_done = 0;
static int display_path_length = 1;
static generic_reader_c *display_reader = NULL;

/** \brief Selects a reader for displaying its progress information
*/
static void
display_progress() {
  if (display_reader == NULL) {
    vector<filelist_t>::const_iterator i;
    const filelist_t *winner;

    winner = &files[0];
    for (i = files.begin() + 1; i != files.end(); ++i)
      if (i->size > winner->size)
        winner = &(*i);

    display_reader = winner->reader;
  }
  if ((display_counter % 500) == 0) {
    display_counter = 0;
    mxinfo("progress: %d%%\r",
           (display_reader->get_progress() + display_files_done * 100) /
           display_path_length);
  }
  display_counter++;
}

/** \brief Add some tags to the list of all tags
*/
void
add_tags(KaxTag *tags) {
  if (!accept_tags)
    return;

  if (tags->ListSize() == 0)
    return;

  if (kax_tags == NULL)
    kax_tags = new KaxTags;

  kax_tags->PushElement(*tags);
}

/** \brief Add a packetizer to the list of packetizers
*/
void
add_packetizer_globally(generic_packetizer_c *packetizer) {
  vector<filelist_t>::iterator file;
  packetizer_t pack;

  memset(&pack, 0, sizeof(packetizer_t));
  pack.packetizer = packetizer;
  pack.orig_packetizer = packetizer;
  pack.status = FILE_STATUS_MOREDATA;
  pack.old_status = pack.status;
  pack.file = -1;
  foreach(file, files)
    if (file->reader == packetizer->reader) {
      pack.file = distance(files.begin(), file);
      pack.orig_file = pack.file;
      break;
    }
  if (pack.file == -1)
    die("filelist_t not found for generic_packetizer_c. %s\n", BUGMSG);
  packetizers.push_back(pack);
}

static void
set_timecode_scale() {
  vector<packetizer_t>::const_iterator ptzr;
  bool video_present, audio_present;
  int64_t highest_sample_rate;

  video_present = false;
  audio_present = false;
  highest_sample_rate = 0;

  foreach(ptzr, packetizers)
    if ((*ptzr).packetizer->get_track_type() == track_video)
      video_present = true;
    else if ((*ptzr).packetizer->get_track_type() == track_audio) {
      int64_t sample_rate;

      audio_present = true;
      sample_rate = (int64_t)(*ptzr).packetizer->get_audio_sampling_freq();
      if (sample_rate > highest_sample_rate)
        highest_sample_rate = sample_rate;
    }

  if (timecode_scale_mode != TIMECODE_SCALE_MODE_FIXED) {
    if (audio_present && (highest_sample_rate > 0) &&
        (!video_present ||
         (timecode_scale_mode == TIMECODE_SCALE_MODE_AUTO))) {
      int64_t max_ns_with_timecode_scale;

      timecode_scale = (double)1000000000.0 / (double)highest_sample_rate -
        1.0;
      max_ns_with_timecode_scale = (int64_t)(32700 * timecode_scale);
      if (max_ns_with_timecode_scale < max_ns_per_cluster)
        max_ns_per_cluster = max_ns_with_timecode_scale;

      mxverb(2, "mkvmerge: using sample precision timestamps. highest sample "
             "rate: %lld, new timecode_scale: %f, "
             "max_ns_with_timecode_scale: %lld, max_ns_per_cluster: %lld\n",
             highest_sample_rate, timecode_scale, max_ns_with_timecode_scale,
             max_ns_per_cluster);
    }
  }

  KaxTimecodeScale &time_scale = GetChild<KaxTimecodeScale>(*kax_infos);
  *(static_cast<EbmlUInteger *>(&time_scale)) = (int64_t)timecode_scale;

  kax_cues->SetGlobalTimecodeScale((int64_t)timecode_scale);
}

/** \brief Render the basic EBML and Matroska headers

   Renders the segment information and track headers. Also reserves
   some space with EBML Void elements so that the headers can be
   overwritten safely by the rerender_headers function.
*/
static void
render_headers(mm_io_c *rout) {
  EbmlHead head;
  bool first_file;
  int i;

  first_file = splitting && (file_num == 1);
  try {
    EDocType &doc_type = GetChild<EDocType>(head);
    *static_cast<EbmlString *>(&doc_type) = "matroska";
    EDocTypeVersion &doc_type_ver = GetChild<EDocTypeVersion>(head);
    *(static_cast<EbmlUInteger *>(&doc_type_ver)) = 1;
    EDocTypeReadVersion &doc_type_read_ver =
      GetChild<EDocTypeReadVersion>(head);
    *(static_cast<EbmlUInteger *>(&doc_type_read_ver)) = 1;

    head.Render(*rout);

    kax_infos = &GetChild<KaxInfo>(*kax_segment);

    if ((video_packetizer == NULL) ||
        (timecode_scale_mode == TIMECODE_SCALE_MODE_AUTO))
      kax_duration = new KaxMyDuration(EbmlFloat::FLOAT_64);
    else
      kax_duration = new KaxMyDuration(EbmlFloat::FLOAT_32);
    *(static_cast<EbmlFloat *>(kax_duration)) = 0.0;
    kax_infos->PushElement(*kax_duration);

    if (!hack_engaged(ENGAGE_NO_VARIABLE_DATA)) {
      string version = string("libebml v") + EbmlCodeVersion +
        string(" + libmatroska v") + KaxCodeVersion;
      *((EbmlUnicodeString *)&GetChild<KaxMuxingApp>(*kax_infos)) =
        cstr_to_UTFstring(version.c_str());
      *((EbmlUnicodeString *)&GetChild<KaxWritingApp>(*kax_infos)) =
        cstr_to_UTFstring(VERSIONINFO " built on " __DATE__ " " __TIME__);
      GetChild<KaxDateUTC>(*kax_infos).SetEpochDate(time(NULL));
    } else {
      *((EbmlUnicodeString *)&GetChild<KaxMuxingApp>(*kax_infos)) =
        cstr_to_UTFstring(ENGAGE_NO_VARIABLE_DATA);
      *((EbmlUnicodeString *)&GetChild<KaxWritingApp>(*kax_infos)) =
        cstr_to_UTFstring(ENGAGE_NO_VARIABLE_DATA);
      GetChild<KaxDateUTC>(*kax_infos).SetEpochDate(0);
    }

    if (segment_title.length() > 0)
      *((EbmlUnicodeString *)&GetChild<KaxTitle>(*kax_infos)) =
        cstrutf8_to_UTFstring(segment_title.c_str());

    // Generate the segment UIDs.
    if (!hack_engaged(ENGAGE_NO_VARIABLE_DATA)) {
      if (file_num == 1) {
        seguid_current.generate_random();
        seguid_next.generate_random();
      } else {
        seguid_prev = seguid_current;
        seguid_current = seguid_next;
        seguid_next.generate_random();
      }
    } else {
      memset(seguid_current.data(), 0, 128 / 8);
      memset(seguid_prev.data(), 0, 128 / 8);
      memset(seguid_next.data(), 0, 128 / 8);
    }

    // Set the segment UIDs.
    KaxSegmentUID &kax_seguid = GetChild<KaxSegmentUID>(*kax_infos);
    kax_seguid.CopyBuffer(seguid_current.data(), 128 / 8);

    // Set the segment family
    if (segfamily_uids.size() != 0) {
      for (i = 0; i < segfamily_uids.size(); i++) {
        KaxSegmentFamily &kax_family =
          AddNewChild<KaxSegmentFamily>(*kax_infos);
        kax_family.CopyBuffer(segfamily_uids[i].data(), 128 / 8);
      }
    }

    if (!no_linking) {
      if (!first_file) {
        KaxPrevUID &kax_prevuid = GetChild<KaxPrevUID>(*kax_infos);
        kax_prevuid.CopyBuffer(seguid_prev.data(), 128 / 8);
      } else if (seguid_link_previous != NULL) {
        KaxPrevUID &kax_prevuid = GetChild<KaxPrevUID>(*kax_infos);
        kax_prevuid.CopyBuffer(seguid_link_previous->data(), 128 / 8);
      }
      if (splitting) {
        KaxNextUID &kax_nextuid = GetChild<KaxNextUID>(*kax_infos);
        kax_nextuid.CopyBuffer(seguid_next.data(), 128 / 8);
      } else if (seguid_link_next != NULL) {
        KaxNextUID &kax_nextuid = GetChild<KaxNextUID>(*kax_infos);
        kax_nextuid.CopyBuffer(seguid_link_next->data(), 128 / 8);
      }
    }

    kax_segment->WriteHead(*rout, 8);

    // Reserve some space for the meta seek stuff.
    kax_sh_main = new KaxSeekHead();
    kax_sh_void = new EbmlVoid();
    kax_sh_void->SetSize(4096);
    kax_sh_void->Render(*rout);
    if (write_meta_seek_for_clusters)
      kax_sh_cues = new KaxSeekHead();

    kax_tracks = &GetChild<KaxTracks>(*kax_segment);
    kax_last_entry = NULL;

    for (i = 0; i < track_order.size(); i++)
      if ((track_order[i].file_id >= 0) &&
          (track_order[i].file_id < files.size()) &&
          !files[track_order[i].file_id].appending)
        files[track_order[i].file_id].reader->
          set_headers_for_track(track_order[i].track_id);
    for (i = 0; i < files.size(); i++)
      if (!files[i].appending)
        files[i].reader->set_headers();
    set_timecode_scale();
    for (i = 0; i < packetizers.size(); i++)
      if (packetizers[i].packetizer != NULL)
        packetizers[i].packetizer->fix_headers();

    kax_infos->Render(*rout, true);
    kax_sh_main->IndexThis(*kax_infos, *kax_segment);

    if (packetizers.size() > 0) {
      kax_tracks->Render(*rout,
                         !hack_engaged(ENGAGE_NO_DEFAULT_HEADER_VALUES));
      kax_sh_main->IndexThis(*kax_tracks, *kax_segment);

      // Reserve some small amount of space for header changes by the
      // packetizers.
      void_after_track_headers = new EbmlVoid;
      void_after_track_headers->SetSize(1024);
      void_after_track_headers->Render(*rout);
    }

  } catch (exception &ex) {
    mxerror(_("The track headers could not be rendered correctly. %s.\n"),
            BUGMSG);
  }
}

/** \brief Overwrites the track headers with current values

   Can be used by packetizers that have to modify their headers
   depending on the track contents.
*/
void
rerender_track_headers() {
  int64_t new_void_size;

  kax_tracks->UpdateSize(!hack_engaged(ENGAGE_NO_DEFAULT_HEADER_VALUES));
  new_void_size = void_after_track_headers->GetElementPosition() +
    void_after_track_headers->GetSize() -
    kax_tracks->GetElementPosition() -
    kax_tracks->ElementSize();
  out->save_pos(kax_tracks->GetElementPosition());
  kax_tracks->Render(*out, !hack_engaged(ENGAGE_NO_DEFAULT_HEADER_VALUES));
  delete void_after_track_headers;
  void_after_track_headers = new EbmlVoid;
  void_after_track_headers->SetSize(new_void_size);
  void_after_track_headers->Render(*out);
  out->restore_pos();
}

static uint32_t
kax_attachment_get_uid(const KaxAttached &a) {
  KaxFileUID *uid;

  uid = FINDFIRST(&a, KaxFileUID);
  if (uid == NULL)
    throw false;
  return uint32(*static_cast<EbmlUInteger *>(uid));
}

static UTFstring
kax_attachment_get_name(const KaxAttached &a) {
  KaxFileName *name;

  name = FINDFIRST(&a, KaxFileName);
  if (name == NULL)
    throw false;
  return UTFstring(*static_cast<EbmlUnicodeString *>(name));
}

static UTFstring
kax_attachment_get_description(const KaxAttached &a) {
  KaxFileDescription *description;

  description = FINDFIRST(&a, KaxFileDescription);
  if (description == NULL)
    return L"";
  return UTFstring(*static_cast<EbmlUnicodeString *>(description));
}

static string
kax_attachment_get_mime_type(const KaxAttached &a) {
  KaxMimeType *mime_type;

  mime_type = FINDFIRST(&a, KaxMimeType);
  if (mime_type == NULL)
    throw false;
  return string(*static_cast<EbmlString *>(mime_type));

}

static int64_t
kax_attachment_get_size(const KaxAttached &a) {
  KaxFileData *data;

  data = FINDFIRST(&a, KaxFileData);
  if (data == NULL)
    throw false;
  return data->GetSize();
}

static bool
operator ==(const KaxAttached &a1,
            const KaxAttached &a2) {
  try {
    if (kax_attachment_get_uid(a1) == kax_attachment_get_uid(a2))
      return true;
    if (kax_attachment_get_name(a1) != kax_attachment_get_name(a2))
      return false;
    if (kax_attachment_get_description(a1) !=
        kax_attachment_get_description(a2))
      return false;
    if (kax_attachment_get_size(a1) != kax_attachment_get_size(a2))
      return false;
    if (kax_attachment_get_mime_type(a1) != kax_attachment_get_mime_type(a2))
      return false;
    return true;
  } catch(...) {
    return false;
  }
}

/** \brief Render all attachments into the output file at the current position

   This function also makes sure that no duplicates are output. This might
   happen when appending files.
*/
static void
render_attachments(IOCallback *rout) {
  KaxAttachments *other_as, *other_as_test;
  KaxAttached *kax_a;
  KaxFileData *fdata;
  vector<attachment_t>::const_iterator attch;
  int i, k;
  int name;
  binary *buffer;
  mm_io_c *io;

  if (kax_as != NULL)
    delete kax_as;
  kax_as = new KaxAttachments();
  kax_a = NULL;
  foreach(attch, attachments) {
    if ((file_num == 1) || (*attch).to_all_files) {
      if (kax_a == NULL)
        kax_a = &GetChild<KaxAttached>(*kax_as);
      else
        kax_a = &GetNextChild<KaxAttached>(*kax_as, *kax_a);

      if ((*attch).description != "")
        *static_cast<EbmlUnicodeString *>
          (&GetChild<KaxFileDescription>(*kax_a)) =
          cstrutf8_to_UTFstring((*attch).description);

      if ((*attch).mime_type != "")
        *static_cast<EbmlString *>(&GetChild<KaxMimeType>(*kax_a)) =
          (*attch).mime_type;

      name = (*attch).name.length() - 1;
      while ((name > 0) && ((*attch).name[name] != PATHSEP))
        name--;
      if ((*attch).name[name] == PATHSEP)
        name++;

      *static_cast<EbmlUnicodeString *>
        (&GetChild<KaxFileName>(*kax_a)) =
        cstr_to_UTFstring((*attch).name.substr(name));

      *static_cast<EbmlUInteger *>
        (&GetChild<KaxFileUID>(*kax_a)) =
        create_unique_uint32(UNIQUE_ATTACHMENT_IDS);

      try {
        int64_t size;

        io = new mm_file_io_c((*attch).name);
        size = io->get_size();
        buffer = new binary[size];
        io->read(buffer, size);
        delete io;

        fdata = &GetChild<KaxFileData>(*kax_a);
        fdata->SetBuffer(buffer, size);
      } catch (...) {
        mxerror(_("The attachment '%s' could not be read.\n"),
                (*attch).name.c_str());
      }
    }
  }

  other_as = new KaxAttachments;
  other_as_test = new KaxAttachments;
  for (i = 0; i < files.size(); i++)
    files[i].reader->add_attachments(other_as_test);

  // Test if such an attachment already exists. This may be the case if
  // mkvmerge is concatenating files.
  i = 0;
  while (i < other_as_test->ListSize()) {
    bool found;

    kax_a = static_cast<KaxAttached *>((*other_as_test)[i]);
    found = false;
    for (k = 0; k < kax_as->ListSize(); k++)
      if (*static_cast<KaxAttached *>((*kax_as)[k]) == *kax_a) {
        found = true;
        break;
      }
    if (found) {
      i++;
      continue;
    }

    for (k = 0; k < other_as->ListSize(); k++)
      if (*static_cast<KaxAttached *>((*other_as)[k]) == *kax_a) {
        found = true;
        break;
      }
    if (found) {
      i++;
      continue;
    }

    other_as->PushElement(*kax_a);
    other_as_test->Remove(i);
  }
  delete other_as_test;

  while (other_as->ListSize() > 0) {
    kax_as->PushElement(*(*other_as)[0]);
    other_as->Remove(0);
  }
  delete other_as;

  if (kax_as->ListSize() != 0)
    kax_as->Render(*rout);
  else {
    delete kax_as;
    kax_as = NULL;
  }
}

/** \brief Check the complete append mapping mechanism

   Each entry given with '--append-to' has to be checked for validity.
   For files that aren't managed with '--append-to' default entries have
   to be created.
*/
static void
check_append_mapping() {
  vector<append_spec_t>::iterator amap, cmp_amap, trav_amap;
  vector<int64_t>::iterator id;
  vector<filelist_t>::iterator src_file, dst_file;
  int count, file_id;

  foreach(amap, append_mapping) {
    // Check each mapping entry for validity.

    // 1. Is there a file with the src_file_id?
    if ((amap->src_file_id < 0) || (amap->src_file_id >= files.size()))
      mxerror("There is no file with the ID '%lld'. The argument for "
              "'--append-to' was invalid.\n", amap->src_file_id);

    // 2. Is the "source" file in "append mode", meaning does its file name
    // start with a '+'?
    src_file = files.begin() + amap->src_file_id;
    if (!src_file->appending)
      mxerror("The file no. %lld ('%s') is not being appended. The argument "
              "for '--append-to' was invalid.\n", amap->src_file_id,
              src_file->name.c_str());

    // 3. Is there a file with the dst_file_id?
    if ((amap->dst_file_id < 0) || (amap->dst_file_id >= files.size()))
      mxerror("There is no file with the ID '%lld'. The argument for "
              "'--append-to' was invalid.\n", amap->dst_file_id);

    // 4. Files cannot be appended to itself.
    if (amap->src_file_id == amap->dst_file_id)
      mxerror("Files cannot be appended to themselves. The argument for "
              "'--append-to' was invalid.\n");
  }

  // Now let's check each appended file if there are NO append to mappings
  // available (in which case we fill in default ones) or if there are fewer
  // mappings than tracks that are to be copied (which is an error).
  foreach(src_file, files) {
    file_id = distance(files.begin(), src_file);
    if (!src_file->appending)
      continue;

    count = 0;
    foreach(amap, append_mapping)
      if (amap->src_file_id == file_id)
        count++;

    if ((count > 0) && (count < src_file->reader->used_track_ids.size()))
      mxerror("Only partial append mappings were given for the file no. %d "
              "('%s'). Either don't specify any mapping (in which case the "
              "default mapping will be used) or specify a mapping for all "
              "tracks that are to be copied.\n", file_id,
              src_file->name.c_str());
    else if (count == 0) {
      string missing_mappings;

      // Default mapping.
      foreach(id, src_file->reader->used_track_ids) {
        append_spec_t new_amap;

        new_amap.src_file_id = file_id;
        new_amap.src_track_id = *id;
        new_amap.dst_file_id = file_id - 1;
        new_amap.dst_track_id = *id;
        append_mapping.push_back(new_amap);
        if (missing_mappings.size() > 0)
          missing_mappings += ",";
        missing_mappings +=
          mxsprintf("%lld:%lld:%lld:%lld",
                    new_amap.src_file_id, new_amap.src_track_id,
                    new_amap.dst_file_id, new_amap.dst_track_id);
      }
      mxinfo("No append mapping was given for the file no. %d ('%s'). "
             "A default mapping of %s will be used instead. Please keep "
             "that in mind if mkvmerge aborts complaining about invalid "
             "'--append-to' options.\n", file_id, src_file->name.c_str(),
             missing_mappings.c_str());
    }
  }

  // Some more checks.
  foreach(amap, append_mapping) {
    src_file = files.begin() + amap->src_file_id;
    dst_file = files.begin() + amap->dst_file_id;

    // 5. Does the "source" file have a track with the src_track_id, and is
    // that track selected for copying?
    if (!mxfind2(id, amap->src_track_id, src_file->reader->used_track_ids))
      mxerror("The file no. %lld ('%s') does not contain a track with the "
              "ID %lld, or that track is not to be copied. The argument "
              "for '--append-to' was invalid.\n", amap->src_file_id,
              src_file->name.c_str(), amap->src_track_id);

    // 6. Does the "destination" file have a track with the dst_track_id, and
    // that track selected for copying?
    if (!mxfind2(id, amap->dst_track_id, dst_file->reader->used_track_ids))
      mxerror("The file no. %lld ('%s') does not contain a track with the "
              "ID %lld, or that track is not to be copied. Therefore no track "
              "can be appended to it. The argument for '--append-to' was "
              "invalid.\n", amap->dst_file_id, dst_file->name.c_str(),
              amap->dst_track_id);

    // 7. Is this track already mapped to somewhere else?
    foreach(cmp_amap, append_mapping) {
      if (cmp_amap == amap)
        continue;
      if (((*cmp_amap).src_file_id == amap->src_file_id) &&
          ((*cmp_amap).src_track_id == amap->src_track_id))
        mxerror("The track %lld from file no. %lld ('%s') is to be appended "
                "more than once. The argument for '--append-to' was "
                "invalid.\n", amap->src_track_id, amap->src_file_id,
                src_file->name.c_str());
    }

    // 8. Is there another track that is being appended to the dst_track_id?
    foreach(cmp_amap, append_mapping) {
      if (cmp_amap == amap)
        continue;
      if (((*cmp_amap).dst_file_id == amap->dst_file_id) &&
          ((*cmp_amap).dst_track_id == amap->dst_track_id))
        mxerror("More than one track is to be appended to the track %lld "
                "from file no. %lld ('%s'). The argument for '--append-to' "
                "was invalid.\n", amap->dst_track_id, amap->dst_file_id,
                dst_file->name.c_str());
    }
  }

  // Finally see if the packetizers can be connected and connect them if they
  // can.
  foreach(amap, append_mapping) {
    vector<generic_packetizer_c *>::const_iterator gptzr;
    generic_packetizer_c *src_ptzr, *dst_ptzr;
    int result;

    src_file = files.begin() + amap->src_file_id;
    src_ptzr = NULL;
    foreach(gptzr, src_file->reader->reader_packetizers)
      if ((*gptzr)->ti->id == amap->src_track_id)
        src_ptzr = (*gptzr);

    dst_file = files.begin() + amap->dst_file_id;
    dst_ptzr = NULL;
    foreach(gptzr, dst_file->reader->reader_packetizers)
      if ((*gptzr)->ti->id == amap->dst_track_id)
        dst_ptzr = (*gptzr);

    if ((src_ptzr == NULL) || (dst_ptzr == NULL))
      die("((src_ptzr == NULL) || (dst_ptzr == NULL)). %s\n", BUGMSG);

    // And now try to connect the packetizers.
    result = src_ptzr->can_connect_to(dst_ptzr);
    if (result != CAN_CONNECT_YES)
      mxerror("The track number %lld from the file '%s' cannot be appended "
              "to the track number %lld from the file '%s' because %s.\n",
              amap->src_track_id, files[amap->src_file_id].name.c_str(),
              amap->dst_track_id, files[amap->dst_file_id].name.c_str(),
              result == CAN_CONNECT_NO_FORMAT ? "the formats do not match" :
              result == CAN_CONNECT_NO_PARAMETERS ?
              "the stream parameters do not match" :
              "of unknown reasons");
    src_ptzr->connect(dst_ptzr);
    dst_file->appended_to = true;
  }

  // Calculate the "longest path" -- meaning the maximum number of
  // concatenated files. This is needed for displaying the progress.
  foreach(amap, append_mapping) {
    int path_length;

    // Is this the first in a chain?
    foreach(cmp_amap, append_mapping) {
      if (amap == cmp_amap)
        continue;
      if ((amap->dst_file_id == cmp_amap->src_file_id) &&
          (amap->dst_track_id == cmp_amap->src_track_id))
        break;
    }
    if (cmp_amap != append_mapping.end())
      continue;

    // Find consecutive mappings.
    trav_amap = amap;
    path_length = 2;
    do {
      foreach(cmp_amap, append_mapping)
        if ((trav_amap->src_file_id == cmp_amap->dst_file_id) &&
            (trav_amap->src_track_id == cmp_amap->dst_track_id)) {
          trav_amap = cmp_amap;
          path_length++;
          break;
        }
    } while (cmp_amap != append_mapping.end());

    if (path_length > display_path_length)
      display_path_length = path_length;
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
  vector<filelist_t>::iterator file;
  KaxChapters *chapters;

  // Step 1: Add all chapters from files that are not being appended.
  foreach(file, files) {
    if (file->appending)
      continue;
    chapters = file->reader->chapters;
    if (chapters == NULL)
      continue;

    if (kax_chapters == NULL)
      kax_chapters = new KaxChapters;

    move_chapters_by_edition(*kax_chapters, *chapters);
    delete chapters;
    file->reader->chapters = NULL;
  }

  // Step 2: Fix the mandatory elements and count the size of all chapters.
  max_chapter_size = 0;
  if (kax_chapters != NULL) {
    fix_mandatory_chapter_elements(kax_chapters);
    kax_chapters->UpdateSize(true);
    max_chapter_size += kax_chapters->ElementSize();
  }

  foreach(file, files) {
    chapters = file->reader->chapters;
    if (chapters == NULL)
      continue;

    fix_mandatory_chapter_elements(chapters);
    chapters->UpdateSize(true);
    max_chapter_size += chapters->ElementSize();
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
  vector<filelist_t>::iterator file;

  foreach(file, files) {
    try {
      switch (file->type) {
        case FILE_TYPE_MATROSKA:
          file->reader = new kax_reader_c(file->ti);
          break;
        case FILE_TYPE_OGM:
          file->reader = new ogm_reader_c(file->ti);
          break;
        case FILE_TYPE_AVI:
          file->reader = new avi_reader_c(file->ti);
          break;
        case FILE_TYPE_REAL:
          file->reader = new real_reader_c(file->ti);
          break;
        case FILE_TYPE_WAV:
          file->reader = new wav_reader_c(file->ti);
          break;
        case FILE_TYPE_SRT:
          file->reader = new srt_reader_c(file->ti);
          break;
        case FILE_TYPE_MP3:
          file->reader = new mp3_reader_c(file->ti);
          break;
        case FILE_TYPE_AC3:
          file->reader = new ac3_reader_c(file->ti);
          break;
        case FILE_TYPE_DTS:
          file->reader = new dts_reader_c(file->ti);
          break;
        case FILE_TYPE_AAC:
          file->reader = new aac_reader_c(file->ti);
          break;
        case FILE_TYPE_SSA:
          file->reader = new ssa_reader_c(file->ti);
          break;
        case FILE_TYPE_VOBSUB:
          file->reader = new vobsub_reader_c(file->ti);
          break;
        case FILE_TYPE_QTMP4:
          file->reader = new qtmp4_reader_c(file->ti);
          break;
#if defined(HAVE_FLAC_FORMAT_H)
        case FILE_TYPE_FLAC:
          file->reader = new flac_reader_c(file->ti);
          break;
#endif
        case FILE_TYPE_TTA:
          file->reader = new tta_reader_c(file->ti);
          break;
        case FILE_TYPE_MPEG_ES:
          file->reader = new mpeg_es_reader_c(file->ti);
          break;
        case FILE_TYPE_MPEG_PS:
          file->reader = new mpeg_ps_reader_c(file->ti);
          break;
        case FILE_TYPE_VOBBTN:
          file->reader = new vobbtn_reader_c(file->ti);
          break;
        case FILE_TYPE_WAVPACK4:
          file->reader = new wavpack_reader_c(file->ti);
          break;
        default:
          mxerror(_("EVIL internal bug! (unknown file type). %s\n"), BUGMSG);
          break;
      }
    } catch (error_c error) {
      mxerror(_("The demultiplexer failed to initialize:\n%s\n)"),
              error.get_error());
    }
  }

  if (!identifying) {
    vector<attachment_t>::const_iterator att;

    // Create the packetizers.
    foreach(file, files) {
      file->reader->appending = file->appending;
      file->reader->create_packetizers();
    }
    // Check if all track IDs given on the command line are actually
    // present.
    foreach(file, files) {
      file->reader->check_track_ids_and_packetizers();
      file->num_unfinished_packetizers =
        file->reader->reader_packetizers.size();
      file->old_num_unfinished_packetizers = file->num_unfinished_packetizers;
    }

    // Check if the append mappings are ok.
    check_append_mapping();

    // Calculate the size of all attachments for split control.
    foreach(att, attachments) {
      attachment_sizes_first += att->size;
      if (att->to_all_files)
        attachment_sizes_others += att->size;
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
string
create_output_name() {
  int p, p2, i;
  bool ok;
  char buffer[20];
  string s;

  s = outfile;
  p2 = 0;
  // First possibility: %d
  p = s.find("%d");
  if (p >= 0) {
    mxprints(buffer, "%d", file_num);
    s.replace(p, 2, buffer);

    return s;
  }

  // Now search for something like %02d
  ok = false;
  p = s.find("%");
  if (p >= 0) {
    p2 = s.find("d", p + 1);
    if (p2 >= 0)
      for (i = p + 1; i < p2; i++)
        if (!isdigit(s[i]))
          break;
      ok = true;
  }

  if (ok) {                     // We've found e.g. %02d
    string format(&s.c_str()[p]), len = format;
    format.erase(p2 - p + 1);
    len.erase(0, 1);
    len.erase(p2 - p - 1);
    char *buffer2 = new char[strtol(len.c_str(), NULL, 10) + 1];
    mxprints(buffer2, format.c_str(), file_num);
    s.replace(p, format.size(), buffer2);
    delete [] buffer2;

    return s;
  }

  mxprints(buffer, "-%03d", file_num);

  // See if we can find a '.'.
  p = s.rfind(".");
  if (p >= 0)
    s.insert(p, buffer);
  else
    s.append(buffer);

  return s;
}

void
add_tags_from_cue_chapters() {
  int i;
  uint32_t tuid;
  bool found;

  if ((tags_from_cue_chapters == NULL) || (ptzrs_in_header_order.size() == 0))
    return;

  found = false;
  tuid = 0;
  for (i = 0; i < ptzrs_in_header_order.size(); i++)
    if (ptzrs_in_header_order[i]->get_track_type() == 'v') {
      found = true;
      tuid = ptzrs_in_header_order[i]->get_uid();
      break;
    }
  if (!found) {
    for (i = 0; i < ptzrs_in_header_order.size(); i++)
      if (ptzrs_in_header_order[i]->get_track_type() == 'a') {
        found = true;
        tuid = ptzrs_in_header_order[i]->get_uid();
        break;
      }
  }
  if (!found)
    tuid = ptzrs_in_header_order[0]->get_uid();

  for (i = 0; i < tags_from_cue_chapters->ListSize(); i++) {
    KaxTagTargets *targets;

    targets = &GetChild<KaxTagTargets>
      (*static_cast<KaxTag *>((*tags_from_cue_chapters)[i]));
    *static_cast<EbmlUInteger *>(&GetChild<KaxTagTrackUID>(*targets)) = tuid;
  }

  if (kax_tags == NULL)
    kax_tags = tags_from_cue_chapters;
  else {
    while (tags_from_cue_chapters->ListSize() > 0) {
      kax_tags->PushElement(*(*tags_from_cue_chapters)[0]);
      tags_from_cue_chapters->Remove(0);
    }
    delete tags_from_cue_chapters;
  }
  tags_from_cue_chapters = NULL;
}

/** \brief Creates the next output file

   Creates a new file name depending on the split settings. Opens that
   file for writing and calls \c render_headers(). Also renders
   attachments if they exist and the chapters if no splitting is used.
*/
void
create_next_output_file() {
  string this_outfile;

  kax_segment = new KaxSegment();
  kax_cues = new KaxCues();
  kax_cues->SetGlobalTimecodeScale((int64_t)timecode_scale);

  if (splitting)
    this_outfile = create_output_name();
  else
    this_outfile = outfile;

  // Open the output file.
  try {
    out = new mm_file_io_c(this_outfile, MODE_CREATE);
  } catch (exception &ex) {
    mxerror(_("The output file '%s' could not be opened for writing (%s).\n"),
            this_outfile.c_str(), strerror(errno));
  }
  if (verbose)
    mxinfo(_("The file '%s\' has been opened for writing.\n"),
           this_outfile.c_str());

  cluster_helper->set_output(out);
  render_headers(out);
  render_attachments(out);

  if (max_chapter_size > 0) {
    kax_chapters_void = new EbmlVoid;
    kax_chapters_void->SetSize(max_chapter_size + 100);
    kax_chapters_void->Render(*out);
  }

  add_tags_from_cue_chapters();

  if (kax_tags != NULL) {
    fix_mandatory_tag_elements(kax_tags);
    sort_ebml_master(kax_tags);
    if (!kax_tags->CheckMandatory())
      mxerror(_("Some tag elements are missing (this error "
                "should not have occured - another similar error should have "
                "occured earlier). %s\n"), BUGMSG);

    kax_tags->UpdateSize();
    tags_size = kax_tags->ElementSize();
  }

  file_num++;
}

/** \brief Finishes and closes the current file

   Renders the data that is generated during the muxing run. The cues
   and meta seek information are rendered at the end. If splitting is
   active the chapters are stripped to those that actually lie in this
   file and rendered at the front.  The segment duration and the
   segment size are set to their actual values.
*/
void
finish_file(bool last_file) {
  int i;
  KaxChapters *chapters_here;
  KaxTags *tags_here;
  int64_t start, end, offset;

  mxinfo("\n");

  // Render the cues.
  if (write_cues && cue_writing_requested) {
    if (verbose >= 1)
      mxinfo(_("The cue entries (the index) are being written..."));
    kax_cues->Render(*out);
    if (verbose >= 1)
      mxinfo("\n");
  }

  // Now re-render the kax_duration and fill in the biggest timecode
  // as the file's duration.
  out->save_pos(kax_duration->GetElementPosition());
  mxverb(3, "mkvmerge: kax_duration: gdur %lld tcs %f du %lld\n",
         cluster_helper->get_duration(), timecode_scale,
         irnd((double)cluster_helper->get_duration() /
              (double)((int64_t)timecode_scale)));
  *(static_cast<EbmlFloat *>(kax_duration)) =
    irnd((double)cluster_helper->get_duration() /
         (double)((int64_t)timecode_scale));
  kax_duration->Render(*out);

  // If splitting is active and this is the last part then remove the
  // 'next segment UID' and re-render the kax_infos.
  if (splitting && last_file) {
    EbmlVoid *void_after_infos;
    int64_t void_size;
    bool changed;

    void_size = kax_infos->ElementSize();
    for (i = 0, changed = false; i < kax_infos->ListSize(); i++)
      if (EbmlId(*(*kax_infos)[i]) == KaxNextUID::ClassInfos.GlobalId) {
        delete (*kax_infos)[i];
        kax_infos->Remove(i);
        changed = true;
        break;
      }

    if (changed) {
      out->setFilePointer(kax_infos->GetElementPosition());
      kax_infos->UpdateSize();
      void_size -= kax_infos->ElementSize();
      kax_infos->Render(*out);
      mxverb(2, "splitting: removed nextUID; void size: %lld\n", void_size);
      void_after_infos = new EbmlVoid();
      void_after_infos->SetSize(void_size);
      void_after_infos->UpdateSize();
      void_after_infos->SetSize(void_size - void_after_infos->HeadSize());
      void_after_infos->Render(*out);
      delete void_after_infos;
    }
  }
  out->restore_pos();

  // Select the chapters that lie in this file and render them in the space
  // that was resesrved at the beginning.
  if (kax_chapters != NULL) {
    if (no_linking)
      offset = cluster_helper->get_first_timecode_in_file();
    else
      offset = 0;
    start = cluster_helper->get_first_timecode_in_file();
    end = start + cluster_helper->get_duration();

    chapters_here = copy_chapters(kax_chapters);
    if (splitting)
      chapters_here = select_chapters_in_timeframe(chapters_here, start, end,
                                                   offset);
    if (chapters_here != NULL) {
      sort_ebml_master(chapters_here);
      kax_chapters_void->ReplaceWith(*chapters_here, *out, true, true);
      chapters_in_this_file =
        static_cast<KaxChapters *>(chapters_here->Clone());
    }
    delete kax_chapters_void;
    kax_chapters_void = NULL;
  } else
    chapters_here = NULL;

  // Render the meta seek information with the cues
  if (write_meta_seek_for_clusters && (kax_sh_cues->ListSize() > 0) &&
      !hack_engaged(ENGAGE_NO_META_SEEK)) {
    kax_sh_cues->UpdateSize();
    kax_sh_cues->Render(*out);
    kax_sh_main->IndexThis(*kax_sh_cues, *kax_segment);
  }

  // Render the tags if we have some.
  if (kax_tags != NULL) {
    if (chapters_in_this_file == NULL) {
      KaxChapters temp_chapters;
      tags_here = select_tags_for_chapters(*kax_tags, temp_chapters);
   } else
      tags_here = select_tags_for_chapters(*kax_tags, *chapters_in_this_file);
    if (tags_here != NULL) {
      fix_mandatory_tag_elements(tags_here);
      tags_here->UpdateSize();
      tags_here->Render(*out, true);
    }
  } else
    tags_here = NULL;

  // Write meta seek information if it is not disabled.
  if (cue_writing_requested)
    kax_sh_main->IndexThis(*kax_cues, *kax_segment);

  if (tags_here != NULL) {
    kax_sh_main->IndexThis(*tags_here, *kax_segment);
    delete tags_here;
  }
  if (chapters_in_this_file != NULL) {
    delete chapters_in_this_file;
    chapters_in_this_file = NULL;
  }

  if (chapters_here != NULL) {
    if (!hack_engaged(ENGAGE_NO_CHAPTERS_IN_META_SEEK))
      kax_sh_main->IndexThis(*chapters_here, *kax_segment);
    delete chapters_here;
  } else if (!splitting && (kax_chapters != NULL))
    if (!hack_engaged(ENGAGE_NO_CHAPTERS_IN_META_SEEK))
      kax_sh_main->IndexThis(*kax_chapters, *kax_segment);

  if (kax_as != NULL) {
    kax_sh_main->IndexThis(*kax_as, *kax_segment);
    delete kax_as;
    kax_as = NULL;
  }

  if ((kax_sh_main->ListSize() > 0) && !hack_engaged(ENGAGE_NO_META_SEEK)) {
    kax_sh_main->UpdateSize();
    if (kax_sh_void->ReplaceWith(*kax_sh_main, *out, true) == 0)
      mxwarn(_("This should REALLY not have happened. The space reserved for "
               "the first meta seek element was too small. Size needed: %lld. "
               "%s\n"), kax_sh_main->ElementSize(), BUGMSG);
  }

  // Set the correct size for the segment.
  if (kax_segment->ForceSize(out->getFilePointer() -
                             kax_segment->GetElementPosition() -
                             kax_segment->HeadSize()))
    kax_segment->OverwriteHead(*out);

  delete out;
  delete kax_segment;
  delete kax_cues;
  delete kax_sh_void;
  delete kax_sh_main;
  delete void_after_track_headers;
  if (kax_sh_cues != NULL)
    delete kax_sh_cues;

  for (i = 0; i < packetizers.size(); i++)
    packetizers[i].packetizer->reset();
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
  vector<generic_packetizer_c *>::const_iterator gptzr;
  filelist_t &src_file = files[amap.src_file_id];
  filelist_t &dst_file = files[amap.dst_file_id];
  generic_packetizer_c *old_packetizer;
  int64_t timecode_adjustment;

  // Find the generic_packetizer_c that we will be appending to the one
  // stored in ptzr.
  foreach(gptzr, src_file.reader->reader_packetizers)
    if (amap.src_track_id == (*gptzr)->ti->id)
      break;
  if (gptzr == src_file.reader->reader_packetizers.end())
    die("Could not find gptzr when appending. %s\n", BUGMSG);

  // If we're dealing with a subtitle track or if the appending file contains
  // chapters then we have to suck the previous file dry. See below for the
  // reason (short version: we need all max_timecode_seen values).
  if ((((*gptzr)->get_track_type() == track_subtitle) ||
       (src_file.reader->chapters != NULL)) &&
      !dst_file.done) {
    dst_file.reader->read_all();
    dst_file.num_unfinished_packetizers = 0;
    dst_file.old_num_unfinished_packetizers = 0;
    dst_file.done = true;
    establish_deferred_connections(dst_file);
  }

  if (((*gptzr)->get_track_type() == track_subtitle) &&
      (dst_file.reader->num_video_tracks == 0) &&
      (video_packetizer != NULL) && !ptzr.deferred) {
    vector<filelist_t>::iterator file;

    foreach(file, files) {
      vector<generic_packetizer_c *>::const_iterator vptzr;

      if (file->done)
        continue;

      foreach(vptzr, file->reader->reader_packetizers) {
        if ((*vptzr)->get_track_type() == track_video)
          break;
      }

      if (vptzr != file->reader->reader_packetizers.end()) {
        deferred_connection_t new_def_con;

        ptzr.deferred = true;
        new_def_con.amap = amap;
        new_def_con.ptzr = &ptzr;
        file->deferred_connections.push_back(new_def_con);
        return;
      }
    }
  }

  mxinfo("Appending track %lld from file no. %lld ('%s') to track %lld from "
         "file no. %lld ('%s').\n",
         (*gptzr)->ti->id, amap.src_file_id, (*gptzr)->ti->fname.c_str(),
         ptzr.packetizer->ti->id, amap.dst_file_id,
         ptzr.packetizer->ti->fname.c_str());

  // Is the current file currently used for displaying the progress? If yes
  // then replace it with the next one.
  if (display_reader == dst_file.reader) {
    display_files_done++;
    display_reader = src_file.reader;
  }

  // Also fix the ptzr structure and reset the ptzr's state to "I want more".
  old_packetizer = ptzr.packetizer;
  ptzr.packetizer = *gptzr;
  ptzr.file = amap.src_file_id;
  ptzr.status = FILE_STATUS_MOREDATA;

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
  timecode_adjustment = dst_file.reader->max_timecode_seen;
  if (ptzr.deferred && (deferred_file != NULL))
    timecode_adjustment = deferred_file->reader->max_timecode_seen;
  else if ((ptzr.packetizer->get_track_type() == track_subtitle) ||
             (src_file.reader->chapters != NULL)) {
    vector<append_spec_t>::const_iterator cmp_amap;

    if (src_file.reader->ptzr_first_packet == NULL)
      ptzr.status = ptzr.packetizer->read();
    if (src_file.reader->ptzr_first_packet != NULL) {
      foreach(cmp_amap, append_mapping)
        if ((cmp_amap->src_file_id == amap.src_file_id) &&
            (cmp_amap->src_track_id ==
             src_file.reader->ptzr_first_packet->ti->id) &&
            (cmp_amap->dst_file_id == amap.dst_file_id))
          break;
      if (cmp_amap != append_mapping.end()) {
        foreach(gptzr, dst_file.reader->reader_packetizers)
          if ((*gptzr)->ti->id == cmp_amap->dst_track_id) {
            timecode_adjustment = (*gptzr)->max_timecode_seen;
            break;
          }
      }
    }
  }

  if (ptzr.packetizer->get_track_type() == track_subtitle) {
    mxverb(2, "append_track: new timecode_adjustment for subtitle track: "
           "%lld for %lld\n", timecode_adjustment, ptzr.packetizer->ti->id);
    // The actual connection.
    ptzr.packetizer->connect(old_packetizer, timecode_adjustment);
  } else {
    mxverb(2, "append_track: new timecode_adjustment for NON subtitle track: "
           "%lld for %lld\n", timecode_adjustment, ptzr.packetizer->ti->id);
    // The actual connection.
    ptzr.packetizer->connect(old_packetizer);
  }

  // Append some more chapters and adjust their timecodes by the highest
  // timecode seen in the previous file/the track that we've been searching
  // for above.
  if (src_file.reader->chapters != NULL) {
    if (kax_chapters == NULL)
      kax_chapters = new KaxChapters;
    adjust_chapter_timecodes(*src_file.reader->chapters, timecode_adjustment);
    move_chapters_by_edition(*kax_chapters, *src_file.reader->chapters);
    delete src_file.reader->chapters;
    src_file.reader->chapters = NULL;
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
  vector<packetizer_t>::iterator ptzr;
  vector<append_spec_t>::const_iterator amap;
  bool appended_a_track;

  appended_a_track = false;
  foreach(ptzr, packetizers) {
    if (ptzr->deferred)
      continue;
    if (!files[ptzr->orig_file].appended_to)
      continue;
    if ((ptzr->status == FILE_STATUS_MOREDATA) ||
        (ptzr->status == FILE_STATUS_HOLDING))
      continue;
    foreach(amap, append_mapping)
      if ((amap->dst_file_id == ptzr->file) &&
          (amap->dst_track_id == ptzr->packetizer->ti->id))
        break;
    if (amap == append_mapping.end())
      continue;

    append_track(*ptzr, *amap);
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
  vector<deferred_connection_t> def_cons;
  vector<deferred_connection_t>::iterator def_con;

  def_cons = file.deferred_connections;
  file.deferred_connections.clear();

  foreach(def_con, def_cons)
    append_track(*def_con->ptzr, def_con->amap, &file);

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
    vector<packetizer_t>::iterator ptzr, winner;
    packet_t *pack;
    bool appended_a_track;

    // Step 1: Make sure a packet is available for each output
    // as long we haven't already processed the last one.
    foreach(ptzr, packetizers) {
      if (ptzr->status == FILE_STATUS_HOLDING)
        ptzr->status = FILE_STATUS_MOREDATA;
      ptzr->old_status = ptzr->status;
      while ((ptzr->pack == NULL) && (ptzr->status == FILE_STATUS_MOREDATA) &&
             (ptzr->packetizer->packet_available() < 1))
        ptzr->status = ptzr->packetizer->read();
      if ((ptzr->status != FILE_STATUS_MOREDATA) &&
          (ptzr->old_status == FILE_STATUS_MOREDATA))
        ptzr->packetizer->force_duration_on_last_packet();
      if (ptzr->pack == NULL)
        ptzr->pack = ptzr->packetizer->get_packet();

      // Has this packetizer changed its status from "data available" to
      // "file done" during this loop? If so then decrease the number of
      // unfinished packetizers in the corresponding file structure.
      if ((ptzr->status == FILE_STATUS_DONE) &&
          (ptzr->old_status != ptzr->status)) {
        filelist_t &file = files[ptzr->file];

        file.num_unfinished_packetizers--;
        // If all packetizers for a file have finished then establish the
        // deferred connections.
        if ((file.num_unfinished_packetizers <= 0) &&
            (file.old_num_unfinished_packetizers > 0))
          establish_deferred_connections(file);
        file.old_num_unfinished_packetizers = file.num_unfinished_packetizers;
      }
    }

    // Step 2: Pick the packet with the lowest timecode and
    // stuff it into the Matroska file.
    winner = packetizers.end();
    foreach(ptzr, packetizers) {
      if (ptzr->pack != NULL) {
        if ((winner == packetizers.end()) || (winner->pack == NULL))
          winner = ptzr;
        else if (ptzr->pack &&
                 (ptzr->pack->assigned_timecode <
                  winner->pack->assigned_timecode))
          winner = ptzr;
      }
    }

    // Append the next track if appending is wanted.
    appended_a_track = append_tracks_maybe();

    if ((winner != packetizers.end()) && (winner->pack != NULL)) {
      pack = winner->pack;

      // Step 3: Add the winning packet to a cluster. Full clusters will be
      // rendered automatically.
      cluster_helper->add_packet(pack);

      winner->pack = NULL;

      // display some progress information
      if (verbose >= 1)
        display_progress();
    } else if (!appended_a_track) // exit if there are no more packets
      break;
  }

  // Render all remaining packets (if there are any).
  if ((cluster_helper != NULL) && (cluster_helper->get_packet_count() > 0))
    cluster_helper->render();

  if (verbose >= 1)
    mxinfo("progress: 100%%\r");
}

/** \brief Global program initialization

   Both platform dependant and independant initialization is done here.
   For Unix like systems a signal handler is installed. The locale's
   \c LC_MESSAGES is set.
*/
void
setup() {
#if ! defined(SYS_WINDOWS) && defined(HAVE_LIBINTL_H)
  if (setlocale(LC_MESSAGES, "") == NULL)
    mxerror("The locale could not be set properly. Check the "
            "LANG, LC_ALL and LC_MESSAGES environment variables.\n");
#endif

#if defined(SYS_UNIX) || defined(COMP_CYGWIN) || defined(SYS_APPLE)
  signal(SIGUSR1, sighandler);
  signal(SIGINT, sighandler);
#endif

  srand(time(NULL));
  cc_local_utf8 = utf8_init("");
  init_cc_stdio();

  cluster_helper = new cluster_helper_c();

  xml_element_map_init();
}

/** \brief Deletes the file readers
*/
static void
destroy_readers() {
  vector<filelist_t>::const_iterator file;

  foreach(file, files)
    if ((*file).reader != NULL)
      delete (*file).reader;

  packetizers.clear();
}

/** \brief Uninitialization

   Frees memory and shuts down the readers.
*/
void
cleanup() {
  delete cluster_helper;
  vector<filelist_t>::const_iterator file;

  destroy_readers();

  foreach(file, files)
    delete (*file).ti;
  files.clear();

  attachments.clear();

  if (kax_tags != NULL)
    delete kax_tags;
  if (tags_from_cue_chapters != NULL)
    delete tags_from_cue_chapters;
  if (kax_chapters != NULL)
    delete kax_chapters;
  if (kax_as != NULL)
    delete kax_as;

  utf8_done();
}

