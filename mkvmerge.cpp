/*
  mkvmerge -- utility for splicing together matroska files
      from component media subtypes

  mkvmerge.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief command line parameter parsing, looping, output handling
    \author Moritz Bunkus <moritz@bunkus.org>
*/

#include <errno.h>
#include <ctype.h>
#include <signal.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef WIN32
#include <unistd.h>
#endif

#include <iostream>
#include <string>
#include <vector>

#if __GNUC__ == 2
#include <typeinfo>
#endif

#include "EbmlHead.h"
#include "EbmlSubHead.h"
#include "EbmlVersion.h"
#include "EbmlVoid.h"

#include "FileKax.h"
#include "KaxBlock.h"
#include "KaxCluster.h"
#include "KaxClusterData.h"
#include "KaxCues.h"
#include "KaxInfo.h"
#include "KaxInfoData.h"
#include "KaxSeekHead.h"
#include "KaxSegment.h"
#include "KaxTags.h"
#include "KaxTracks.h"
#include "KaxTrackEntryData.h"
#include "KaxTrackAudio.h"
#include "KaxTrackVideo.h"
#include "KaxVersion.h"

#include "mkvmerge.h"
#include "mm_io.h"
#include "cluster_helper.h"
#include "common.h"
#include "iso639.h"
#include "r_aac.h"
#include "r_ac3.h"
#include "r_dts.h"
#include "r_avi.h"
#include "r_mp3.h"
#include "r_wav.h"
#ifdef HAVE_OGGVORBIS
#include "r_ogm.h"
#endif
#include "r_srt.h"
#include "r_ssa.h"
#include "r_matroska.h"
#include "r_mp4.h"

using namespace LIBMATROSKA_NAMESPACE;
using namespace std;

typedef struct {
  char *ext;
  int   type;
  char *desc;
} file_type_t;

typedef struct filelist_tag {
  char *name;
  mm_io_c *fp;

  int type, status;

  packet_t *pack;

  generic_reader_c *reader;

  track_info_t *ti;
} filelist_t;

typedef struct {
  int status;
  packet_t *pack;
  generic_packetizer_c *packetizer;
} packetizer_t;

vector<packetizer_t *> packetizers;
vector<filelist_t *> files;

// Variables set by the command line parser.
char *outfile = NULL;
int64_t file_sizes = 0;
int max_blocks_per_cluster = 65535;
int max_ms_per_cluster = 1000;
bool write_cues = true, cue_writing_requested = false;
bool video_track_present = false;
bool write_meta_seek = true;
int64_t meta_seek_size = 0;
bool no_lacing = false, no_linking = false;
int64_t split_after = -1;
bool split_by_time = false;
int split_max_num_files = 65535;

float video_fps = -1.0;
int default_tracks[3];

cluster_helper_c *cluster_helper = NULL;
KaxSegment *kax_segment;
KaxInfo *kax_infos;
KaxTracks *kax_tracks;
KaxTrackEntry *kax_last_entry;
KaxCues *kax_cues;
EbmlVoid *kax_seekhead_void = NULL;
KaxDuration *kax_duration;
KaxSeekHead *kax_seekhead = NULL;

// Actual pass. 0 for normal operation. 1 for first pass of splitting
// (only find split points, output goes to /dev/null). 2 for the second
// pass of the splitting run.
int pass = 0;
int file_num = 1;

// Specs say that track numbers should start at 1.
int track_number = 1;

mm_io_c *out;

bitvalue_c seguid_prev(128), seguid_current(128), seguid_next(128);
bitvalue_c *seguid_link_previous = NULL, *seguid_link_next = NULL;

file_type_t file_types[] =
  {{"---", TYPEUNKNOWN, "<unknown>"},
   {"demultiplexers:", -1, ""},
   {"mkv", TYPEMATROSKA, "general Matroska files"},
#ifdef HAVE_OGGVORBIS
   {"ogg", TYPEOGM, "general OGG media stream, audio/video embedded in OGG"},
#endif // HAVE_OGGVORBIS
   {"avi", TYPEAVI, "AVI (Audio/Video Interleaved)"},
   {"wav", TYPEWAV, "WAVE (uncompressed PCM)"},
   {"srt", TYPESRT, "SRT text subtitles"},
   {"ssa", TYPESSA, "SSA/ASS text subtitles"},
//    {"   ", TYPEMICRODVD, "MicroDVD text subtitles"},
//    {"idx", TYPEVOBSUB, "VobSub subtitles"},
   {"mp3", TYPEMP3, "MPEG1 layer III audio (CBR and VBR/ABR)"},
   {"ac3", TYPEAC3, "A/52 (aka AC3)"},
   {"dts", TYPEDTS, "DTS (Digital Theater System)"},
   {"aac", TYPEAAC, "AAC (Advanced Audio Coding)"},
   {"output modules:", -1, ""},
#ifdef HAVE_OGGVORBIS
   {"   ", -1,      "Vorbis audio"},
#endif // HAVE_OGGVORBIS
   {"   ", -1,      "Video (not MPEG1/2)"},
   {"   ", -1,      "uncompressed PCM audio"},
   {"   ", -1,      "simple text subtitles"},
//    {"   ", -1,      "VobSub subtitles"},
   {"   ", -1,      "MP3 audio"},
   {"   ", -1,      "AC3 audio"},
   {"   ", -1,      "DTS audio"},
   {"   ", -1,      "AAC audio"},
   {NULL,  -1,      NULL}};

