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

// {{{ includes

#include "os.h"

#include <errno.h>
#include <ctype.h>
#if defined(SYS_UNIX) || defined(COMP_CYGWIN)
#include <signal.h>
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

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
#include <matroska/KaxTagMulti.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>
#include <matroska/KaxVersion.h>

#include "chapters.h"
#include "cluster_helper.h"
#include "common.h"
#include "iso639.h"
#include "mkvmerge.h"
#include "mm_io.h"
#include "r_aac.h"
#include "r_ac3.h"
#include "r_avi.h"
#include "r_dts.h"
#include "r_matroska.h"
#include "r_mp3.h"
#ifdef HAVE_OGGVORBIS
#include "r_ogm.h"
#endif
#include "r_qtmp4.h"
#include "r_real.h"
#include "r_srt.h"
#include "r_ssa.h"
#include "r_wav.h"
#include "tagparser.h"

using namespace libmatroska;
using namespace std;

// }}}

// {{{ global structs and variables

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

typedef struct {
  char *name, *mime_type, *description;
  int64_t size;
  bool to_all_files;
} attachment_t;

vector<packetizer_t *> packetizers;
vector<filelist_t *> files;
vector<attachment_t *> attachments;
int64_t attachment_sizes_first = 0, attachment_sizes_others = 0;

// Variables set by the command line parser.
char *outfile = NULL;
int64_t file_sizes = 0;
int max_blocks_per_cluster = 65535;
int max_ms_per_cluster = 2000;
bool write_cues = true, cue_writing_requested = false;
bool video_track_present = false;
bool write_meta_seek_for_clusters = true;
bool no_lacing = true, no_linking = false;
int64_t split_after = -1;
bool split_by_time = false;
int split_max_num_files = 65535;

float video_fps = -1.0;
int default_tracks[3];

bool identifying = false;

char *dump_packets = NULL;

cluster_helper_c *cluster_helper = NULL;
KaxSegment *kax_segment;
KaxInfo *kax_infos;
KaxTracks *kax_tracks;
KaxTrackEntry *kax_last_entry;
KaxCues *kax_cues;
EbmlVoid *kax_sh_void = NULL;
KaxDuration *kax_duration;
KaxSeekHead *kax_sh_main = NULL, *kax_sh_cues = NULL;
KaxTags *kax_tags = NULL;
KaxAttachments *kax_as = NULL;
KaxChapters *kax_chapters = NULL;
EbmlVoid *kax_chapters_void = NULL;

char *chapter_file_name = NULL;
char *chapter_language = NULL;
char *chapter_charset = NULL;

string title;

int64_t tags_size = 0;
bool accept_tags = true;

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
   {"aac", TYPEAAC, "AAC (Advanced Audio Coding)"},
   {"ac3", TYPEAC3, "A/52 (aka AC3)"},
   {"avi", TYPEAVI, "AVI (Audio/Video Interleaved)"},
   {"dts", TYPEDTS, "DTS (Digital Theater System)"},
   {"mp3", TYPEMP3, "MPEG1 layer III audio (CBR and VBR/ABR)"},
   {"mkv", TYPEMATROSKA, "general Matroska files"},
#ifdef HAVE_OGGVORBIS
   {"ogg", TYPEOGM, "general OGG media stream, audio/video embedded in OGG"},
#endif // HAVE_OGGVORBIS
   {"mov", TYPEQTMP4, "Quicktime/MP4 audio and video"},
   {"rm ", TYPEREAL, "RealMedia audio and video"},
   {"srt", TYPESRT, "SRT text subtitles"},
   {"ssa", TYPESSA, "SSA/ASS text subtitles"},
   {"wav", TYPEWAV, "WAVE (uncompressed PCM)"},
   {"output modules:", -1, ""},
   {"   ", -1,      "AAC audio"},
   {"   ", -1,      "AC3 audio"},
   {"   ", -1,      "DTS audio"},
   {"   ", -1,      "MP3 audio"},
   {"   ", -1,      "simple text subtitles"},
   {"   ", -1,      "uncompressed PCM audio"},
   {"   ", -1,      "Video (not MPEG1/2)"},
#ifdef HAVE_OGGVORBIS
   {"   ", -1,      "Vorbis audio"},
#endif // HAVE_OGGVORBIS
   {NULL,  -1,      NULL}};

// }}}

// {{{ FUNCTION usage()

