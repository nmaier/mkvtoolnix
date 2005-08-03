/*
   mkvextract -- extract tracks from Matroska files into other files

   Distributed under the GPL
   see the file COPYING for details
   or visit http://www.gnu.org/copyleft/gpl.html

   $Id$

   command line parsing, setup

   Written by Moritz Bunkus <moritz@bunkus.org>.
*/

#include "os.h"

#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#if defined(COMP_MSC)
#include <assert.h>
#else
#include <unistd.h>
#endif

#include <iostream>
#include <string>
#include <vector>

#include <ebml/EbmlHead.h>
#include <ebml/EbmlSubHead.h>
#include <ebml/EbmlStream.h>
#include <ebml/EbmlVoid.h>
#include <matroska/FileKax.h>

#include <matroska/KaxAttached.h>
#include <matroska/KaxAttachments.h>
#include <matroska/KaxBlock.h>
#include <matroska/KaxBlockData.h>
#include <matroska/KaxChapters.h>
#include <matroska/KaxCluster.h>
#include <matroska/KaxClusterData.h>
#include <matroska/KaxCues.h>
#include <matroska/KaxCuesData.h>
#include <matroska/KaxInfo.h>
#include <matroska/KaxInfoData.h>
#include <matroska/KaxSeekHead.h>
#include <matroska/KaxSegment.h>
#include <matroska/KaxTags.h>
#include <matroska/KaxTracks.h>
#include <matroska/KaxTrackEntryData.h>
#include <matroska/KaxTrackAudio.h>
#include <matroska/KaxTrackVideo.h>

#include "chapters.h"
#include "common.h"
#include "matroska.h"
#include "mkvextract.h"
#include "mm_io.h"
#include "tagwriter.h"
#include "xml_element_mapping.h"

using namespace libmatroska;
using namespace std;

#define NAME "mkvextract"

#define MODE_TRACKS      0
#define MODE_TAGS        1
#define MODE_ATTACHMENTS 2
#define MODE_CHAPTERS    3
#define MODE_CUESHEET    4

bool no_variable_data = false;

void
usage() {
  const char *usage_infos = N_(
"Usage: mkvextract tracks <inname> [options] [TID1:out1 [TID2:out2 ...]]\n"
"   or  mkvextract tags <inname> [options]\n"
"   or  mkvextract attachments <inname> [options] [AID1:out1 [AID2:out2 ...]]"
"\n   or  mkvextract chapters <inname> [options]\n"
"   or  mkvextract cuesheet <inname> [options]\n"
"   or  mkvextract <-h|-V>\n"
"\n"
" The first word tells mkvextract what to extract. The second must be the\n"
" source file. The only 'global' option that can be used with all modes is\n"
" '-v' or '--verbose' to increase the verbosity. All other options depend\n"
" on the mode.\n"
"\n"
" The first mode extracts some tracks to external files.\n"
"  -c charset       Convert text subtitles to this charset (default: UTF-8).\n"
"  --no-ogg         Write raw FLAC files (default: write OggFLAC files).\n"
"  --cuesheet       Also try to extract the CUE sheet from the chapter\n"
"                   information and tags for this track.\n"
"  --blockadd level Keep only the BlockAdditions up to this level\n"
"                   (default: keep all levels)\n"
"  --raw            Extract the data to a raw file.\n"
"  --fullraw        Extract the data to a raw file including the CodecPrivate"
"\n                   as a header.\n"
"  TID:out          Write track with the ID TID to the file 'out'.\n"
"\n"
" Example:\n"
" mkvextract tracks \"a movie.mkv\" 2:audio.ogg -c ISO8859-1 3:subs.srt\n"
"\n"
" The second mode extracts the tags and converts them to XML. The output is\n"
" written to the standard output. The output can be used as a source\n"
" for mkvmerge.\n"
"\n"
" Example:\n"
" mkvextract tags \"a movie.mkv\" > movie_tags.xml\n"
"\n"
" The third mode extracts attachments from the source file.\n"
"  AID:outname    Write the attachment with the ID 'AID' to 'outname'.\n"
"\n"
" Example:\n"
" mkvextract attachments \"a movie.mkv\" 4:cover.jpg\n"
"\n"
" The fourth mode extracts the chapters and converts them to XML. The\n"
" output is written to the standard output. The output can be used as a\n"
" source for mkvmerge.\n"
"  -s, --simple   Exports the chapter infomartion in the simple format\n"
"                 used in OGM tools (CHAPTER01=... CHAPTER01NAME=...).\n"
"\n"
" Example:\n"
" mkvextract chapters \"a movie.mkv\" > movie_chapters.xml\n"
"\n"
" The fifth mode tries to extract chapter information and tags and outputs\n"
" them as a CUE sheet. This is the reverse of using a CUE sheet with\n"
" mkvmerge's '--chapters' option.\n"
"\n"
" Example:\n"
" mkvextract cuesheet \"audiofile.mka\" > audiofile.cue\n"
"\n"
" These options can be used instead of the mode keyword to obtain\n"
" further information:\n"
"  -v, --verbose  Increase verbosity.\n"
"  -h, --help     Show this help.\n"
"  -V, --version  Show version information.\n");

  mxinfo(_(usage_infos));
}