static void usage(void) {
  fprintf(stdout,
    "mkvmerge -o out [global options] [options] <file1> [@optionsfile ...]"
    "\n\n Global options:\n"
    "  -v, --verbose            verbose status\n"
    "  -q, --quiet              suppress status output\n"
    "  -o, --output out         Write to the file 'out'.\n"
    "  --cluster-length <n[ms]> Put at most n data blocks into each cluster.\n"
    "                           If the number is postfixed with 'ms' then\n"
    "                           put at most n milliseconds of data into each\n"
    "                           cluster.\n"
    "  --no-cues                Do not write the cue data (the index).\n"
    "  --no-meta-seek           Do not write any meta seek information.\n"
    "  --meta-seek-size <d>     Reserve d bytes for the meta seek entries.\n"
    "  --no-lacing              Do not use lacing.\n"
    "  --split <d[K,M,G]|HH:MM:SS|ns>\n"
    "                           Create a new file after d bytes (KB, MB, GB)\n"
    "                           or after a specific time.\n"
    "  --split-max-files <n>    Create at most n files.\n"
    "  --dont-link              Don't link splitted files.\n"
    "  --link-to-previous <UID> Link the first file to the given UID.\n"
    "  --link-to-next <UID>     Link the last file to the given UID.\n"
    "\n Options for each input file:\n"
    "  -a, --atracks <n,m,...>  Copy audio tracks n,m etc. Default: copy all\n"
    "                           audio tracks.\n"
    "  -d, --vtracks <n,m,...>  Copy video tracks n,m etc. Default: copy all\n"
    "                           video tracks.\n"
    "  -s, --stracks <n,m,...>  Copy subtitle tracks n,m etc. Default: copy\n"
    "                           all subtitle tracks.\n"
    "  -A, --noaudio            Don't copy any audio track from this file.\n"
    "  -D, --novideo            Don't copy any video track from this file.\n"
    "  -S, --nosubs             Don't copy any text track from this file.\n"
    "  -y, --sync <TID:d[,o[/p]]>\n"
    "                           Synchronize, delay the audio track with the\n"
    "                           id TID by d ms. \n"
    "                           d > 0: Pad with silent samples.\n"
    "                           d < 0: Remove samples from the beginning.\n"
    "                           o/p: Adjust the timecodes by o/p to fix\n"
    "                           linear drifts. p defaults to 1000 if\n"
    "                           omitted. Both o and p can be floating point\n"
    "                           numbers.\n"
    "  --default-track <TID>    Sets the 'default' flag for this track.\n"
    "  --cues <TID:none|iframes|all>\n"
    "                           Create cue (index) entries for this track:\n"
    "                           None at all, only for I frames, for all.\n"
    "  --language <TID:lang>    Sets the language for the track (ISO639-2\n"
    "                           code, see --list-languages).\n"
    "\n Options that only apply to video tracks:\n"
    "  -f, --fourcc <FOURCC>    Forces the FourCC to the specified value.\n"
    "                           Works only for video tracks.\n"
    "  --aspect-ratio <f|a/b>   Sets the aspect ratio.\n"
    "\n Options that only apply to text subtitle tracks:\n"
    "  --sub-charset <charset>  Sets the charset the text subtitles are\n"
    "                           written in for the conversion to UTF-8.\n"
    "\n\n Other options:\n"
    "  -i, --identify <file>    Print information about the source file.\n"
    "  -l, --list-types         Lists supported input file types.\n"
    "  --list-languages         Lists all ISO639 languages and their\n"
    "                           ISO639-2 codes.\n"
    "  -h, --help               Show this help.\n"
    "  -V, --version            Show version information.\n"
    "  @optionsfile             Reads additional command line options from\n"
    "                           the specified file (see man page).\n"
    "\n\nPlease read the man page/the HTML documentation to mkvmerge. It\n"
    "explains several details in great length which are not obvious from\n"
    "this listing.\n"
  );
}

static void sighandler(int signum) {
#ifdef DEBUG
  if (signum == SIGUSR1)
    debug_c::dump_info();
#endif // DEBUG
}

static int get_type(char *filename) {
  mm_io_c *mm_io;
  off_t size;
  int type;

  try {
    mm_io = new mm_io_c(filename, MODE_READ);
    mm_io->setFilePointer(0, seek_end);
    size = mm_io->getFilePointer();
    mm_io->setFilePointer(0, seek_current);
  } catch (exception &ex) {
    fprintf(stderr, "Error: could not open source file or seek properly in it "
            "(%s).\n", filename);
    exit(1);
  }
  if (avi_reader_c::probe_file(mm_io, size))
    type = TYPEAVI;
  else if (mkv_reader_c::probe_file(mm_io, size))
    type = TYPEMATROSKA;
  else if (wav_reader_c::probe_file(mm_io, size))
    type = TYPEWAV;
  else if (mp4_reader_c::probe_file(mm_io, size)) {
    fprintf(stderr, "Error: MP4/Quicktime files are not supported (%s).\n",
            filename);
    exit(1);
  }
#ifdef HAVE_OGGVORBIS
  else if (ogm_reader_c::probe_file(mm_io, size))
    type = TYPEOGM;
#endif // HAVE_OGGVORBIS
  else if (srt_reader_c::probe_file(mm_io, size))
    type = TYPESRT;
  else if (mp3_reader_c::probe_file(mm_io, size))
    type = TYPEMP3;
  else if (ac3_reader_c::probe_file(mm_io, size))
    type = TYPEAC3;
  else if (dts_reader_c::probe_file(mm_io, size))
    type = TYPEDTS;
  else if (aac_reader_c::probe_file(mm_io, size))
    type = TYPEAAC;
  else if (ssa_reader_c::probe_file(mm_io, size))
    type = TYPESSA;
  else
    type = TYPEUNKNOWN;

  delete mm_io;

  file_sizes += size;

  return type;
}

static int display_counter = 1;

static void display_progress(int force) {
  filelist_t *winner;
  int i;

  if (((display_counter % 500) == 0) || force) {
    display_counter = 0;
    winner = files[0];
    for (i = 1; i < files.size(); i++)
      if (files[i]->reader->display_priority() >
          winner->reader->display_priority())
        winner = files[i];
    winner->reader->display_progress();
  }
  display_counter++;
}

void add_packetizer(generic_packetizer_c *packetizer) {
  packetizer_t *pack = (packetizer_t *)safemalloc(sizeof(packetizer_t));
  pack->packetizer = packetizer;
  pack->status = EMOREDATA;
  pack->pack = NULL;
  packetizers.push_back(pack);
}

static void parse_tracks(char *s, vector<int64_t> *tracks) {
  char *comma;
  int64_t tid;

  tracks->clear();

  comma = strchr(s, ',');
  while ((comma != NULL) && (*s != 0)) {
    *comma = 0;

    if (!parse_int(s, tid)) {
      fprintf(stderr, "Error: Invalid track ID '%s'.\n", s);
      exit(1);
    }
    tracks->push_back(tid);

    s = &comma[1];
    comma = strchr(s, ',');
  }

  if (*s != 0) {
    if (!parse_int(s, tid)) {
      fprintf(stderr, "Error: Invalid track ID '%s'.\n", s);
      exit(1);
    }
    tracks->push_back(tid);
  }
}