static void usage() {
  mxinfo(
    "mkvmerge -o out [global options] [options] <file1> [@optionsfile ...]"
    "\n\n Global options:\n"
    "  -v, --verbose            verbose status\n"
    "  -q, --quiet              suppress status output\n"
    "  -o, --output out         Write to the file 'out'.\n"
    "  --title <title>          Title for this output file.\n"
    "  --chapters <file>        Read chapter information from the file.\n"
    "  --chapter-language <lng> Set the 'language' element in chapter entries."
    "\n  --chapter-charset <cset> Charset for a simple chapter file.\n"
    "  --global-tags <file>     Read global tags from a XML file.\n"
    "\n General output control (still global, advanced options):\n"
    "  --cluster-length <n[ms]> Put at most n data blocks into each cluster.\n"
    "                           If the number is postfixed with 'ms' then\n"
    "                           put at most n milliseconds of data into each\n"
    "                           cluster.\n"
    "  --no-cues                Do not write the cue data (the index).\n"
    "  --no-clusters-in-meta-seek\n"
    "                           Do not write meta seek data for clusters.\n"
    "  --enable-lacing          Use lacing.\n"
    "\n File splitting and linking (still global):\n"
    "  --split <d[K,M,G]|HH:MM:SS|ns>\n"
    "                           Create a new file after d bytes (KB, MB, GB)\n"
    "                           or after a specific time.\n"
    "  --split-max-files <n>    Create at most n files.\n"
    "  --dont-link              Don't link splitted files.\n"
    "  --link-to-previous <UID> Link the first file to the given UID.\n"
    "  --link-to-next <UID>     Link the last file to the given UID.\n"
    "\n Attachment support (still global):\n"
    "  --attachment-description <desc>\n"
    "                           Description for the following attachment.\n"
    "  --attachment-mime-type <mime type>\n"
    "                           Mime type for the following attachment.\n"
    "  --attach-file <file>     Creates a file attachment inside the\n"
    "                           Matroska file.\n"
    "  --attach-file-once <file>\n"
    "                           Creates a file attachment inside the\n"
    "                           firsts Matroska file written.\n"
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
    "  -t, --tags <TID:file>    Read tags for the track from a XML file.\n"
    "  --aac-is-sbr <TID>       Track with the ID is HE-AAC/AAC+/SBR-AAC.\n"
    "\n Options that only apply to video tracks:\n"
    "  -f, --fourcc <FOURCC>    Forces the FourCC to the specified value.\n"
    "                           Works only for video tracks.\n"
    "  --aspect-ratio <f|a/b>   Sets the aspect ratio.\n"
    "\n Options that only apply to text subtitle tracks:\n"
    "  --sub-charset <TID:charset>\n"
    "                           Sets the charset the text subtitles are\n"
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

// }}}

// {{{ FUNCTION get_type(char *filename)

static int get_type(char *filename) {
  mm_io_c *mm_io;
  mm_text_io_c *mm_text_io;
  uint64_t size;
  int type;

  try {
    mm_io = new mm_io_c(filename, MODE_READ);
    mm_io->setFilePointer(0, seek_end);
    size = mm_io->getFilePointer();
    mm_io->setFilePointer(0, seek_current);
  } catch (exception &ex) {
    mxerror("Could not open source file or seek properly in it "
            "(%s).\n", filename);
  }

  if (avi_reader_c::probe_file(mm_io, size))
    type = TYPEAVI;
  else if (kax_reader_c::probe_file(mm_io, size))
    type = TYPEMATROSKA;
  else if (wav_reader_c::probe_file(mm_io, size))
    type = TYPEWAV;
#ifdef HAVE_OGGVORBIS
  else if (ogm_reader_c::probe_file(mm_io, size))
    type = TYPEOGM;
#endif // HAVE_OGGVORBIS
  else if (real_reader_c::probe_file(mm_io, size))
    type = TYPEREAL;
  else if (qtmp4_reader_c::probe_file(mm_io, size))
    type = TYPEQTMP4;
  else if (mp3_reader_c::probe_file(mm_io, size))
    type = TYPEMP3;
  else if (ac3_reader_c::probe_file(mm_io, size))
    type = TYPEAC3;
  else if (dts_reader_c::probe_file(mm_io, size))
    type = TYPEDTS;
  else if (aac_reader_c::probe_file(mm_io, size))
    type = TYPEAAC;
  else {
    delete mm_io;

    try {
      mm_text_io = new mm_text_io_c(filename);
      mm_text_io->setFilePointer(0, seek_end);
      size = mm_text_io->getFilePointer();
      mm_text_io->setFilePointer(0, seek_current);
    } catch (exception &ex) {
      mxerror("Could not open source file or seek properly in it (%s).\n",
              filename);
    }

    if (srt_reader_c::probe_file(mm_text_io, size))
      type = TYPESRT;
    else if (ssa_reader_c::probe_file(mm_text_io, size))
      type = TYPESSA;
    else
      type = TYPEUNKNOWN;

    mm_io = mm_text_io;
  }

  delete mm_io;

  file_sizes += size;

  return type;
}

// }}}

// {{{ helper functions (sighandler, display_progress)

#if defined(SYS_UNIX) || defined(COMP_CYGWIN)
static void sighandler(int signum) {
#ifdef DEBUG
  if (signum == SIGUSR1)
    debug_facility.dump_info();
#endif // DEBUG
}
#endif

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

// }}}

// {{{ command line parsing, helper functions

void parse_and_add_tags(const char *file_name) {
  KaxTags *tags;
  KaxTag *tag;

  tags = new KaxTags;

  parse_xml_tags(file_name, tags);

  if (!tags->CheckMandatory())
    mxerror("Error parsing the tags in '%s': some mandatory "
            "elements are missing.\n", file_name);

  while (tags->ListSize() > 0) {
    tag = (KaxTag *)(*tags)[0];
    if (!tag->CheckMandatory())
      mxerror("Error parsing the tags in '%s': some mandatory "
              "elements are missing.\n", file_name);
    tags->Remove(0);
    add_tags(tag);
  }

  delete tags;
}

void add_tags(KaxTag *tags) {
  if (!accept_tags)
    return;

  if (kax_tags == NULL)
    kax_tags = new KaxTags;

  kax_tags->PushElement(*tags);
}

void add_packetizer(generic_packetizer_c *packetizer) {
  packetizer_t *pack = (packetizer_t *)safemalloc(sizeof(packetizer_t));
  pack->packetizer = packetizer;
  pack->status = EMOREDATA;
  pack->pack = NULL;
  packetizers.push_back(pack);
}

static void parse_tracks(char *s, vector<int64_t> *tracks, const char *opt) {
  char *comma;
  int64_t tid;
  string orig = s;

  tracks->clear();

  comma = strchr(s, ',');
  while ((comma != NULL) && (*s != 0)) {
    *comma = 0;

    if (!parse_int(s, tid))
      mxerror("Invalid track ID in '%s %s'.\n", opt, orig.c_str());
    tracks->push_back(tid);

    s = &comma[1];
    comma = strchr(s, ',');
  }

  if (*s != 0) {
    if (!parse_int(s, tid))
      mxerror("Invalid track ID in '%s %s'.\n", opt, orig.c_str());
    tracks->push_back(tid);
  }
}

static void parse_sync(char *s, audio_sync_t &async, const char *opt) {
  char *linear, *div, *colon;
  double d1, d2;
  string orig = s;

  // Extract the track number.
  if ((colon = strchr(s, ':')) == NULL)
    mxerror("Invalid sync option. No track ID specified in '%s %s'.\n", s,
            opt);

  *colon = 0;
  if (!parse_int(s, async.id))
    mxerror("Invalid track ID specified in '%s %s'.\n", opt, s);

  s = &colon[1];
  if (*s == 0)
    mxerror("Invalid sync option specified in '%s %s'.\n", opt, orig.c_str());

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
      if (d2 == 0.0)
        mxerror("Linear sync: division by zero in '%s %s'.\n", opt,
                orig.c_str());

      async.linear = d1 / d2;
    }
    if (async.linear <= 0.0)
      mxerror("Linear sync value may not be <= 0 in '%s %s'.\n", opt,
              orig.c_str());

  } else
    async.linear = 1.0;
  async.displacement = atoi(s);
}

