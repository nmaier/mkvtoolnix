/*
  mkvextract -- extract tracks from Matroska files into other files

  mkvextract.cpp

  Written by Moritz Bunkus <moritz@bunkus.org>

  Distributed under the GPL
  see the file COPYING for details
  or visit http://www.gnu.org/copyleft/gpl.html
*/

/*!
    \file
    \version $Id$
    \brief extracts tracks and other items from Matroska files into other files
    \author Moritz Bunkus <moritz@bunkus.org>
*/

// {{{ includes

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

using namespace libmatroska;
using namespace std;

// }}}

// {{{ #defines, structs, global variables

#define NAME "mkvextract"

#define MODE_TRACKS      0
#define MODE_TAGS        1
#define MODE_ATTACHMENTS 2
#define MODE_CHAPTERS    3

vector<kax_track_t> tracks;

bool ssa_line_c::operator < (const ssa_line_c &cmp) const {
  return num < cmp.num;
}

// }}}

// {{{ FUNCTIONS find_track(), usage()

kax_track_t *find_track(int tid) {
  int i;

  for (i = 0; i < tracks.size(); i++)
    if (tracks[i].tid == tid)
      return &tracks[i];

  return NULL;
}

char typenames[14][20] = {"unknown", "Ogg", "AVI", "WAV", "SRT", "MP3", "AC3",
                          "chapter", "MicroDVD", "VobSub", "Matroska", "DTS",
                          "AAC", "SSA/ASS"};

void usage() {
  const char *usage_infos =
"Usage: mkvextract tracks <inname> [options] [TID1:out1 [TID2:out2 ...]]\n"
"   or  mkvextract tags <inname> [options]\n"
"   or  mkvextract attachments <inname> [options] [AID1:out1 [AID2:out2 ...]]"
"\n   or  mkvextract chapters <inname> [options]\n"
"   or  mkvextract <-h|-V>\n"
"\n"
" The first word tells mkvextract what to extract. The second must be the\n"
" source file. The only 'global' option that can be used with all modes is\n"
" '-v' or '--verbose' to increase the verbosity. All other options depend\n"
" on the mode.\n"
"\n"
" First mode extracts some tracks to external files.\n"
"  -c charset     Convert text subtitles to this charset.\n"
"  TID:out        Write track with the ID TID to the file 'out'.\n"
"\n"
" Second mode extracts the tags and converts them to XML. The output is\n"
" written to the standard output. The output can be used as a source\n"
" for mkvmerge.\n"
"\n"
" Third mode extracts attachments from inname.\n"
"  AID:outname    Write the attachment with the ID AID to 'outname'.\n"
"\n"
" Fourth mode extracts the chapters and converts them to XML. The output is\n"
" written to the standard output. The output can be used as a source\n"
" for mkvmerge.\n"
"  -s, --simple   Exports the chapter infomartion in the simple format\n"
"                 used in OGM tools (CHAPTER01=... CHAPTER01NAME=...).\n"
"\n"
" These options can be used instead of the mode keyword to obtain\n"
" further information:\n"
"  -v, --verbose  Increase verbosity.\n"
"  -h, --help     Show this help.\n"
"  -V, --version  Show version information.\n";

  mxinfo(usage_infos);
}

// }}}

// {{{ FUNCTION parse_args

static bool chapter_format_simple = false;