static void parse_sync(char *s, audio_sync_t &async) {
  char *linear, *div, *colon;
  double d1, d2;

  // Extract the track number.
  if ((colon = strchr(s, ':')) == NULL) {
    fprintf(stderr, "Error: Invalid sync option. No track ID specified (%s)."
            "\n", s);
    exit(1);
  }
  *colon = 0;
  if (!parse_int(s, async.id)) {
    fprintf(stderr, "Error: Invalid track ID specified.\n");
    exit(1);
  }
  s = &colon[1];
  if (*s == 0) {
    fprintf(stderr, "Error: Invalid sync option specified.\n");
    exit(1);
  }

  // Now parse the actual sync values.
  if ((linear = strchr(s, ',')) != NULL) {
    *linear = 0;
    linear++;
    div = strchr(linear, '/');
    if (div == NULL)
      async.linear = strtod(linear, NULL) / 1000.0;
    else {
      *div = 0;
      div++;
      d1 = strtod(linear, NULL);
      d2 = strtod(div, NULL);
      if (d2 == 0.0) {
        fprintf(stderr, "Error: linear sync: division by zero?\n");
        exit(1);
      }
      async.linear = d1 / d2;
    }
    if (async.linear <= 0.0) {
      fprintf(stderr, "Error: linear sync value may not be <= 0.\n");
      exit(1);
    }
  } else
    async.linear = 1.0;
  async.displacement = atoi(s);
}

static float parse_aspect_ratio(char *s) {
  char *div;
  float w, h;

  div = strchr(s, '/');
  if (div == NULL)
    return strtod(s, NULL);

  *div = 0;
  div++;
  if (*s == 0) {
    fprintf(stderr, "Error: aspect ratio: missing dividend.\n");
    exit(1);
  }
  if (*div == 0) {
    fprintf(stderr, "Error: aspect ratio: missing divisor.\n");
    exit(1);
  }
  w = strtod(s, NULL);
  h = strtod(div, NULL);
  if (h == 0.0) {
    fprintf(stderr, "Error: aspect ratio: divisor is 0.\n");
    exit(1);
  }

  return w/h;
}

static void parse_split(const char *arg) {
  int64_t modifier;
  char *s, *p;

  if ((arg == NULL) || (arg[0] == 0) || (arg[1] == 0)) {
    fprintf(stderr, "Error: Invalid format for --split.\n");
    exit(1);
  }

  s = safestrdup(arg);

  // HH:MM:SS
  if ((strlen(s) == 8) && (s[2] == ':') && (s[5] == ':') &&
      isdigit(s[0]) && isdigit(s[1]) && isdigit(s[3]) &&
      isdigit(s[4]) && isdigit(s[6]) && isdigit(s[7])) {
    int secs, mins, hours;

    s[2] = 0;
    s[5] = 0;
    parse_int(s, hours);
    parse_int(&s[3], mins);
    parse_int(&s[6], secs);
    split_after = secs + mins * 60 + hours * 3600;
    if ((hours < 0) || (mins < 0) || (mins > 59) ||
        (secs < 0) || (secs > 59) || (split_after < 10)) {
      fprintf(stderr, "Error: Invalid time for --split.\n");
      exit(1);
    }

    split_after *= 1000;
    split_by_time = true;
    safefree(s);
    return;
  }

  // Number of seconds: e.g. 1000s or 4254S
  if ((s[strlen(s) - 1] == 's') || (s[strlen(s) - 1] == 'S')) {
    s[strlen(s) - 1] = 0;
    if (!parse_int(s, split_after)) {
      fprintf(stderr, "Error: Invalid time for --split.\n");
      exit(1);
    }

    split_after *= 1000;
    split_by_time = true;
    safefree(s);
    return;
  }

  // Size in bytes/KB/MB/GB
  p = &s[strlen(s) - 1];
  if ((*p == 'k') || (*p == 'K'))
    modifier = 1024;
  else if ((*p == 'm') || (*p == 'M'))
    modifier = 1024 * 1024;
  else if ((*p == 'g') || (*p == 'G'))
    modifier = 1024 * 1024 * 1024;
  else
    modifier = 1;
  if (modifier != 1)
    *p = 0;
  if (!parse_int(s, split_after)) {
    fprintf(stderr, "Error: invalid split size.\n");
    exit(1);
  }
  split_after *= modifier;
  if (split_after <= (1024 * 1024)) {
    fprintf(stderr, "Error: invalid split size.\n");
    exit(1);
  }

  safefree(s);
  split_by_time = false;
}

static void parse_cues(char *s, cue_creation_t &cues) {
  char *colon;

  // Extract the track number.
  if ((colon = strchr(s, ':')) == NULL) {
    fprintf(stderr, "Error: Invalid cues option. No track ID specified (%s)."
            "\n", s);
    exit(1);
  }
  *colon = 0;
  if (!parse_int(s, cues.id)) {
    fprintf(stderr, "Error: Invalid track ID specified.\n");
    exit(1);
  }
  s = &colon[1];
  if (*s == 0) {
    fprintf(stderr, "Error: Invalid cues option specified.\n");
    exit(1);
  }

  if (!strcmp(s, "all"))
    cues.cues = CUES_ALL;
  else if (!strcmp(s, "iframes"))
    cues.cues = CUES_IFRAMES;
  else if (!strcmp(s, "none"))
    cues.cues = CUES_NONE;
  else {
    fprintf(stderr, "Error: '%s' is an unsupported argument for --cues.\n", s);
    exit(1);
  }
}

static void parse_language(char *s, language_t &lang) {
  char *colon;

  // Extract the track number.
  if ((colon = strchr(s, ':')) == NULL) {
    fprintf(stderr, "Error: Invalid language option. No track ID specified "
            "(%s).\n", s);
    exit(1);
  }
  *colon = 0;
  if (!parse_int(s, lang.id)) {
    fprintf(stderr, "Error: Invalid track ID specified.\n");
    exit(1);
  }
  s = &colon[1];
  if (*s == 0) {
    fprintf(stderr, "Error: Invalid language specified.\n");
    exit(1);
  }

  if (!is_valid_iso639_2_code(s)) {
    fprintf(stderr, "Error: '%s' is not a valid ISO639-2 code. See "
            "'mkvmerge --list-languages'.\n", s);
    exit(1);
  }
  
  lang.language = s;
}