static float parse_aspect_ratio(char *s, const char *opt) {
  char *div;
  float w, h;
  string orig = s;

  div = strchr(s, '/');
  if (div == NULL)
    div = strchr(s, ':');
  if (div == NULL)
    return strtod(s, NULL);

  *div = 0;
  div++;
  if (*s == 0)
    mxerror("Aspect ratio: missing dividend in '%s %s'.\n", opt, orig.c_str());

  if (*div == 0)
    mxerror("Aspect ratio: missing divisor in '%s %s'.\n", opt, orig.c_str());

  w = strtod(s, NULL);
  h = strtod(div, NULL);
  if (h == 0.0)
    mxerror("Aspect ratio: divisor is 0 in '%s %s'.\n", opt, orig.c_str());

  return w/h;
}

static void parse_split(const char *arg) {
  int64_t modifier;
  char *s, *p;
  string orig = arg;

  if ((arg == NULL) || (arg[0] == 0) || (arg[1] == 0))
    mxerror("Invalid format for '--split' in '--split %s'.\n", arg);

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
        (secs < 0) || (secs > 59) || (split_after < 10))
      mxerror("Invalid time for '--split' in '--split %s'.\n", orig.c_str());

    split_after *= 1000;
    split_by_time = true;
    safefree(s);
    return;
  }

  // Number of seconds: e.g. 1000s or 4254S
  if ((s[strlen(s) - 1] == 's') || (s[strlen(s) - 1] == 'S')) {
    s[strlen(s) - 1] = 0;
    if (!parse_int(s, split_after))
      mxerror("Invalid time for '--split' in '--split %s'.\n", orig.c_str());

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
  if (!parse_int(s, split_after))
    mxerror("Invalid split size in '--split %s'.\n", orig.c_str());

  split_after *= modifier;
  if (split_after <= (1024 * 1024))
    mxerror("Invalid split size in '--split %s'.\n", orig.c_str());

  safefree(s);
  split_by_time = false;
}

static void parse_cues(char *s, cue_creation_t &cues) {
  char *colon;
  string orig = s;

  // Extract the track number.
  if ((colon = strchr(s, ':')) == NULL)
    mxerror("Invalid cues option. No track ID specified in '--cues %s'.\n", s);

  *colon = 0;
  if (!parse_int(s, cues.id))
    mxerror("Invalid track ID specified in '--cues %s'.\n", orig.c_str());

  s = &colon[1];
  if (*s == 0)
    mxerror("Invalid cues option specified in '--cues %s'.\n", orig.c_str());

  if (!strcmp(s, "all"))
    cues.cues = CUES_ALL;
  else if (!strcmp(s, "iframes"))
    cues.cues = CUES_IFRAMES;
  else if (!strcmp(s, "none"))
    cues.cues = CUES_NONE;
  else
    mxerror("'%s' is an unsupported argument for --cues.\n", orig.c_str());
}

static void parse_language(char *s, language_t &lang) {
  char *colon;
  string orig = s;

  // Extract the track number.
  if ((colon = strchr(s, ':')) == NULL)
    mxerror("Invalid language option. No track ID specified in "
            "'--language %s'.\n", orig.c_str());

  *colon = 0;
  if (!parse_int(s, lang.id))
    mxerror("Invalid track ID specified in '--language %s'.\n", orig.c_str());

  s = &colon[1];
  if (*s == 0)
    mxerror("Invalid language specified in '--language %s'.\n", orig.c_str());

  if (!is_valid_iso639_2_code(s))
    mxerror("'%s' is not a valid ISO639-2 code. See "
            "'mkvmerge --list-languages'.\n", s);
  
  lang.language = s;
}

static void parse_sub_charset(char *s, language_t &sub_charset) {
  char *colon;
  string orig = s;

  // Extract the track number.
  if ((colon = strchr(s, ':')) == NULL)
    mxerror("Invalid sub charset option. No track ID specified in "
            "'--sub-charset %s'.\n", orig.c_str());

  *colon = 0;
  if (!parse_int(s, sub_charset.id))
    mxerror("Invalid track ID specified in '--sub-charset %s'.\n",
            orig.c_str());

  s = &colon[1];
  if (*s == 0)
    mxerror("Invalid sub charset specified in '--sub-charset %s'.\n",
            orig.c_str());

  sub_charset.language = s;
}

static void parse_tags(char *s, tags_t &tags, const char *opt) {
  char *colon;
  string orig = s;

  // Extract the track number.
  if ((colon = strchr(s, ':')) == NULL)
    mxerror("Invalid tags option. No track ID specified in '%s %s'.\n", opt,
            orig.c_str());
  *colon = 0;
  if (!parse_int(s, tags.id))
    mxerror("Invalid track ID specified in '%s %s'.\n", opt, orig.c_str());

  s = &colon[1];
  if (*s == 0)
    mxerror("Invalid tags file name specified in '%s %s'.\n", opt,
            orig.c_str());

  tags.file_name = s;
}

// }}}

// {{{ render functions (render_headers, render_attachments)

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

    if (title.length() > 0)
      *((EbmlUnicodeString *)&GetChild<KaxTitle>(*kax_infos)) =
        cstr_to_UTFstring(title.c_str());

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
    kax_sh_main = new KaxSeekHead();
    kax_sh_void = new EbmlVoid();
    kax_sh_void->SetSize(100);
    kax_sh_void->Render(*out);
    if (write_meta_seek_for_clusters)
      kax_sh_cues = new KaxSeekHead();

    kax_infos->Render(*out);

    kax_tracks = &GetChild<KaxTracks>(*kax_segment);
    kax_last_entry = NULL;

    for (i = 0; i < files.size(); i++)
      files[i]->reader->set_headers();

    kax_tracks->Render(*out);
  } catch (exception &ex) {
    mxerror("Could not render the track headers.\n");
  }
}