void parse_args(int argc, char **argv, char *&file_name, int &mode) {
  int i, conv_handle;
  char *colon, *copy;
  int64_t tid;
  kax_track_t track;

  file_name = NULL;
  verbose = 0;

  if (argc < 2) {
    usage();
    mxexit(0);
  }

  if (!strcmp(argv[1], "-V") || !strcmp(argv[1], "--version")) {
    mxinfo("mkvextract v" VERSION "\n");
    mxexit(0);

  } else if (!strcmp(argv[1], "-h") || !strcmp(argv[1], "-?") ||
             !strcmp(argv[1], "--help")) {
    usage();
    mxexit(0);
  }

  if (!strcmp(argv[1], "tracks"))
    mode = MODE_TRACKS;
  else if (!strcmp(argv[1], "tags"))
    mode = MODE_TAGS;
  else if (!strcmp(argv[1], "attachments"))
    mode = MODE_ATTACHMENTS;
  else if (!strcmp(argv[1], "chapters"))
    mode = MODE_CHAPTERS;
  else
    mxerror("Unknown mode '%s'.\n", argv[1]);

  if (argc < 3) {
    usage();
    mxexit(0);
  }

  file_name = argv[2];

  conv_handle = 0;

  // Now process all the other options.
  for (i = 3; i < argc; i++)
    if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose"))
      verbose++;
    else if (!strcmp(argv[i], "-c")) {
      if (mode != MODE_TRACKS)
        mxerror("-c is only allowed when extracting tracks.\n");

      if ((i + 1) >= argc)
        mxerror("-c lacks a charset.\n");

      conv_handle = utf8_init(argv[i + 1]);
      i++;

    } else if (mode == MODE_TAGS)
      mxerror("No further options allowed when extracting %s.\n", argv[1]);

    else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--simple")) {
      if (mode != MODE_CHAPTERS)
        mxerror("%s is only allowed for chapter extraction.\n", argv[i]);

      chapter_format_simple = true;

    } else {
      copy = safestrdup(argv[i]);
      colon = strchr(copy, ':');
      if (colon == NULL)
        mxerror("Missing %s ID in argument '%s'.\n",
                mode == MODE_TRACKS ? "track" : "attachment", argv[i]);

      *colon = 0;
      if (!parse_int(copy, tid) || (tid < 0))
        mxerror("Invalid %s ID in argument '%s'.\n",
                mode == MODE_TRACKS ? "track" : "attachment", argv[i]);

      colon++;
      if (*colon == 0)
        mxerror("Missing output file name in argument '%s'.\n", argv[i]);

      memset(&track, 0, sizeof(kax_track_t));
      track.tid = tid;
      track.out_name = safestrdup(colon);
      track.conv_handle = conv_handle;
      tracks.push_back(track);
      safefree(copy);
    }

  if ((mode == MODE_TAGS) || (mode == MODE_CHAPTERS))
    return;

  if (tracks.size() == 0) {
    mxinfo("Nothing to do.\n\n");
    usage();
    mxexit(0);
  }
}

// }}}

// {{{ FUNCTIONS show_element(), show_error()

#define ARGS_BUFFER_LEN (200 * 1024) // Ok let's be ridiculous here :)
static char args_buffer[ARGS_BUFFER_LEN];

void show_element(EbmlElement *l, int level, const char *fmt, ...) {
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
    mxinfo(" at %llu", l->GetElementPosition());
  mxinfo("\n");
}

void show_error(const char *fmt, ...) {
  va_list ap;
  string new_fmt;

  fix_format(fmt, new_fmt);
  va_start(ap, fmt);
  args_buffer[ARGS_BUFFER_LEN - 1] = 0;
  vsnprintf(args_buffer, ARGS_BUFFER_LEN - 1, new_fmt.c_str(), ap);
  va_end(ap);

  mxinfo("(%s) %s\n", NAME, args_buffer);
}

// }}}

// {{{ FUNCTION main()

int main(int argc, char **argv) {
  char *input_file;
  int mode;

#if defined(SYS_UNIX)
  nice(2);
#endif

  utf8_init(NULL);

  parse_args(argc, argv, input_file, mode);
  if (mode == MODE_TRACKS) {
    extract_tracks(input_file);

    if (verbose == 0)
      mxinfo("progress: 100%%\n");

  } else if (mode == MODE_TAGS)
    extract_tags(input_file);

  else if (mode == MODE_ATTACHMENTS)
    extract_attachments(input_file);

  else if (mode == MODE_CHAPTERS)
    extract_chapters(input_file, chapter_format_simple);

  else
    die("mkvextract: Unknown mode!?");

  utf8_done();

  return 0;

}

// }}}