static void render_headers(mm_io_c *out, bool last_file, bool first_file) {
  EbmlHead head;
  int i;

  try {
    EDocType &doc_type = GetChild<EDocType>(head);
    *static_cast<EbmlString *>(&doc_type) = "matroska";
    EDocTypeVersion &doc_type_ver = GetChild<EDocTypeVersion>(head);
    *(static_cast<EbmlUInteger *>(&doc_type_ver)) = 1;
    EDocTypeReadVersion &doc_type_read_ver =
      GetChild<EDocTypeReadVersion>(head);
    *(static_cast<EbmlUInteger *>(&doc_type_read_ver)) = 1;

    head.Render(*out);

    kax_infos = &GetChild<KaxInfo>(*kax_segment);
    KaxTimecodeScale &time_scale = GetChild<KaxTimecodeScale>(*kax_infos);
    *(static_cast<EbmlUInteger *>(&time_scale)) = TIMECODE_SCALE;

    kax_duration = &GetChild<KaxDuration>(*kax_infos);
    *(static_cast<EbmlFloat *>(kax_duration)) = 0.0;

    string version = string("libebml v") + EbmlCodeVersion +
      string(" + libmatroska v") + KaxCodeVersion;
    *((EbmlUnicodeString *)&GetChild<KaxMuxingApp>(*kax_infos)) =
      cstr_to_UTFstring(version.c_str());
    *((EbmlUnicodeString *)&GetChild<KaxWritingApp>(*kax_infos)) =
      cstr_to_UTFstring(VERSIONINFO);
    GetChild<KaxDateUTC>(*kax_infos).SetEpochDate(time(NULL));

    // Generate the segment UIDs.
    if (first_file) {
      seguid_current.generate_random();
      if (!last_file)
        seguid_next.generate_random();
    } else {
      seguid_prev = seguid_current;
      seguid_current = seguid_next;
      if (!last_file)
        seguid_next.generate_random();
    }

    // Set the segment UIDs.
    KaxSegmentUID &kax_seguid = GetChild<KaxSegmentUID>(*kax_infos);
    kax_seguid.CopyBuffer(seguid_current.data(), 128 / 8);

    if (!no_linking) {
      if (!first_file) {
        KaxPrevUID &kax_prevuid = GetChild<KaxPrevUID>(*kax_infos);
        kax_prevuid.CopyBuffer(seguid_prev.data(), 128 / 8);
      } else if (seguid_link_previous != NULL) {
        KaxPrevUID &kax_prevuid = GetChild<KaxPrevUID>(*kax_infos);
        kax_prevuid.CopyBuffer(seguid_link_previous->data(), 128 / 8);
      }
      if (!last_file) {
        KaxNextUID &kax_nextuid = GetChild<KaxNextUID>(*kax_infos);
        kax_nextuid.CopyBuffer(seguid_next.data(), 128 / 8);
      } else if (seguid_link_next != NULL) {
        KaxNextUID &kax_nextuid = GetChild<KaxNextUID>(*kax_infos);
        kax_nextuid.CopyBuffer(seguid_link_next->data(), 128 / 8);
      }
    }

    kax_segment->WriteHead(*out, 5);

    // Reserve some space for the meta seek stuff.
    if (write_cues && write_meta_seek) {
      kax_seekhead = new KaxSeekHead();
      kax_seekhead_void = new EbmlVoid();
      if (meta_seek_size == 0)
        if (video_track_present)
          meta_seek_size =
            (int64_t)((float)file_sizes * 1.5 / 10240.0);
        else
          meta_seek_size =
            (int64_t)((float)file_sizes * 3 / 4096.0);
      kax_seekhead_void->SetSize(meta_seek_size);
      kax_seekhead_void->Render(*out);
    }

    kax_infos->Render(*out);

    kax_tracks = &GetChild<KaxTracks>(*kax_segment);
    kax_last_entry = NULL;

    for (i = 0; i < files.size(); i++)
      files[i]->reader->set_headers();

    kax_tracks->Render(*out);
  } catch (exception &ex) {
    fprintf(stderr, "Error: Could not render the track headers.\n");
    exit(1);
  }
}

static void create_readers() {
  filelist_t *file;
  int i;

  for (i = 0; i < files.size(); i++) {
    file = files[i];
    try {
      switch (file->type) {
        case TYPEMATROSKA:
          file->reader = new mkv_reader_c(file->ti);
          break;
#ifdef HAVE_OGGVORBIS
        case TYPEOGM:
          file->reader = new ogm_reader_c(file->ti);
          break;
#endif // HAVE_OGGVORBIS
        case TYPEAVI:
          if ((file->ti->stracks->size() != 0) || file->ti->no_subs)
            fprintf(stderr, "Warning: -s/-S are ignored for AVI files.\n");
          file->reader = new avi_reader_c(file->ti);
          break;
        case TYPEWAV:
          if ((file->ti->stracks->size() != 0) || file->ti->no_subs ||
              (file->ti->atracks->size() != 0) || file->ti->no_audio ||
              (file->ti->vtracks->size() != 0) || file->ti->no_video)
            fprintf(stderr, "Warning: -a/-A/-d/-D/-s/-S are ignored for "
                    "WAVE files.\n");
          file->reader = new wav_reader_c(file->ti);
          break;
        case TYPESRT:
          if ((file->ti->stracks->size() != 0) || file->ti->no_subs ||
              (file->ti->atracks->size() != 0) || file->ti->no_audio ||
              (file->ti->vtracks->size() != 0) || file->ti->no_video)
            fprintf(stderr, "Warning: -a/-A/-d/-D/-s/-S are ignored for "
                    "SRT files.\n");
          file->reader = new srt_reader_c(file->ti);
          break;
        case TYPEMP3:
          if ((file->ti->stracks->size() != 0) || file->ti->no_subs ||
              (file->ti->atracks->size() != 0) || file->ti->no_audio ||
              (file->ti->vtracks->size() != 0) || file->ti->no_video)
            fprintf(stderr, "Warning: -a/-A/-d/-D/-s/-S are ignored for "
                    "MP3 files.\n");
          file->reader = new mp3_reader_c(file->ti);
          break;
        case TYPEAC3:
          if ((file->ti->stracks->size() != 0) || file->ti->no_subs ||
              (file->ti->atracks->size() != 0) || file->ti->no_audio ||
              (file->ti->vtracks->size() != 0) || file->ti->no_video)
            fprintf(stderr, "Warning: -a/-A/-d/-D/-s/-S are ignored for "
                    "AC3 files.\n");
          file->reader = new ac3_reader_c(file->ti);
          break;
        case TYPEDTS:
          if ((file->ti->stracks->size() != 0) || file->ti->no_subs ||
              (file->ti->atracks->size() != 0) || file->ti->no_audio ||
              (file->ti->vtracks->size() != 0) || file->ti->no_video)
            fprintf(stderr, "Warning: -a/-A/-d/-D/-s/-S are ignored for "
                    "DTS files.\n");
          file->reader = new dts_reader_c(file->ti);
          break;
        case TYPEAAC:
          if ((file->ti->stracks->size() != 0) || file->ti->no_subs ||
              (file->ti->atracks->size() != 0) || file->ti->no_audio ||
              (file->ti->vtracks->size() != 0) || file->ti->no_video)
            fprintf(stderr, "Warning: -a/-A/-d/-D/-s/-S are ignored for "
                    "AAC files.\n");
          file->reader = new aac_reader_c(file->ti);
          break;
        case TYPESSA:
          if ((file->ti->stracks->size() != 0) || file->ti->no_subs ||
              (file->ti->atracks->size() != 0) || file->ti->no_audio ||
              (file->ti->vtracks->size() != 0) || file->ti->no_video)
            fprintf(stderr, "Warning: -a/-A/-d/-D/-s/-S are ignored for "
                    "SSA/ASS files.\n");
          file->reader = new ssa_reader_c(file->ti);
          break;
        default:
          fprintf(stderr, "Error: EVIL internal bug! (unknown file type)\n");
          exit(1);
          break;
      }
    } catch (error_c error) {
      fprintf(stderr, "Error: Demultiplexer failed to initialize:\n%s\n",
              error.get_error());
      exit(1);
    }
  }
}