static void render_attachments(IOCallback *out) {
  KaxAttached *kax_a;
  KaxFileData *fdata;
  attachment_t *attch;
  int i;
  char *name;
  binary *buffer;
  int64_t size;
  mm_io_c *io;

  if (!(((file_num == 1) && (attachment_sizes_first > 0)) ||
        (attachment_sizes_others > 0)))
    return;

  if (kax_as != NULL)
    delete kax_as;
  kax_as = new KaxAttachments();
  kax_a = NULL;
  for (i = 0; i < attachments.size(); i++) {
    attch = attachments[i];

    if ((file_num == 1) || attch->to_all_files) {
      if (kax_a == NULL)
        kax_a = &GetChild<KaxAttached>(*kax_as);
      else
        kax_a = &GetNextChild<KaxAttached>(*kax_as, *kax_a);

      if (attch->description != NULL)
        *static_cast<EbmlUnicodeString *>
          (&GetChild<KaxFileDescription>(*kax_a)) =
          cstr_to_UTFstring(attch->description);

      if (attch->mime_type != NULL)
        *static_cast<EbmlString *>(&GetChild<KaxMimeType>(*kax_a)) =
          attch->mime_type;

      name = &attch->name[strlen(attch->name) - 1];
      while ((name != attch->name) && (*name != '/'))
        name--;
      if (*name == '/')
        name++;
      if (*name == 0)
        die("Internal error: *name == 0 on %d.", __LINE__);

      *static_cast<EbmlUnicodeString *>
        (&GetChild<KaxFileName>(*kax_a)) = cstr_to_UTFstring(name);

      *static_cast<EbmlUInteger *>
        (&GetChild<KaxFileUID>(*kax_a)) = create_unique_uint32();

      try {
        io = new mm_io_c(attch->name, MODE_READ);
        io->setFilePointer(0, seek_end);
        size = io->getFilePointer();
        io->setFilePointer(0, seek_beginning);

        buffer = new binary[size];
        io->read(buffer, size);
        delete io;

        fdata = &GetChild<KaxFileData>(*kax_a);
        fdata->SetBuffer(buffer, size);
      } catch (...) {
        mxerror("Could not open the attachment '%s'.\n", attch->name);
      }
    }
  }

  kax_as->Render(*out);
}

// }}}

// {{{ FUNCTION create_readers()

static void create_readers() {
  filelist_t *file;
  int i;

  for (i = 0; i < files.size(); i++) {
    file = files[i];
    try {
      switch (file->type) {
        case TYPEMATROSKA:
          file->reader = new kax_reader_c(file->ti);
          break;
#ifdef HAVE_OGGVORBIS
        case TYPEOGM:
          file->reader = new ogm_reader_c(file->ti);
          break;
#endif // HAVE_OGGVORBIS
        case TYPEAVI:
          file->reader = new avi_reader_c(file->ti);
          break;
        case TYPEREAL:
          file->reader = new real_reader_c(file->ti);
          break;
        case TYPEWAV:
          file->reader = new wav_reader_c(file->ti);
          break;
        case TYPESRT:
          file->reader = new srt_reader_c(file->ti);
          break;
        case TYPEMP3:
          file->reader = new mp3_reader_c(file->ti);
          break;
        case TYPEAC3:
          file->reader = new ac3_reader_c(file->ti);
          break;
        case TYPEDTS:
          file->reader = new dts_reader_c(file->ti);
          break;
        case TYPEAAC:
          file->reader = new aac_reader_c(file->ti);
          break;
        case TYPESSA:
          file->reader = new ssa_reader_c(file->ti);
          break;
        case TYPEQTMP4:
          file->reader = new qtmp4_reader_c(file->ti);
          break;
        default:
          mxerror("EVIL internal bug! (unknown file type)\n");
          break;
      }
    } catch (error_c error) {
      mxerror("Demultiplexer failed to initialize:\n%s\n", error.get_error());
    }
  }
}

// }}}

// {{{ FUNCTION identify(const char *filename)

static void identify(const char *filename) {
  track_info_t ti;
  filelist_t *file;

  memset(&ti, 0, sizeof(track_info_t));
  ti.audio_syncs = new vector<audio_sync_t>;
  ti.cue_creations = new vector<cue_creation_t>;
  ti.default_track_flags = new vector<int64_t>;
  ti.languages = new vector<language_t>;
  ti.sub_charsets = new vector<language_t>;
  ti.all_tags = new vector<tags_t>;
  ti.aspect_ratio = 0.0;
  ti.atracks = new vector<int64_t>;
  ti.vtracks = new vector<int64_t>;
  ti.stracks = new vector<int64_t>;
  ti.aac_is_sbr = new vector<int64_t>;

  file = (filelist_t *)safemalloc(sizeof(filelist_t));

  file->name = safestrdup(filename);
  file->type = get_type(file->name);
  ti.fname = file->name;
  
  if (file->type == TYPEUNKNOWN)
    mxerror("File %s has unknown type. Please have a look "
            "at the supported file types ('mkvmerge --list-types') and "
            "contact me at moritz@bunkus.org if your file type is "
            "supported but not recognized properly.\n", file->name);

  file->fp = NULL;
  file->status = EMOREDATA;
  file->pack = NULL;
  file->ti = duplicate_track_info(&ti);

  files.push_back(file);

  verbose = 0;
  identifying = true;
  create_readers();

  file->reader->identify();
}

// }}}

// {{{ FUNCTION parse_args(int argc, char **argv)