static bool chapter_format_simple = false;
static bool parse_fully = false;

void
parse_args(vector<string> args,
           string &file_name,
           int &mode,
           vector<track_spec_t> &tracks) {
  int i;
  char *colon, *copy;
  const char *sub_charset;
  int64_t tid;
  track_spec_t track;
  bool embed_in_ogg, extract_cuesheet;
  int extract_raw;
  int extract_blockadd_level = -1;

  file_name = "";
  sub_charset = NULL;
  verbose = 0;

  if (args.size() < 1) {
    usage();
    mxexit(0);
  }

  if ((args[0] == "-V") || (args[0] == "--version")) {
    mxinfo("mkvextract v" VERSION " ('" VERSIONNAME "')\n");
    mxexit(0);

  } else if ((args[0] == "-h") || (args[0] == "-?") ||
             (args[0] == "--help")) {
    usage();
    mxexit(0);
  }

  if ((args[0] == "tracks"))
    mode = MODE_TRACKS;
  else if ((args[0] == "tags"))
    mode = MODE_TAGS;
  else if ((args[0] == "attachments"))
    mode = MODE_ATTACHMENTS;
  else if ((args[0] == "chapters"))
    mode = MODE_CHAPTERS;
  else if ((args[0] == "cuesheet"))
    mode = MODE_CUESHEET;
  else
    mxerror(_("Unknown mode '%s'.\n"), args[0].c_str());

  if (args.size() < 2) {
    usage();
    mxexit(0);
  }

  file_name = args[1];

  sub_charset = "UTF-8";
  embed_in_ogg = true;
  extract_cuesheet = false;
  extract_raw = 0;

  // Now process all the other options.
  for (i = 2; i < args.size(); i++)
    if ((args[i] == "-v") || (args[i] == "--verbose"))
      verbose++;

    else if ((args[i] == "--no-variable-data"))
      no_variable_data = true;

    else if ((args[i] == "-f") || (args[i] == "--parse-fully"))
      parse_fully = true;

    else if ((args[i] == "-c")) {
      if (mode != MODE_TRACKS)
        mxerror(_("'-c' is only allowed when extracting tracks.\n"));

      if ((i + 1) >= args.size())
        mxerror(_("'-c' lacks a charset.\n"));

      sub_charset = args[i + 1].c_str();
      i++;

    } else if ((args[i] == "--no-ogg")) {
      if (mode != MODE_TRACKS)
        mxerror(_("'--no-ogg' is only allowed when extracting tracks.\n"));
      embed_in_ogg = false;

    } else if ((args[i] == "--cuesheet")) {
      if (mode != MODE_TRACKS)
        mxerror(_("'--cuesheet' is only allowed when extracting tracks.\n"));
      extract_cuesheet = true;

    } else if ((args[i] == "--blockadd")) {
      if (mode != MODE_TRACKS)
        mxerror(_("'--blockadd' is only allowed when extracting tracks.\n"));

      if ((i + 1) >= args.size())
        mxerror(_("'--blockadd' lacks a level.\n"));

      if (!parse_int(args[i + 1], extract_blockadd_level) ||
          (extract_blockadd_level < -1))
        mxerror("Invalid BlockAddition level in argument '%s'.\n",
                args[i + 1].c_str());
      i++;

    } else if ((args[i] == "--raw")) {
      if (mode != MODE_TRACKS)
        mxerror(_("'--raw' is only allowed when extracting tracks.\n"));
      extract_raw = 1;

    } else if ((args[i] == "--fullraw")) {
      if (mode != MODE_TRACKS)
        mxerror(_("'--fullraw' is only allowed when extracting tracks.\n"));
      extract_raw = 2;

   } else if (mode == MODE_TAGS)
      mxerror(_("No further options allowed when extracting %s.\n"),
              args[0].c_str());

    else if (mode == MODE_CUESHEET)
      mxerror(_("No further options allowed when regenerating the CUE "
                "sheet.\n"));

    else if ((args[i] == "-s") || (args[i] == "--simple")) {
      if (mode != MODE_CHAPTERS)
        mxerror(_("'%s' is only allowed for chapter extraction.\n"),
                args[i].c_str());

      chapter_format_simple = true;

    } else if ((mode == MODE_TRACKS) || (mode == MODE_ATTACHMENTS)) {
      copy = safestrdup(args[i].c_str());
      colon = strchr(copy, ':');
      if (colon == NULL)
        mxerror(mode == MODE_TRACKS ?
                _("Missing track ID in argument '%s'.\n") :
                _("Missing attachment ID in argument '%s'.\n"),
                args[i].c_str());

      *colon = 0;
      if (!parse_int(copy, tid) || (tid < 0))
        mxerror("Invalid %s ID in argument '%s'.\n",
                mode == MODE_TRACKS ?
                _("Invalid track ID in argument '%s'.\n") :
                _("Invalid attachment ID in argument '%s'.\n"),
                args[i].c_str());

      colon++;
      if (*colon == 0)
        mxerror(_("Missing output file name in argument '%s'.\n"),
                args[i].c_str());

      memset(&track, 0, sizeof(track_spec_t));
      track.tid = tid;
      track.out_name = safestrdup(colon);
      track.sub_charset = safestrdup(sub_charset);
      track.embed_in_ogg = embed_in_ogg;
      track.extract_cuesheet = extract_cuesheet;
      track.extract_blockadd_level = extract_blockadd_level;
      track.extract_raw = extract_raw;
      tracks.push_back(track);
      safefree(copy);
      sub_charset = "UTF-8";
      embed_in_ogg = true;
      extract_cuesheet = false;
      extract_raw = 0;

    } else
      mxerror(_("Unrecognized command line option '%s'. Maybe you put a "
                "mode specific option before the input file name?\n"),
              args[i].c_str());

  if ((mode == MODE_TAGS) || (mode == MODE_CHAPTERS) ||
      (mode == MODE_CUESHEET))
    return;

  if (tracks.size() == 0) {
    mxinfo(_("Nothing to do.\n\n"));
    usage();
    mxexit(0);
  }
}