static void identify(const char *filename) {
  track_info_t ti;
  filelist_t *file;

  memset(&ti, 0, sizeof(track_info_t));
  ti.audio_syncs = new vector<audio_sync_t>;
  ti.cue_creations = new vector<cue_creation_t>;
  ti.default_track_flags = new vector<int64_t>;
  ti.languages = new vector<language_t>;
  ti.aspect_ratio = 1.0;
  ti.atracks = new vector<int64_t>;
  ti.vtracks = new vector<int64_t>;
  ti.stracks = new vector<int64_t>;

  file = (filelist_t *)safemalloc(sizeof(filelist_t));

  file->name = safestrdup(filename);
  file->type = get_type(file->name);
  ti.fname = file->name;
  
  if (file->type == TYPEUNKNOWN) {
    fprintf(stderr, "Error: File %s has unknown type. Please have a look "
            "at the supported file types ('mkvmerge --list-types') and "
            "contact me at moritz@bunkus.org if your file type is "
            "supported but not recognized properly.\n", file->name);
    exit(1);
  }

  file->fp = NULL;
  file->status = EMOREDATA;
  file->pack = NULL;
  file->ti = duplicate_track_info(&ti);

  files.push_back(file);

  verbose = 0;
  create_readers();

  file->reader->identify();
}