static void parse_args(int argc, char **argv) {
  track_info_t ti;
  int i, j;
  filelist_t *file;
  char *s, *this_arg, *next_arg;
  audio_sync_t async;
  cue_creation_t cues;
  int64_t id;
  language_t lang;
  attachment_t *attachment;
  tags_t tags;
  mm_io_c *io;

  memset(&ti, 0, sizeof(track_info_t));
  ti.audio_syncs = new vector<audio_sync_t>;
  ti.cue_creations = new vector<cue_creation_t>;
  ti.default_track_flags = new vector<int64_t>;
  ti.languages = new vector<language_t>;
  ti.sub_charsets = new vector<language_t>;
  ti.all_tags = new vector<tags_t>;
  ti.aac_is_sbr = new vector<int64_t>;
  ti.aspect_ratio = 0.0;
  ti.atracks = new vector<int64_t>;
  ti.vtracks = new vector<int64_t>;
  ti.stracks = new vector<int64_t>;
  attachment = (attachment_t *)safemalloc(sizeof(attachment_t));
  memset(attachment, 0, sizeof(attachment_t));
  memset(&tags, 0, sizeof(tags_t));

  // Check if only information about the file is wanted. In this mode only
  // two parameters are allowed: the --identify switch and the file.
  if ((argc == 2) &&
      (!strcmp(argv[0], "-i") || !strcmp(argv[0], "--identify"))) {
    identify(argv[1]);
    mxexit();
  }

  // First parse options that either just print some infos and then exit
  // or that are needed right at the beginning.
  for (i = 0; i < argc; i++) {
    this_arg = argv[i];
    if ((i + 1) >= argc)
      next_arg = NULL;
    else
      next_arg = argv[i + 1];

    if (!strcmp(this_arg, "-V") || !strcmp(this_arg, "--version")) {
      mxinfo(VERSIONINFO "\n");
      mxexit(0);

    } else if (!strcmp(this_arg, "-h") || !strcmp(this_arg, "-?") ||
               !strcmp(this_arg, "--help")) {
      usage();
      mxexit(0);

    } else if (!strcmp(this_arg, "-o") || !strcmp(this_arg, "--output")) {
      if (next_arg == NULL)
        mxerror("'%s' lacks a file name.\n", this_arg);

      if (outfile != NULL)
        mxerror("Only one output file allowed.\n");

      outfile = safestrdup(next_arg);
      i++;

    } else if (!strcmp(this_arg, "-l") || !strcmp(this_arg, "--list-types")) {
      mxinfo("Known file types:\n  ext  description\n"
             "  ---  --------------------------\n");
      for (j = 1; file_types[j].ext; j++)
        mxinfo("  %s  %s\n", file_types[j].ext, file_types[j].desc);
      mxexit(0);

    } else if (!strcmp(this_arg, "--list-languages")) {
      list_iso639_languages();
      mxexit(0);

    } else if (!strcmp(this_arg, "-i") || !strcmp(this_arg, "--identify"))
      mxerror("'%s' can only be used with a file name. "
              "No other options are allowed.\n", this_arg);
  }

  if (outfile == NULL) {
    mxinfo("Error: no output file given.\n\n");
    usage();
    mxexit(2);
  }

  for (i = 0; i < argc; i++) {
    this_arg = argv[i];
    if ((i + 1) >= argc)
      next_arg = NULL;
    else
      next_arg = argv[i + 1];

    // Ignore the options we took care of in the first step.
    if (!strcmp(this_arg, "-o") || !strcmp(this_arg, "--output")) {
      i++;
      continue;
    }

    // Global options
    if (!strcmp(this_arg, "-q"))
      verbose = 0;

    else if (!strcmp(this_arg, "-v") || !strcmp(this_arg, "--verbose"))
      verbose = 2;

    else if (!strcmp(this_arg, "--title")) {
      if ((next_arg == NULL) || (next_arg[0] == 0))
        mxerror("'--title' lacks the title.\n");

      title = next_arg;
      i++;

    } else if (!strcmp(this_arg, "--split")) {
      if ((next_arg == NULL) || (next_arg[0] == 0))
        mxerror("'--split' lacks the size.\n");

      parse_split(next_arg);
      i++;

    } else if (!strcmp(this_arg, "--split-max-files")) {
      if ((next_arg == NULL) || (next_arg[0] == 0))
        mxerror("'--split-max-files' lacks the number of files.\n");

      if (!parse_int(next_arg, split_max_num_files) ||
          (split_max_num_files < 2))
        mxerror("Wrong argument to '--split-max-files'.\n");

      i++;

    } else if (!strcmp(this_arg, "--dont-link")) {
      no_linking = true;

    } else if (!strcmp(this_arg, "--link-to-previous")) {
      if ((next_arg == NULL) || (next_arg[0] == 0))
        mxerror("'--link-to-previous' lacks the previous UID.\n");

      if (seguid_link_previous != NULL)
        mxerror("Previous UID was already given in '%s %s'.\n", this_arg,
                next_arg);

      try {
        seguid_link_previous = new bitvalue_c(next_arg, 128);
      } catch (exception &ex) {
        mxerror("Unknown format for the previous UID in '%s %s'.\n", this_arg,
                next_arg);
      }

      i++;

    } else if (!strcmp(this_arg, "--link-to-next")) {
      if ((next_arg == NULL) || (next_arg[0] == 0))
        mxerror("'--link-to-next' lacks the previous UID.\n");

      if (seguid_link_next != NULL)
        mxerror("Next UID was already given in '%s %s'.\n", this_arg,
                next_arg);

      try {
        seguid_link_next = new bitvalue_c(next_arg, 128);
      } catch (exception &ex) {
        mxerror("Unknown format for the next UID in '%s %s'.\n", this_arg,
                next_arg);
      }

      i++;

    } else if (!strcmp(this_arg, "--cluster-length")) {
      if (next_arg == NULL)
        mxerror("'--cluster-length' lacks the length.\n");

      s = strstr("ms", next_arg);
      if (s != NULL) {
        *s = 0;
        if (!parse_int(next_arg, max_ms_per_cluster) ||
            (max_ms_per_cluster < 100) ||
            (max_ms_per_cluster > 32000))
          mxerror("Cluster length '%s' out of range (100..32000).\n",
                  next_arg);

        max_blocks_per_cluster = 65535;
      } else {
        if (!parse_int(next_arg, max_blocks_per_cluster) ||
            (max_blocks_per_cluster < 0) ||
            (max_blocks_per_cluster > 60000))
          mxerror("Cluster length '%s' out of range (0..60000).\n",
                  next_arg);

        max_ms_per_cluster = 32000;
      }

      i++;

    } else if (!strcmp(this_arg, "--no-cues"))
      write_cues = false;

    else if (!strcmp(this_arg, "--no-clusters-in-meta-seek"))
      write_meta_seek_for_clusters = false;

    else if (!strcmp(this_arg, "--enable-lacing"))
      no_lacing = false;

    else if (!strcmp(this_arg, "--attachment-description")) {
      if (next_arg == NULL)
        mxerror("'--attachment-description' lacks the description.\n");

      if (attachment->description != NULL)
        mxwarn("More than one description given for a single attachment. "
               "Discarding '%s' and using '%s'.\n", attachment->description,
               next_arg);
      safefree(attachment->description);
      attachment->description = safestrdup(next_arg);
      i++;

    } else if (!strcmp(this_arg, "--attachment-mime-type")) {
      if (next_arg == NULL)
        mxerror("'--attachment-mime-type' lacks the MIME type.\n");

      if (attachment->description != NULL)
        mxwarn("More than one MIME type given for a single attachment. "
               "Discarding '%s' and using '%s'.\n", attachment->mime_type,
               next_arg);
      safefree(attachment->mime_type);
      attachment->mime_type = safestrdup(next_arg);
      i++;

    } else if (!strcmp(this_arg, "--attach-file") ||
               !strcmp(this_arg, "--attach-file-once")) {
      if (next_arg == NULL)
        mxerror("'%s' lacks the file name.\n", this_arg);

      if (attachment->mime_type == NULL)
        mxerror("No MIME type was set for the attachment '%s'.\n",
                next_arg);

      attachment->name = safestrdup(next_arg);
      if (!strcmp(this_arg, "--attach-file"))
        attachment->to_all_files = true;
      try {
        io = new mm_io_c(attachment->name, MODE_READ);
        io->setFilePointer(0, seek_end);
        attachment->size = io->getFilePointer();
        delete io;
        if (attachment->size == 0)
          throw exception();
      } catch (...) {
        mxerror("Could not open the attachment '%s', or its "
                "size is 0.\n", attachment->name);
      }

      attachments.push_back(attachment);
      attachment = (attachment_t *)safemalloc(sizeof(attachment_t));
      memset(attachment, 0, sizeof(attachment_t));

      i++;

    } else if (!strcmp(this_arg, "--global-tags")) {
      if (next_arg == NULL)
        mxerror("'--global-tags' lacks the file name.\n");

      parse_and_add_tags(next_arg);
      i++;

    } else if (!strcmp(this_arg, "--chapter-language")) {
      if (next_arg == NULL)
        mxerror("'--chapter-language' lacks the language.\n");

      if (chapter_language != NULL)
        mxerror("'--chapter-language' may only be given once in '"
                "--chapter-language %s'.\n", next_arg);

      if (chapter_file_name != NULL)
        mxerror("'--chapter-language' must be given before '--chapters' in "
                "'--chapter-language %s'.\n", next_arg);

      if (!is_valid_iso639_2_code(next_arg))
        mxerror("'%s' is not a valid ISO639-2 language code. Run "
                "'mkvmerge --list-languages' for a complete list of language "
                "codes.\n", next_arg);

      chapter_language = safestrdup(next_arg);
      i++;

    } else if (!strcmp(this_arg, "--chapter-charset")) {
      if (next_arg == NULL)
        mxerror("'--chapter-charset' lacks the charset.\n");

      if (chapter_charset != NULL)
        mxerror("'--chapter-charset' may only be given once in '"
                "--chapter-charset %s'.\n", next_arg);

      if (chapter_file_name != NULL)
        mxerror("'--chapter-charset' must be given before '--chapters' in "
                "'--chapter-charset %s'.\n", next_arg);

      chapter_charset = safestrdup(next_arg);
      i++;

    } else if (!strcmp(this_arg, "--chapters")) {
      if (next_arg == NULL)
        mxerror("'--chapters' lacks the file name.\n");

      if (chapter_file_name != NULL)
        mxerror("Only one chapter file allowed in '%s %s'.\n", this_arg,
                next_arg);

      chapter_file_name = safestrdup(next_arg);
      kax_chapters = parse_chapters(next_arg, 0, -1, 0, chapter_language,
                                    chapter_charset);
      i++;

    } else if (!strcmp(this_arg, "--dump-packets")) {
      if (next_arg == NULL)
        mxerror("'--dump-packets' lacks the output path.\n");

      safefree(dump_packets);
      dump_packets = safestrdup(next_arg);
      i++;

    } else if (!strcmp(this_arg, "--meta-seek-size")) {
      mxwarn("The option '--meta-seek-size' is no longer supported. Please "
             "read mkvmerge's documentation, especially the section about "
             "the MATROSKA FILE LAYOUT.");
      i++;

    }

    // Options that apply to the next input file only.
    else if (!strcmp(this_arg, "-A") || !strcmp(this_arg, "--noaudio"))
      ti.no_audio = true;

    else if (!strcmp(this_arg, "-D") || !strcmp(this_arg, "--novideo"))
      ti.no_video = true;

    else if (!strcmp(this_arg, "-S") || !strcmp(this_arg, "--nosubs"))
      ti.no_subs = true;

    else if (!strcmp(this_arg, "-a") || !strcmp(this_arg, "--atracks")) {
      if (next_arg == NULL)
        mxerror("'%s' lacks the stream number(s).\n", this_arg);

      parse_tracks(next_arg, ti.atracks, this_arg);
      i++;

    } else if (!strcmp(this_arg, "-d") || !strcmp(this_arg, "--vtracks")) {
      if (next_arg == NULL)
        mxerror("'%s' lacks the stream number(s).\n", this_arg);

      parse_tracks(next_arg, ti.vtracks, this_arg);
      i++;

    } else if (!strcmp(this_arg, "-s") || !strcmp(this_arg, "--stracks")) {
      if (next_arg == NULL)
        mxerror("'%s' lacks the stream number(s).\n", this_arg);

      parse_tracks(next_arg, ti.stracks, this_arg);
      i++;

    } else if (!strcmp(this_arg, "-f") || !strcmp(this_arg, "--fourcc")) {
      if (next_arg == NULL)
        mxerror("'%s' lacks the FourCC.\n", this_arg);

      if (strlen(next_arg) != 4)
        mxerror("The FourCC must be exactly four characters long in '%s %s'."
                "\n", this_arg, next_arg);

      memcpy(ti.fourcc, next_arg, 4);
      ti.fourcc[4] = 0;
      i++;

    } else if (!strcmp(this_arg, "--aspect-ratio")) {
      if (next_arg == NULL)
        mxerror("'--aspect-ratio' lacks the aspect ratio.\n");

      ti.aspect_ratio = parse_aspect_ratio(next_arg, this_arg);
      ti.aspect_ratio_given = true;
      i++;

    } else if (!strcmp(this_arg, "-y") || !strcmp(this_arg, "--sync")) {
      if (next_arg == NULL)
        mxerror("'%s' lacks the audio delay.\n", this_arg);

      parse_sync(next_arg, async, this_arg);
      ti.audio_syncs->push_back(async);
      i++;

    } else if (!strcmp(this_arg, "--cues")) {
      if (next_arg == NULL)
        mxerror("'--cues' lacks its argument.\n");

      parse_cues(next_arg, cues);
      ti.cue_creations->push_back(cues);
      i++;

    } else if (!strcmp(this_arg, "--default-track")) {
      if (next_arg == NULL)
        mxerror("'--default-track' lacks the track ID.\n");

      if (!parse_int(next_arg, id))
        mxerror("Invalid track ID specified in '%s %s'.\n", this_arg,
                next_arg);

      ti.default_track_flags->push_back(id);
      i++;

    } else if (!strcmp(this_arg, "--language")) {
      if (next_arg == NULL)
        mxerror("'--language' lacks its argument.\n");

      parse_language(next_arg, lang);
      ti.languages->push_back(lang);
      i++;

    } else if (!strcmp(this_arg, "--sub-charset")) {
      if (next_arg == NULL)
        mxerror("'--sub-charset' lacks its argument.\n");

      parse_sub_charset(next_arg, lang);
      ti.sub_charsets->push_back(lang);
      i++;

    } else if (!strcmp(this_arg, "-t") || !strcmp(this_arg, "--tags")) {
      if (next_arg == NULL)
        mxerror("'%s' lacks its argument.\n", this_arg);

      parse_tags(next_arg, tags, this_arg);
      ti.all_tags->push_back(tags);
      i++;

    } else if (!strcmp(this_arg, "--aac-is-sbr")) {
      if (next_arg == NULL)
        mxerror("'%s' lacks the track ID.\n", this_arg);

      if (!parse_int(next_arg, id) || (id < 0))
        mxerror("Invalid track ID specified in '%s %s'.\n", this_arg,
                next_arg);

      ti.aac_is_sbr->push_back(id);
      i++;
    }

    // The argument is an input file.
    else {
      if ((ti.atracks->size() != 0) && ti.no_audio)
        mxerror("'-A' and '-a' used on the same source file.\n");

      if ((ti.vtracks->size() != 0) && ti.no_video)
        mxerror("'-D' and '-d' used on the same source file.\n");

      if ((ti.stracks->size() != 0) && ti.no_subs)
        mxerror("'-S' and '-s' used on the same source file.\n");

      file = (filelist_t *)safemalloc(sizeof(filelist_t));

      file->name = this_arg;
      file->type = get_type(file->name);
      ti.fname = this_arg;

      if (file->type == TYPEUNKNOWN)
        mxerror("File '%s' has unknown type. Please have a look "
                "at the supported file types ('mkvmerge --list-types') and "
                "contact me at moritz@bunkus.org if your file type is "
                "supported but not recognized properly.\n", file->name);

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
      delete ti.sub_charsets;
      delete ti.all_tags;
      delete ti.aac_is_sbr;
      memset(&ti, 0, sizeof(track_info_t));
      ti.audio_syncs = new vector<audio_sync_t>;
      ti.cue_creations = new vector<cue_creation_t>;
      ti.default_track_flags = new vector<int64_t>;
      ti.languages = new vector<language_t>;
      ti.sub_charsets = new vector<language_t>;
      ti.all_tags = new vector<tags_t>;
      ti.aac_is_sbr = new vector<int64_t>;
      ti.aspect_ratio = 0.0;
      ti.atracks = new vector<int64_t>;
      ti.vtracks = new vector<int64_t>;
      ti.stracks = new vector<int64_t>;
    }
  }

  if (files.size() == 0) {
    usage();
    mxexit();
  }

  if (no_linking) {
    if ((seguid_link_previous != NULL) || (seguid_link_next != NULL))
      mxerror("'--dont-link' cannot be used together with "
              "'--link-to-previous' or '--link-to-next'.\n");

    if (split_after <= 0)
      mxwarn("'--dont-link' is only useful in combination with '--split'.\n");
  }

  for (i = 0; i < attachments.size(); i++) {
    attachment_sizes_first += attachments[i]->size;
    if (attachments[i]->to_all_files)
      attachment_sizes_others += attachments[i]->size;
  }

  delete ti.audio_syncs;
  delete ti.cue_creations;
  delete ti.default_track_flags;
  delete ti.languages;
  delete ti.sub_charsets;
  delete ti.all_tags;
  delete ti.aac_is_sbr;
  delete ti.atracks;
  delete ti.vtracks;
  delete ti.stracks;
  safefree(attachment);
}