#define ARGS_BUFFER_LEN (200 * 1024) // Ok let's be ridiculous here :)
static char args_buffer[ARGS_BUFFER_LEN];

void
show_element(EbmlElement *l,
             int level,
             const char *fmt,
             ...) {
  va_list ap;
  char level_buffer[10];
  string new_fmt;

  if (level > 9)
    die("mkvextract.cpp/show_element(): level > 9: %d", level);

  if (verbose == 0)
    return;

  fix_format(fmt, new_fmt);
  va_start(ap, fmt);
  args_buffer[ARGS_BUFFER_LEN - 1] = 0;
  vsnprintf(args_buffer, ARGS_BUFFER_LEN - 1, new_fmt.c_str(), ap);
  va_end(ap);

  memset(&level_buffer[1], ' ', 9);
  level_buffer[0] = '|';
  level_buffer[level] = 0;
  mxinfo("(%s) %s+ %s", NAME, level_buffer, args_buffer);
  if (l != NULL)
    mxinfo(_(" at %llu"), l->GetElementPosition());
  mxinfo("\n");
}

void
show_error(const char *fmt,
           ...) {
  va_list ap;
  string new_fmt;

  fix_format(fmt, new_fmt);
  va_start(ap, fmt);
  args_buffer[ARGS_BUFFER_LEN - 1] = 0;
  vsnprintf(args_buffer, ARGS_BUFFER_LEN - 1, new_fmt.c_str(), ap);
  va_end(ap);

  mxinfo("(%s) %s\n", NAME, args_buffer);
}

int
main(int argc,
     char **argv) {
  string input_file;
  int mode;
  vector<track_spec_t> tracks;

  init_mm_stdio();

#if defined(SYS_UNIX)
  nice(2);
#endif

#if defined(HAVE_LIBINTL_H)
  if (setlocale(LC_MESSAGES, "") == NULL)
    mxerror("The locale could not be set properly. Check the "
            "LANG, LC_ALL and LC_MESSAGES environment variables.\n");
  bindtextdomain("mkvtoolnix", MTX_LOCALE_DIR);
  textdomain("mkvtoolnix");
#endif

  mm_file_io_c::setup();
  srand(time(NULL));
  utf8_init("");

  xml_element_map_init();

  parse_args(command_line_utf8(argc, argv), input_file, mode, tracks);
  if (mode == MODE_TRACKS) {
    extract_tracks(input_file.c_str(), tracks);

    if (verbose == 0)
      mxinfo(_("progress: 100%%\n"));

  } else if (mode == MODE_TAGS)
    extract_tags(input_file.c_str(), parse_fully);

  else if (mode == MODE_ATTACHMENTS)
    extract_attachments(input_file.c_str(), tracks, parse_fully);

  else if (mode == MODE_CHAPTERS)
    extract_chapters(input_file.c_str(), chapter_format_simple, parse_fully);

  else if (mode == MODE_CUESHEET)
    extract_cuesheet(input_file.c_str(), parse_fully);

  else
    die("mkvextract: Unknown mode!?");

  utf8_done();

  return 0;

}