static void parse_args(int argc, char **argv) {
  track_info_t ti;
  int i, j;
  filelist_t *file;
  char *s;
  audio_sync_t async;
  cue_creation_t cues;
  int64_t id;
  language_t lang;

  memset(&ti, 0, sizeof(track_info_t));
  ti.audio_syncs = new vector<audio_sync_t>;
  ti.cue_creations = new vector<cue_creation_t>;
  ti.default_track_flags = new vector<int64_t>;
  ti.languages = new vector<language_t>;
  ti.aspect_ratio = 1.0;
  ti.atracks = new vector<int64_t>;
  ti.vtracks = new vector<int64_t>;
  ti.stracks = new vector<int64_t>;

  // Check if only information about the file is wanted. In this mode only
  // two parameters are allowed: the --identify switch and the file.
  if ((argc == 2) &&
      (!strcmp(argv[0], "-i") || !strcmp(argv[0], "--identify"))) {
    identify(argv[1]);
    exit(0);
  }

  // First parse options that either just print some infos and then exit
  // or that are needed right at the beginning.
  for (i = 0; i < argc; i++)
    if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--version")) {
      fprintf(stdout, VERSIONINFO "\n");
      exit(0);
    } else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-?") ||
               !strcmp(argv[i], "--help")) {
      usage();
      exit(0);
    } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: -o lacks a file name.\n");
        exit(1);
      } else if (outfile != NULL) {
        fprintf(stderr, "Error: only one output file allowed.\n");
        exit(1);
      }
      outfile = safestrdup(argv[i + 1]);
      i++;
    } else if (!strcmp(argv[i], "-l") || !strcmp(argv[i], "--list-types")) {
      fprintf(stdout, "Known file types:\n  ext  description\n"
              "  ---  --------------------------\n");
      for (j = 1; file_types[j].ext; j++)
        fprintf(stdout, "  %s  %s\n", file_types[j].ext, file_types[j].desc);
      exit(0);
    } else if (!strcmp(argv[i], "--list-languages")) {
      list_iso639_languages();
      exit(0);
    } else if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--identify")) {
      fprintf(stderr, "Error: --identify can only be used with a file name. "
              "No other options are allowed.\n");
      exit(1);
    }

  if (outfile == NULL) {
    fprintf(stderr, "Error: no output file given.\n\n");
    usage();
    exit(1);
  }

  for (i = 0; i < argc; i++) {

    // Ignore the options we took care of in the first step.
    if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
      i++;
      continue;
    }

    // Global options
    if (!strcmp(argv[i], "-q"))
      verbose = 0;

    else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose"))
      verbose = 2;

    else if (!strcmp(argv[i], "--split")) {
      if (((i + 1) >= argc) || (argv[i + 1][0] == 0)) {
        fprintf(stderr, "Error: --split lacks the size.\n");
        exit(1);
      }
      parse_split(argv[i + 1]);
      i++;

    } else if (!strcmp(argv[i], "--split-max-files")) {
      if (((i + 1) >= argc) || (argv[i + 1][0] == 0)) {
        fprintf(stderr, "Error: --split-max-files lacks the number of files."
                "\n");
        exit(1);
      }
      if (!parse_int(argv[i + 1], split_max_num_files) ||
          (split_max_num_files < 2)) {
        fprintf(stderr, "Error: Wrong argument to --split-max-files.\n");
        exit(1);
      }
      i++;

    } else if (!strcmp(argv[i], "--dont-link")) {
      no_linking = true;

    } else if (!strcmp(argv[i], "--link-to-previous")) {
      if (((i + 1) >= argc) || (argv[i + 1][0] == 0)) {
        fprintf(stderr, "Error: --link-to-previous lacks the previous UID."
                "\n");
        exit(1);
      }
      if (seguid_link_previous != NULL) {
        fprintf(stderr, "Error: Previous UID was already given.\n");
        exit(1);
      }
      try {
        seguid_link_previous = new bitvalue_c(argv[i + 1], 128);
      } catch (exception &ex) {
        fprintf(stderr, "Error: Unknown format for the previous UID.\n");
        exit(1);
      }
      i++;

    } else if (!strcmp(argv[i], "--link-to-next")) {
      if (((i + 1) >= argc) || (argv[i + 1][0] == 0)) {
        fprintf(stderr, "Error: --link-to-next lacks the previous UID."
                "\n");
        exit(1);
      }
      if (seguid_link_next != NULL) {
        fprintf(stderr, "Error: Next UID was already given.\n");
        exit(1);
      }
      try {
        seguid_link_next = new bitvalue_c(argv[i + 1], 128);
      } catch (exception &ex) {
        fprintf(stderr, "Error: Unknown format for the next UID.\n");
        exit(1);
      }
      i++;

    } else if (!strcmp(argv[i], "--cluster-length")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: --cluster-length lacks the length.\n");
        exit(1);
      }
      s = strstr("ms", argv[i + 1]);
      if (s != NULL) {
        *s = 0;
        if (!parse_int(argv[i + 1], max_ms_per_cluster) ||
            (max_ms_per_cluster < 0) ||
            (max_ms_per_cluster > 65535)) {
          fprintf(stderr, "Error: Cluster length out of range (0..65535).\n");
          exit(1);
        }
        max_blocks_per_cluster = 65535;
      } else {
        if (!parse_int(argv[i + 1], max_blocks_per_cluster) ||
            (max_blocks_per_cluster < 0) ||
            (max_blocks_per_cluster > 65535)) {
          fprintf(stderr, "Error: Cluster length out of range (0..65535).\n");
          exit(1);
        }
        max_ms_per_cluster = 65535;
      }
      i++;

    } else if (!strcmp(argv[i], "--meta-seek-size")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: --meta-seek-size lacks the size argument.\n");
        exit(1);
      }
      if (!parse_int(argv[i + 1], meta_seek_size) || (meta_seek_size < 1)) {
        fprintf(stderr, "Error: Invalid size given for --meta-seek-size.\n");
        exit(1);
      }
      i++;

    } else if (!strcmp(argv[i], "--no-cues"))
      write_cues = false;

    else if (!strcmp(argv[i], "--no-meta-seek"))
      write_meta_seek = false;

    else if (!strcmp(argv[i], "--no-lacing"))
      no_lacing = true;

    // Options that apply to the next input file only.
    else if (!strcmp(argv[i], "-A") || !strcmp(argv[i], "--noaudio"))
      ti.no_audio = true;

    else if (!strcmp(argv[i], "-D") || !strcmp(argv[i], "--novideo"))
      ti.no_video = true;

    else if (!strcmp(argv[i], "-S") || !strcmp(argv[i], "--nosubs"))
      ti.no_subs = true;

    else if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "--atracks")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: -a lacks the stream number(s).\n");
        exit(1);
      }
      parse_tracks(argv[i + 1], ti.atracks);
      i++;

    } else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--vtracks")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: -d lacks the stream number(s).\n");
        exit(1);
      }
      parse_tracks(argv[i + 1], ti.vtracks);
      i++;

    } else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--stracks")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: -s lacks the stream number(s).\n");
        exit(1);
      }
      parse_tracks(argv[i + 1], ti.stracks);
      i++;

    } else if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--fourcc")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: -f lacks the FourCC.\n");
        exit(1);
      }
      if (strlen(argv[i + 1]) != 4) {
        fprintf(stderr, "Error: The FourCC must be exactly four chars "
                "long.\n");
        exit(1);
      }
      memcpy(ti.fourcc, argv[i + 1], 4);
      ti.fourcc[4] = 0;
      i++;

    } else if (!strcmp(argv[i], "--aspect-ratio")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: --aspect-ratio lacks the aspect ratio.\n");
        exit(1);
      }
      ti.aspect_ratio = parse_aspect_ratio(argv[i + 1]);
      i++;

    } else if (!strcmp(argv[i], "-y") || !strcmp(argv[i], "--sync")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: -y lacks the audio delay.\n");
        exit(1);
      }
      parse_sync(argv[i + 1], async);
      ti.audio_syncs->push_back(async);
      i++;

    } else if (!strcmp(argv[i], "--cues")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: --cues lacks its argument.\n");
        exit(1);
      }
      parse_cues(argv[i + 1], cues);
      ti.cue_creations->push_back(cues);
      i++;

    } else if (!strcmp(argv[i], "--default-track")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: --default-track lacks the track ID.\n");
        exit(1);
      }
      if (!parse_int(argv[i + 1], id)) {
        fprintf(stderr, "Error: Invalid track ID specified.\n");
        exit(1);
      }
      ti.default_track_flags->push_back(id);
      i++;

    } else if (!strcmp(argv[i], "--language")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: --language lacks its argument.\n");
        exit(1);
      }
      parse_language(argv[i + 1], lang);
      ti.languages->push_back(lang);
      i++;

    } else if (!strcmp(argv[i], "--sub-charset")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: --sub-charset lacks its argument.\n");
        exit(1);
      }

      ti.sub_charset = argv[i + 1];
      i++;
    }

    // The argument is an input file.
    else {
      if ((ti.atracks->size() != 0) && ti.no_audio) {
        fprintf(stderr, "Error: -A and -a used on the same source file.\n");
        exit(1);
      }
      if ((ti.vtracks->size() != 0) && ti.no_video) {
        fprintf(stderr, "Error: -D and -d used on the same source file.\n");
        exit(1);
      }
      if ((ti.stracks->size() != 0) && ti.no_subs) {
        fprintf(stderr, "Error: -S and -s used on the same source file.\n");
        exit(1);
      }
      file = (filelist_t *)safemalloc(sizeof(filelist_t));

      file->name = argv[i];
      file->type = get_type(file->name);
      ti.fname = argv[i];

      if (file->type == TYPEUNKNOWN) {
        fprintf(stderr, "Error: File %s has unknown type. Please have a look "
                "at the supported file types ('mkvmerge --list-types') and "
                "contact me at moritz@bunkus.org if your file type is "
                "supported but not recognized properly.\n", file->name);
        exit(1);
      }

      file->fp = NULL;
      if (file->type != TYPECHAPTERS) {
        file->status = EMOREDATA;
        file->pack = NULL;
        file->ti = duplicate_track_info(&ti);

        files.push_back(file);
      } else
        safefree(file);

      delete ti.atracks;
      delete ti.vtracks;
      delete ti.stracks;
      delete ti.audio_syncs;
      delete ti.cue_creations;
      delete ti.default_track_flags;
      delete ti.languages;
      memset(&ti, 0, sizeof(track_info_t));
      ti.audio_syncs = new vector<audio_sync_t>;
      ti.cue_creations = new vector<cue_creation_t>;
      ti.default_track_flags = new vector<int64_t>;
      ti.languages = new vector<language_t>;
      ti.aspect_ratio = 1.0;
      ti.atracks = new vector<int64_t>;
      ti.vtracks = new vector<int64_t>;
      ti.stracks = new vector<int64_t>;
    }
  }

  if (files.size() == 0) {
    usage();
    exit(1);
  }

  if (no_linking) {
    if ((seguid_link_previous != NULL) || (seguid_link_next != NULL)) {
      fprintf(stderr, "Error: '--dont-link' cannot be used together with "
              "'--link-to-previous' or '--link-to-next'.\n");
      exit(1);
    }
    if (split_after <= 0)
      fprintf(stderr, "Warning: '--dont-link' is only useful in combination "
              "with '--split'.\n");
  }
}