// }}}

// {{{ command line parsing helper functions

static char **add_string(int &num, char **values, const char *new_string) {
  values = (char **)saferealloc(values, (num + 1) * sizeof(char *));
  values[num] = safestrdup(new_string);
  num++;

  return values;
}

static char **read_args_from_file(int &num_args, char **args, char *filename) {
  mm_text_io_c *mm_io;
  string buffer, opt1, opt2;
  int space;

  try {
    mm_io = new mm_text_io_c(filename);
  } catch (exception &ex) {
    mxerror("Could not open file '%s' for reading command line "
            "arguments from.", filename);
  }

  while (!mm_io->eof() && mm_io->getline2(buffer)) {
    strip(buffer);
    if ((buffer[0] == '#') || (buffer[0] == 0))
      continue;

    space = buffer.find(" ");
    if (space >= 0) {
      opt1 = buffer.substr(space + 1);
      opt2 = buffer.substr(0, space);
      args = add_string(num_args, args, opt1.c_str());
      args = add_string(num_args, args, opt2.c_str());
    } else
      args = add_string(num_args, args, buffer.c_str());
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

// }}}

// {{{ global setup and cleanup functions

static void setup() {
#if ! defined(COMP_MSC)
  if (setlocale(LC_CTYPE, "") == NULL)
    mxerror("Could not set the locale properly. Check the "
            "LANG, LC_ALL and LC_CTYPE environment variables.\n");
#endif

#if defined(SYS_UNIX) || defined(COMP_CYGWIN)
  signal(SIGUSR1, sighandler);

  nice(2);
#endif

  srand(time(NULL));
  cc_local_utf8 = utf8_init(NULL);

  cluster_helper = new cluster_helper_c();
}

static void init_globals() {
  track_number = 1;
  cluster_helper = NULL;
  kax_sh_main = NULL;
  kax_sh_cues = NULL;
  kax_sh_void = NULL;
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
  if (kax_tags != NULL)
    delete kax_tags;
  if (kax_chapters != NULL)
    delete kax_chapters;
  if (kax_as != NULL)
    delete kax_as;

  utf8_done();
}

// }}}

// {{{ FUNCTIONs create_output_name()/file(), finish_file()

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
    char buffer[strtol(len.c_str(), NULL, 10) + 1];
    mxprints(buffer, format.c_str(), file_num);
    s.replace(p, format.size(), buffer);

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
    render_attachments(out);
    if (kax_chapters != NULL) {
      kax_chapters_void = new EbmlVoid;
      kax_chapters->UpdateSize();
      kax_chapters_void->SetSize(kax_chapters->ElementSize() + 10);
      kax_chapters_void->Render(*out);
    }

    if (kax_tags != NULL) {
      if (!kax_tags->CheckMandatory())
        mxerror("Some tag elements are missing (this error "
                "should not have occured - another similar error should have "
                "occured earlier...).\n");

      kax_tags->UpdateSize();
      tags_size = kax_tags->ElementSize();
    }

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
    mxerror("Couldn't open output file %s (%s).\n", this_outfile.c_str(),
            strerror(errno));
  }
  if (verbose)
    mxinfo("Opened '%s\' for writing.\n", this_outfile.c_str());

  cluster_helper->set_output(out);
  render_headers(out, last_file, first_file);
  render_attachments(out);
  if (kax_chapters != NULL) {
    if (pass == 2) {
      kax_chapters_void = new EbmlVoid;
      kax_chapters_void->SetSize(kax_chapters->ElementSize() + 10);
      kax_chapters_void->Render(*out);
    } else
      kax_chapters->Render(*out);
  }

  if (kax_tags != NULL) {
    if (!kax_tags->CheckMandatory())
      mxerror("Some tag elements are missing (this error "
              "should not have occured - another similar error should have "
              "occured earlier...).\n");

    kax_tags->UpdateSize();
    tags_size = kax_tags->ElementSize();
  }

  file_num++;
}