static char **add_string(int &num, char **values, char *new_string) {
  values = (char **)saferealloc(values, (num + 1) * sizeof(char *));
  values[num] = safestrdup(new_string);
  num++;

  return values;
}

static char *strip(char *s) {
  char *p;

  if (s[0] == 0)
    return s;

  p = &s[strlen(s) - 1];
  while ((p != s) && ((*p == '\n') || (*p == '\r') || isspace(*p))) {
    *p = 0;
    p--;
  }

  p = s;
  while ((*p != 0) && isspace(*p))
    p++;

  memmove(s, p, strlen(p) + 1);

  return s;
}

static char **read_args_from_file(int &num_args, char **args, char *filename) {
  mm_io_c *mm_io;
  char buffer[8192], *space;

  try {
    mm_io = new mm_io_c(filename, MODE_READ);
  } catch (exception &ex) {
    fprintf(stderr, "Error: Could not open file '%s' for reading command line "
            "arguments from.", filename);
    exit(1);
  }

  while (!mm_io->eof() && (mm_io->gets(buffer, 8191) != NULL)) {
    buffer[8191] = 0;
    strip(buffer);
    if ((buffer[0] == '#') || (buffer[0] == 0))
      continue;

    space = strchr(buffer, ' ');
    if ((space != NULL) && (buffer[0] == '-')) {
      *space = 0;
      space++;
    } else
      space = NULL;
    args = add_string(num_args, args, buffer);
    if (space != NULL)
      args = add_string(num_args, args, space);
  }

  delete mm_io;

  return args;
}

static void handle_args(int argc, char **argv) {
  int i, num_args;
  char **args;

  args = NULL;
  num_args = 0;

  for (i = 1; i < argc; i++)
    if (argv[i][0] == '@')
      args = read_args_from_file(num_args, args, &argv[i][1]);
    else
      args = add_string(num_args, args, argv[i]);

  parse_args(num_args, args);

  for (i = 0; i < num_args; i++)
    safefree(args[i]);
  if (args != NULL)
    safefree(args);
}

static void setup() {
#if ! defined WIN32
  if (setlocale(LC_CTYPE, "") == NULL) {
    fprintf(stderr, "Error: Could not set the locale properly. Check the "
            "LANG, LC_ALL and LC_CTYPE environment variables.\n");
    exit(1);
  }
#endif

  signal(SIGUSR1, sighandler);

  nice(2);

  srand(time(NULL));
  cc_local_utf8 = utf8_init(NULL);

  cluster_helper = new cluster_helper_c();
}

static void init_globals() {
  track_number = 1;
  cluster_helper = NULL;
  kax_seekhead = NULL;
  kax_seekhead_void = NULL;
  video_fps = -1.0;
  default_tracks[0] = 0;
  default_tracks[1] = 0;
  default_tracks[2] = 0;
}

static void destroy_readers() {
  int i;
  filelist_t *file;

  for (i = 0; i < files.size(); i++) {
    file = files[i];
    if (file->reader != NULL) {
      delete file->reader;
      file->reader = NULL;
    }
  }

  while (packetizers.size()) {
    safefree(packetizers[packetizers.size() - 1]);
    packetizers.pop_back();
  }
}

static void cleanup() {
  delete cluster_helper;
  filelist_t *file;

  destroy_readers();

  while (files.size()) {
    file = files[files.size() - 1];
    free_track_info(file->ti);
    safefree(file);
    files.pop_back();
  }

  safefree(outfile);

  utf8_done();
}

// Transform the output filename and insert the current file number.
// Rules and search order:
// 1) %d
// 2) %[0-9]+d
// 3) . ("-%03d" will be inserted before the .)
// 4) "-%03d" will be appended
string create_output_name() {
  int p, p2, i;
  string s(outfile);
  bool ok;
  char buffer[20];

  // First possibility: %d
  p = s.find("%d");
  if (p >= 0) {
    sprintf(buffer, "%d", file_num);
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
    char buffer[strtol(len.c_str(), NULL, 10) + 1];
    sprintf(buffer, format.c_str(), file_num);
    s.replace(p, format.size(), buffer);

    return s;
  }

  sprintf(buffer, "-%03d", file_num);

  // See if we can find a '.'.
  p = s.rfind(".");
  if (p >= 0)
    s.insert(p, buffer);
  else
    s.append(buffer);

  return s;
}