void finish_file() {
  int i;
  KaxChapters *chapters_here;
  int64_t start, end, offset;

  // Render the cues.
  if (write_cues && cue_writing_requested) {
    if (verbose >= 1)
      mxinfo("Writing cue entries (the index)...");
    kax_cues->Render(*out);
    if (verbose >= 1)
      mxinfo("\n");
  }

  // Now re-render the kax_duration and fill in the biggest timecode
  // as the file's duration.
  out->save_pos(kax_duration->GetElementPosition());
  *(static_cast<EbmlFloat *>(kax_duration)) =
    (cluster_helper->get_max_timecode() -
     cluster_helper->get_first_timecode()) * 1000000.0 / TIMECODE_SCALE;
  kax_duration->Render(*out);
  out->restore_pos();

  if ((kax_chapters != NULL) && (pass > 0)) {
    if (no_linking)
      offset = cluster_helper->get_timecode_offset();
    else
      offset = 0;
    start = cluster_helper->get_first_timecode() + offset;
    end = cluster_helper->get_max_timecode() + offset;

    chapters_here = parse_chapters(chapter_file_name, start, end, offset,
                                   chapter_language, chapter_charset);
    if (chapters_here != NULL)
      kax_chapters_void->ReplaceWith(*chapters_here, *out, true);
    delete kax_chapters_void;
    kax_chapters_void = NULL;
  } else
    chapters_here = NULL;

  // Render the meta seek information with the cues
  if (write_meta_seek_for_clusters && (kax_sh_cues->ListSize() > 0)) {
    kax_sh_cues->UpdateSize();
    kax_sh_cues->Render(*out);
    kax_sh_main->IndexThis(*kax_sh_cues, *kax_segment);
  }

  // Render the tags if we have some.
  if (kax_tags != NULL) {
    kax_tags->UpdateSize();
    kax_tags->Render(*out);
  }

  // Write meta seek information if it is not disabled.
  if (cue_writing_requested)
    kax_sh_main->IndexThis(*kax_cues, *kax_segment);

  if (kax_tags != NULL)
    kax_sh_main->IndexThis(*kax_tags, *kax_segment);

  if (chapters_here != NULL) {
    kax_sh_main->IndexThis(*chapters_here, *kax_segment);
    delete chapters_here;
  } else if ((pass == 0) && (kax_chapters != NULL))
    kax_sh_main->IndexThis(*kax_chapters, *kax_segment);

  if (kax_as != NULL) {
    kax_sh_main->IndexThis(*kax_as, *kax_segment);
    delete kax_as;
    kax_as = NULL;
  }

  if (kax_sh_main->ListSize() > 0) {
    kax_sh_main->UpdateSize();
    if (kax_sh_void->ReplaceWith(*kax_sh_main, *out, true) == 0)
      mxwarn("This should REALLY not have happened. The space reserved for "
             "the first meta seek element was too small. Size needed: %lld. "
             "Please contact Moritz Bunkus at moritz@bunkus.org.\n",
             kax_sh_main->ElementSize());
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
  if (kax_sh_cues != NULL)
    delete kax_sh_cues;

  for (i = 0; i < packetizers.size(); i++)
    packetizers[i]->packetizer->reset();;
}

// }}}

// {{{ FUNCTION main_loop()

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
    mxinfo("\n");
  }
}

// }}}

// {{{ FUNCTION main(int argc, char **argv)

int main(int argc, char **argv) {
  init_globals();

  setup();

  handle_args(argc, argv);

  if (split_after > 0) {
    mxinfo("Pass 1: finding split points. This may take a while.\n\n");

    create_readers();

    if (packetizers.size() == 0)
      mxerror("No streams to output found. Aborting.\n");

    pass = 1;
    create_next_output_file(true, true);
    main_loop();
    finish_file();

    mxinfo("\nPass 2: merging the files. This will take even longer.\n\n");

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

    if (packetizers.size() == 0)
      mxerror("No streams to output found. Aborting.\n");

    pass = 0;
    create_next_output_file(true, true);
    main_loop();
    finish_file();
  }

  cleanup();

  mxexit();
}

// }}}