void create_next_output_file(bool last_file, bool first_file) {
  string this_outfile;

  kax_segment = new KaxSegment();
  kax_cues = new KaxCues();
  kax_cues->SetGlobalTimecodeScale(TIMECODE_SCALE);

  if (pass == 1) {
    // Open the a dummy file.
    try {
      out = new mm_null_io_c();
    } catch (exception &ex) {
      die("mkvmerge.cpp/create_next_output_file(): Could not create a dummy "
          "output class.");
    }

    cluster_helper->set_output(out);
    render_headers(out, last_file, first_file);

    return;
  }

  if (pass == 2)
    this_outfile = create_output_name();
  else
    this_outfile = outfile;

  // Open the output file.
  try {
    out = new mm_io_c(this_outfile.c_str(), MODE_CREATE);
  } catch (exception &ex) {
    fprintf(stderr, "Error: Couldn't open output file %s (%s).\n",
            this_outfile.c_str(), strerror(errno));
    exit(1);
  }
  if (verbose)
    fprintf(stdout, "Opened '%s\' for writing.\n", this_outfile.c_str());

  cluster_helper->set_output(out);
  render_headers(out, last_file, first_file);

  file_num++;
}

void finish_file() {
  int64_t old_pos;
  int i;

  // Render the cues.
  if (write_cues && cue_writing_requested) {
    if (verbose == 1)
      fprintf(stdout, "Writing cue entries (the index)...");
    kax_cues->Render(*out);
    if (verbose == 1)
      fprintf(stdout, "\n");
  }

  // Write meta seek information if it is not disabled.
  if (write_meta_seek) {
    if (write_cues && cue_writing_requested)
      kax_seekhead->IndexThis(*kax_cues, *kax_segment);

    kax_seekhead->UpdateSize();
    if (kax_seekhead_void->ReplaceWith(*kax_seekhead, *out, true) == 0) {
      fprintf(stdout, "Warning: Could not update the meta seek information "
              "as the space reserved for them was too small. Re-run "
              "mkvmerge with the additional parameters '--meta-seek-size "
              "%lld'.\n", kax_seekhead->ElementSize() + 100);

      if (write_cues && cue_writing_requested) {
        delete kax_seekhead;

        kax_seekhead = new KaxSeekHead();
        kax_seekhead->IndexThis(*kax_cues, *kax_segment);
        kax_seekhead->UpdateSize();
        kax_seekhead_void->ReplaceWith(*kax_seekhead, *out, true);
      }
    }
  }

  // Now re-render the kax_duration and fill in the biggest timecode
  // as the file's duration.
  old_pos = out->getFilePointer();
  out->setFilePointer(kax_duration->GetElementPosition());
  *(static_cast<EbmlFloat *>(kax_duration)) =
    (cluster_helper->get_max_timecode() -
     cluster_helper->get_first_timecode()) * 1000000.0 / TIMECODE_SCALE;
  kax_duration->Render(*out);
  out->setFilePointer(old_pos);

  // Set the correct size for the segment.
  if (kax_segment->ForceSize(out->getFilePointer() -
                             kax_segment->GetElementPosition() -
                             kax_segment->HeadSize()))
    kax_segment->OverwriteHead(*out);

  delete out;
  delete kax_segment;
  delete kax_cues;
  if (write_meta_seek) {
    delete kax_seekhead_void;
    delete kax_seekhead;
  }

  for (i = 0; i < packetizers.size(); i++)
    packetizers[i]->packetizer->reset();;
}

void main_loop() {
  packet_t *pack;
  int i;
  packetizer_t *ptzr, *winner;

  // Let's go!
  while (1) {
    // Step 1: Make sure a packet is available for each output
    // as long we havne't already processed the last one.
    for (i = 0; i < packetizers.size(); i++) {
      ptzr = packetizers[i];
      while ((ptzr->pack == NULL) && (ptzr->status == EMOREDATA) &&
             (ptzr->packetizer->packet_available() < 2))
        ptzr->status = ptzr->packetizer->read();
      if (ptzr->pack == NULL)
        ptzr->pack = ptzr->packetizer->get_packet();
      if ((ptzr->pack != NULL) && !ptzr->packetizer->packet_available())
        ptzr->pack->duration_mandatory = 1;
    }

    // Step 2: Pick the packet with the lowest timecode and
    // stuff it into the Matroska file.
    winner = NULL;
    for (i = 0; i < packetizers.size(); i++) {
      ptzr = packetizers[i];
      if (ptzr->pack != NULL) {
        if ((winner == NULL) || (winner->pack == NULL))
          winner = ptzr;
        else if (ptzr->pack &&
                 (ptzr->pack->timecode < winner->pack->timecode))
          winner = ptzr;
      }
    }
    if ((winner != NULL) && (winner->pack != NULL))
      pack = winner->pack;
    else                        // exit if there are no more packets
      break;

    // Step 3: Add the winning packet to a cluster. Full clusters will be
    // rendered automatically.
    cluster_helper->add_packet(pack);

    winner->pack = NULL;

    // display some progress information
    if (verbose >= 1)
      display_progress(0);
  }

  // Render all remaining packets (if there are any).
  if ((cluster_helper != NULL) && (cluster_helper->get_packet_count() > 0))
    cluster_helper->render();

  if (verbose == 1) {
    display_progress(1);
    fprintf(stdout, "\n");
  }
}

int main(int argc, char **argv) {
  init_globals();

  setup();

  handle_args(argc, argv);

  if (split_after > 0) {
    fprintf(stdout, "Pass 1: finding split points. This may take a while."
            "\n\n");

    create_readers();

    pass = 1;
    create_next_output_file(true, true);
    main_loop();
    finish_file();

    fprintf(stdout, "\nPass 2: merging the files. This will take even longer."
            "\n\n");

    delete cluster_helper;
    destroy_readers();

    init_globals();
    cluster_helper = new cluster_helper_c();
    cluster_helper->find_next_splitpoint();
    create_readers();

    pass = 2;
    create_next_output_file(cluster_helper->get_next_splitpoint() >= 
                            cluster_helper_c::splitpoints.size(), true);
    main_loop();
    finish_file();

  } else {

    create_readers();

    pass = 0;
    create_next_output_file(true, true);
    main_loop();
    finish_file();
  }

  cleanup();

  return 0;
}
