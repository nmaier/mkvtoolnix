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
    \version \$Id: mkvmerge.cpp,v 1.8 2003/02/19 11:08:01 mosu Exp $
    \brief command line parameter parsing, looping, output handling
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

#include <iostream>

#ifdef GCC2
#include <typeinfo>
#endif

#include "StdIOCallback.h"

#include "EbmlHead.h"
#include "EbmlSubHead.h"
#include "FileKax.h"
#include "KaxSegment.h"
#include "KaxTracks.h"
#include "KaxTrackEntryData.h"
#include "KaxTrackAudio.h"
#include "KaxTrackVideo.h"
#include "KaxCluster.h"
#include "KaxClusterData.h"
#include "KaxBlock.h"
#include "KaxBlockAdditional.h"
#include "KaxCues.h"

#include "common.h"
#include "queue.h"
#include "r_avi.h"

#ifdef DMALLOC
#include <dmalloc.h>
#endif

using namespace LIBMATROSKA_NAMESPACE;

typedef struct {
  char *ext;
  int   type;
  char *desc;
} file_type_t;

typedef struct filelist_tag {
  char *name;
  FILE *fp;

  int type;

  int status;
  
  packet_t *pack;

  generic_reader_c *reader;

  struct filelist_tag *next;
} filelist_t;

char *outfile = NULL;
filelist_t *input= NULL;
int create_index = 0;
int force_flushing = 0;

float video_fps = -1.0;

KaxSegment     kax_segment;
KaxTracks     *kax_tracks;
KaxTrackEntry *kax_last_entry;
int            track_number = 1;
StdIOCallback *out;

/*int                  idx_num = 0;
int                 *idx_serials = NULL;
int                 *idx_num_entries = NULL;
idx_entry          **idx_entries = NULL;
index_packetizer_c **idx_packetizers = NULL;*/

file_type_t file_types[] =
  {{"---", TYPEUNKNOWN, "<unknown>"},
   {"demultiplexers:", -1, ""},
   {"ogg", TYPEOGM, "general OGG media stream, Vorbis audio embedded in OGG"},
   {"avi", TYPEAVI, "AVI (Audio/Video Interleaved)"},
   {"wav", TYPEWAV, "WAVE (uncompressed PCM)"},
   {"srt", TYPEWAV, "SRT text subtitles"},
   {"   ", TYPEMICRODVD, "MicroDVD text subtitles"},
   {"idx", TYPEVOBSUB, "VobSub subtitles"},
   {"mp3", TYPEMP3, "MPEG1 layer III audio (CBR and VBR/ABR)"},
   {"ac3", TYPEAC3, "A/52 (aka AC3)"},
   {"output modules:", -1, ""},
   {"   ", -1,      "Vorbis audio"},
   {"   ", -1,      "Video (not MPEG1/2)"},
   {"   ", -1,      "uncompressed PCM audio"},
   {"   ", -1,      "text subtitles"},
   {"   ", -1,      "VobSub subtitles"},
   {"   ", -1,      "MP3 audio"},
   {"   ", -1,      "AC3 audio"},
   {NULL,  -1,      NULL}};

static void usage(void) {
  fprintf(stdout,
    "mkvmerge -o out [global options] [options] <file1> [[options] <file2> ...]"
    "\n\n Global options:\n"
    "  -v, --verbose            verbose status\n"
    "  -q, --quiet              suppress status output\n"
    "  -o, --output out         Write to the file 'out'.\n"
    "  -i, --index              Create index for the video streams.\n"
    "\n Options for each input file:\n"
    "  -a, --astreams <n,m,...> Copy the n'th audio stream, NOT the stream with"
  "\n                           the serial number n. Default: copy all audio\n"
    "                           streams.\n"
    "  -d, --vstreams <n,m,...> Copy the n'th video stream, NOT the stream with"
  "\n                           the serial number n. Default: copy all video\n"
    "                           streams.\n"
    "  -t, --tstreams <n,m,...> Copy the n'th text stream, NOT the stream with"
  "\n                           the serial number n. Default: copy all text\n"
    "                           streams.\n"
    "  -A, --noaudio            Don't copy any audio stream from this file.\n"
    "  -D, --novideo            Don't copy any video stream from this file.\n"
    "  -T, --notext             Don't copy any text stream from this file.\n"
    "  -s, --sync <d[,o[/p]]>   Ssynchronize, delay the audio stream by d ms.\n"
    "                           d > 0: Pad with silent samples.\n"
    "                           d < 0: Remove samples from the beginning.\n"
    "                           o/p: Adjust the timestamps by o/p to fix\n"
    "                           linear drifts. p defaults to 1000 if\n"
    "                           omitted. Both o and p can be floating point\n"
    "                           numbers.\n"
    "  -r, --range <s-e>        Only process from start to end. Both values\n"
    "                           take the form 'HH:MM:SS.mmm' or 'SS.mmm',\n"
    "                           e.g. '00:01:00.500' or '60.500'. If one of\n"
    "                           s or e is omitted then it defaults to 0 or\n"
    "                           to end of the file respectively.\n"
    "  -c, --comment 'A=B#C=D'  Set additional comment fields for the\n"
    "                           streams. Sesitive values would be\n"
    "                           'LANGUAGE=English' or 'TITLE=Ally McBeal'.\n"
    "  -f, --fourcc <FOURCC>    Forces the FourCC to the specified value.\n"
    "                           Works only for video streams.\n"
    "\n"
    " Other options:\n"
    "  -l, --list-types         Lists supported input file types.\n"
    "  -h, --help               Show this help.\n"
    "  -V, --version            Show version information.\n"
  );
}

static int get_type(char *filename) {
  FILE *f = fopen(filename, "r");
  u_int64_t size;
  
  if (f == NULL) {
    fprintf(stderr, "Error: could not open source file (%s).\n", filename);
    exit(1);
  }
  if (fseek(f, 0, SEEK_END) != 0) {
    fprintf(stderr, "Error: could not seek to end of file (%s).\n", filename);
    exit(1);
  }
  size = ftell(f);
  if (fseek(f, 0, SEEK_SET) != 0) {
    fprintf(stderr, "Error: could not seek to beginning of file (%s).\n",
            filename);
    exit(1);
  }
  if (avi_reader_c::probe_file(f, size))
    return TYPEAVI;
/*  else if (wav_reader_c::probe_file(f, size))
    return TYPEWAV;
  else if (ogm_reader_c::probe_file(f, size))
    return TYPEOGM;
  else if (srt_reader_c::probe_file(f, size))
    return TYPESRT;
  else if (mp3_reader_c::probe_file(f, size))
    return TYPEMP3;
  else if (ac3_reader_c::probe_file(f, size))
    return TYPEAC3;
  else if (microdvd_reader_c::probe_file(f, size))
    return TYPEMICRODVD;
  else if (vobsub_reader_c::probe_file(f, size)) 
    return TYPEVOBSUB;
  else if (chapter_information_probe(f, size))
    return TYPECHAPTERS;
  else*/
    return TYPEUNKNOWN;
}

static void add_file(filelist_t *file) {
  filelist_t *temp;

  if (input == NULL) {
    input = file;
  } else {
    temp = input;
    while (temp->next) temp = temp->next;
    temp->next = file;
  }
}

static int display_counter = 1;

void display_progress(int force) {
  filelist_t *winner, *current;
  
  if (((display_counter % 500) == 0) || force) {
    display_counter = 0;
    winner = input;
    current = winner->next;
    while (current) {
      if (current->reader->display_priority() >
          winner->reader->display_priority())
        winner = current;
      current = current->next;
    }
    winner->reader->display_progress();
  }
  display_counter++;
}

static unsigned char *parse_streams(char *s) {
  char *c = s;
  char *nstart;
  int n, nstreams;
  unsigned char *streams;
  
  nstart = NULL;
  streams = NULL;
  nstreams = 0;
  while (*c) {
    if ((*c >= '0') && (*c <= '9')) {
      if (nstart == NULL)
        nstart = c;
    } else if (*c == ',') {
      *c = 0;
      if (nstart != NULL) {
        n = atoi(nstart);
        if ((n <= 0) || (n > 255)) {
          fprintf(stderr, "Error: stream number out of range (1..255): %d\n",
                  n);
          exit(1);
        }
        streams = (unsigned char *)realloc(streams, nstreams + 2);
        if (streams == NULL)
          die("malloc");
        streams[nstreams] = (unsigned char)n;
        streams[nstreams + 1] = 0;
        nstart = NULL;
        nstreams++;
      } else
        fprintf(stderr, "Warning: useless use of ','\n");
    } else if (!isspace(*c)) {
      fprintf(stderr, "Error: unrecognized character in stream list: '%c'\n",
              *c);
      exit(1);
    }
    c++;
  }
  
  if (nstart != NULL) {
    n = atoi(nstart);
    if ((n <= 0) || (n > 255)) {
      fprintf(stderr, "Error: stream number out of range (1..255): %d\n",
              n);
      exit(1);
    }
    streams = (unsigned char *)realloc(streams, nstreams + 2);
    if (streams == NULL)
      die("malloc");
    streams[nstreams] = (unsigned char)n;
    streams[nstreams + 1] = 0;
    nstart = NULL;
    nstreams++;
  }
  
  return streams;
}

static void parse_sync(char *s, audio_sync_t *async) {
  char *linear, *div;
  double d1, d2;
  
  if ((linear = strchr(s, ',')) != NULL) {
    *linear = 0;
    linear++;
    div = strchr(linear, '/');
    if (div == NULL)
      async->linear = strtod(linear, NULL) / 1000.0;
    else {
      *div = 0;
      div++;
      d1 = strtod(linear, NULL);
      d2 = strtod(div, NULL);
      if (d2 == 0.0) {
        fprintf(stderr, "Error: linear sync: division by zero?\n");
        exit(1);
      }
      async->linear = d1 / d2;
    }
    if (async->linear <= 0.0) {
      fprintf(stderr, "Error: linear sync value may not be <= 0.\n");
      exit(1);
    }
  } else
    async->linear = 1.0;
  async->displacement = atoi(s);
}

static double parse_time(char *s) {
  char *c, *a, *dot;
  int num_colons;
  double seconds;
  
  dot = strchr(s, '.');
  if (dot != NULL) {
    *dot = 0;
    dot++;
  }
  for (c = s, num_colons = 0; *c; c++) {
    if (*c == ':')
      num_colons++;
    else if ((*c < '0') || (*c > '9')) {
      fprintf(stderr, "ERROR: illegal character '%c' in time range.\n", *c);
      exit(1);
    }
  }
  if (num_colons > 2) {
    fprintf(stderr, "ERROR: illegal time range: %s.\n", s);
    exit(1);
  }
  if (num_colons == 0) {
    seconds = strtod(s, NULL);
    if (dot != NULL)
      seconds += strtod(dot, NULL) / 1000.0;
  }
  for (a = s, c = s, seconds = 0; *c; c++) {
    if (*c == ':') {
      *c = 0;
      seconds *= 60;
      seconds += atoi(a);
      a = c + 1;
    }
  }
  seconds *= 60;
  seconds += atoi(a);
  
  if (dot != NULL)
    seconds += strtod(dot, NULL) / 1000.0;
  
  return seconds;
}

static void parse_range(char *s, range_t *range) {
  char *end;
  
  end = strchr(s, '-');
  if (end != NULL) {
    *end = 0;
    end++;
    range->end = parse_time(end);
  } else
    range->end = 0;
  range->start = parse_time(s);
  if ((range->end != 0) && (range->end < range->start)) {
    fprintf(stderr, "ERROR: end time is set before start time.\n");
    exit(1);
  }
}

void render_head(StdIOCallback *out) {
  EbmlHead head;

  EDocType &doc_type = GetChild<EDocType>(head);
  *static_cast<EbmlString *>(&doc_type) = "matroska";
  EDocTypeVersion &doc_type_ver = GetChild<EDocTypeVersion>(head);
  *(static_cast<EbmlUInteger *>(&doc_type_ver)) = 1;
  EDocTypeReadVersion &doc_type_read_ver =
    GetChild<EDocTypeReadVersion>(head);
  *(static_cast<EbmlUInteger *>(&doc_type_read_ver)) = 1;

  head.Render(static_cast<StdIOCallback &>(*out));
}

static void parse_args(int argc, char **argv) {
  int              i, j;
  int              noaudio, novideo, notext;
  unsigned char   *astreams, *vstreams, *tstreams;
  filelist_t      *file;
  audio_sync_t     async;
  range_t          range;
  char           **comments, *fourcc;
//  vorbis_comment  *chapters;

  noaudio = 0;
  novideo = 0;
  notext  = 0;
  astreams = NULL;
  vstreams = NULL;
  tstreams = NULL;
  memset(&range, 0, sizeof(range_t));
  async.displacement = 0;
  async.linear = 1.0;
  comments = NULL;
  fourcc = NULL;
//  chapters = NULL;
  for (i = 1; i < argc; i++)
    if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--version")) {
      fprintf(stdout, "mkvmerge v" VERSION "\n");
      exit(0);
    } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: -o lacks a file name.\n");
        exit(1);
      } else if (outfile != NULL) {
        fprintf(stderr, "Error: only one output file allowed.\n");
        exit(1);
      }
      outfile = (char *)strdup(argv[i + 1]);
      i++;
    }

  if (outfile == NULL) {
    fprintf(stderr, "Error: no output files given.\n");
    exit(1);
  }

  /* open output file */
  try {
    out = new StdIOCallback(outfile, MODE_CREATE);
  } catch (std::exception &ex) {
    fprintf(stderr, "Error: Couldn't open output file %s (%s).\n", outfile,
            strerror(errno));
    exit(1);
  }
  try {
    render_head(out);
    kax_segment.Render(static_cast<StdIOCallback &>(*out));
  } catch (std::exception &ex) {
    fprintf(stderr, "Error: Could not render the file header.\n");
    exit(1);
  }

  kax_tracks = &GetChild<KaxTracks>(kax_segment);
  kax_last_entry = NULL;
  
  for (i = 1; i < argc; i++) {
    if (!strcmp(argv[i], "-q"))
      verbose = 0;
    else if (!strcmp(argv[i], "-v") || !strcmp(argv[i], "--verbose"))
      verbose = 2;
    else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "-?") ||
             !strcmp(argv[i], "--help")) {
      usage();
      exit(0);
    } else if (!strcmp(argv[i], "-o") || !strcmp(argv[i], "--output"))
      i++;
    else if (!strcmp(argv[i], "-l") || !strcmp(argv[i], "--list-types")) {
      fprintf(stdout, "Known file types:\n  ext  description\n" \
                                         "  ---  --------------------------\n");
      for (j = 1; file_types[j].ext; j++)
        fprintf(stdout, "  %s  %s\n", file_types[j].ext, file_types[j].desc);
      exit(0);
    } else if (!strcmp(argv[i], "-A") || !strcmp(argv[i], "--noaudio"))
      noaudio = 1;
    else if (!strcmp(argv[i], "-D") || !strcmp(argv[i], "--novideo"))
      novideo = 1;
    else if (!strcmp(argv[i], "-T") || !strcmp(argv[i], "--notext"))
      notext = 1;
    else if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "--astreams")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: -a lacks the stream number(s).\n");
        exit(1);
      }
      if (astreams != NULL)
        free(astreams);
      astreams = parse_streams(argv[i + 1]);
      i++;
    } else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--vstreams")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: -d lacks the stream number(s).\n");
        exit(1);
      }
      if (astreams != NULL)
        free(vstreams);
      vstreams = parse_streams(argv[i + 1]);
      i++;
    } else if (!strcmp(argv[i], "-t") || !strcmp(argv[i], "--tstreams")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: -t lacks the stream number(s).\n");
        exit(1);
      }
      if (tstreams != NULL)
        free(tstreams);
      tstreams = parse_streams(argv[i + 1]);
      i++;
/*    } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "--comments")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: -c lacks the comments.\n");
        exit(1);
      }
      if (argv[i + 1][0] == '@')
        comments = append_comments_from_file(argv[i + 1], comments);
      else
        comments = unpack_append_comments(argv[i + 1], comments);
#ifdef DEBUG
      dump_comments(comments);
#endif // DEBUG
      i++;*/
    } else if (!strcmp(argv[i], "-f") || !strcmp(argv[i], "--fourcc")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: -f lacks the FourCC.\n");
        exit(1);
      }
      fourcc = argv[i + 1];
      if (strlen(fourcc) != 4) {
        fprintf(stderr, "Error: The FourCC must be exactly four chars long.\n");
        exit(1);
      }
      i++;
    } else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--sync")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: -s lacks the audio delay.\n");
        exit(1);
      }
      parse_sync(argv[i + 1], &async);
      i++;
    } else if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "--range")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: -r lacks the range.\n");
        exit(1);
      }
      parse_range(argv[i + 1], &range);
      i++;
    } else if (!strcmp(argv[i], "-i") || !strcmp(argv[i], "--index"))
      create_index = 1;
    else {
      if ((astreams != NULL) && noaudio) {
        fprintf(stderr, "Error: -A and -a used on the same source file.\n");
        exit(1);
      }
      if ((vstreams != NULL) && novideo) {
        fprintf(stderr, "Error: -D and -d used on the same source file.\n");
        exit(1);
      }
      if ((tstreams != NULL) && notext) {
        fprintf(stderr, "Error: -T and -t used on the same source file.\n");
        exit(1);
      }
      if (noaudio) {
        astreams = (unsigned char *)malloc(1);
        if (astreams == NULL)
          die("malloc");
        *astreams = 0;
      }
      if (novideo) {
        vstreams = (unsigned char *)malloc(1);
        if (vstreams == NULL)
          die("malloc");
        *vstreams = 0;
      }
      if (notext) {
        tstreams = (unsigned char *)malloc(1);
        if (tstreams == NULL)
          die("malloc");
        *tstreams = 0;
      }
      file = (filelist_t *)malloc(sizeof(filelist_t));
      if (file == NULL)
        die("malloc");

      file->name = argv[i];
      file->type = get_type(file->name);

      if (file->type == TYPEUNKNOWN) {
        fprintf(stderr, "Error: File %s has unknown type. Please have a look "
                "at the supported file types ('mkvmerge --list-types') and "
                "contact me at moritz@bunkus.org if your file type is "
                "supported but not recognized properly.\n", file->name);
        exit(1);
      }

      file->fp = NULL;
      try {
        switch (file->type) {
/*          case TYPEOGM:
            file->reader = new ogm_reader_c(file->name, astreams, vstreams, 
                                            tstreams, &async, &range,
                                            comments, fourcc);
            break;*/
          case TYPEAVI:
            if (tstreams != NULL)
              fprintf(stderr, "Warning: -t/-T are ignored for AVI files.\n");
            file->reader = new avi_reader_c(file->name, astreams, vstreams,
                                            &async, &range, fourcc);
            break;
/*          case TYPEWAV:
            if ((astreams != NULL) || (vstreams != NULL) ||
                (tstreams != NULL))
              fprintf(stderr, "Warning: -a/-A/-d/-D/-t/-T are ignored for " \
                      "WAVE files.\n");
            file->reader = new wav_reader_c(file->name, &async, &range,
                                            comments);
            break;
          case TYPESRT:
            if ((astreams != NULL) || (vstreams != NULL) ||
                (tstreams != NULL))
              fprintf(stderr, "Warning: -a/-A/-d/-D/-t/-T are ignored for " \
                      "SRT files.\n");
            file->reader = new srt_reader_c(file->name, &async, &range,
                                            comments);
            break;
          case TYPEMP3:
            if ((astreams != NULL) || (vstreams != NULL) ||
                (tstreams != NULL))
              fprintf(stderr, "Warning: -a/-A/-d/-D/-t/-T are ignored for " \
                      "MP3 files.\n");
            file->reader = new mp3_reader_c(file->name, &async, &range,
                                            comments);
            break;
          case TYPEAC3:
            if ((astreams != NULL) || (vstreams != NULL) ||
                (tstreams != NULL))
              fprintf(stderr, "Warning: -a/-A/-d/-D/-t/-T are ignored for " \
                      "AC3 files.\n");
            file->reader = new ac3_reader_c(file->name, &async, &range,
                                            comments);
            break;
          case TYPECHAPTERS:
            if (chapters != NULL) {
              fprintf(stderr, "Error: only one chapter file allowed.\n");
              exit(1);
            }
            chapters = chapter_information_read(file->name);
            break;
          case TYPEMICRODVD:
            if ((astreams != NULL) || (vstreams != NULL) ||
                (tstreams != NULL))
              fprintf(stderr, "Warning: -a/-A/-d/-D/-t/-T are ignored for " \
                      "MicroDVD files.\n");
            file->reader = new microdvd_reader_c(file->name, &async, &range,
                                                 comments);
            break;
          case TYPEVOBSUB:
            if ((astreams != NULL) || (vstreams != NULL) ||
                (tstreams != NULL))
              fprintf(stderr, "Warning: -a/-A/-d/-D/-t/-T are ignored for " \
                      "VobSub files.\n");
            file->reader = new vobsub_reader_c(file->name, &async, &range,
                                               comments);
            break;*/
          default:
            fprintf(stderr, "EVIL internal bug! (unknown file type)\n");
            exit(1);
            break;
        }
      } catch (error_c error) {
        fprintf(stderr, "Demultiplexer failed to initialize:\n%s\n",
                error.get_error());
        exit(1);
      }
      if (file->type != TYPECHAPTERS) {
        file->status = EMOREDATA;
        file->next = NULL;
        file->pack = NULL;

        add_file(file);
      } else
        free(file);

//      free_comments(comments);
//      comments = NULL;      
      fourcc = NULL;
      noaudio = 0;
      novideo = 0;
      notext = 0;
      if (astreams != NULL) {
        free(astreams);
        astreams = NULL;
      }
      if (vstreams != NULL) {
        free(vstreams);
        vstreams = NULL;
      }
      async.displacement = 0;
      async.linear = 1.0;
      memset(&range, 0, sizeof(range_t));
    }
  }
  
  if (input == NULL) {
    usage();
    exit(1);
  }
  try {
    kax_tracks->Render(static_cast<StdIOCallback &>(*out));
  } catch (std::exception &ex) {
    fprintf(stderr, "Error: Could not render the track headers.\n");
    exit(1);
  }
/*  if (chapters != NULL) {
    file = input;
    while (file != NULL) {
      file->reader->set_chapter_info(chapters);
      file = file->next;
    }
    vorbis_comment_clear(chapters);
    free(chapters);
    chapters = NULL;
  }*/
}

/*void add_index(int serial) {
  int i, found = -1;
  
  if (!create_index)
    return;

  for (i = 0; i < idx_num; i++)
    if (idx_serials[i] == serial) {
      found = i;
      break;
    }

  if (found == -1) {
    idx_serials = (int *)realloc(idx_serials, (idx_num + 1) * sizeof(int));
    if (idx_serials == NULL)
      die("realloc");
    idx_num_entries = (int *)realloc(idx_num_entries, (idx_num + 1) *
                                     sizeof(int));
    if (idx_num_entries == NULL)
      die("realloc");
    idx_packetizers = (index_packetizer_c **)
      realloc(idx_packetizers, (idx_num + 1) * sizeof(index_packetizer_c));
    if (idx_packetizers == NULL)
      die("realloc");
    idx_entries = (idx_entry **)realloc(idx_entries, (idx_num + 1) *
                                        sizeof(idx_entry));
    if (idx_entries == NULL)
      die("realloc");
    i = idx_num;
    idx_serials[i] = serial;
    idx_num_entries[i] = 0;
    idx_entries[i] = NULL;
    idx_num++;
    try {
      idx_packetizers[i] = new index_packetizer_c(serial);
    } catch (error_c error) {
      
    }
  }
}

void add_to_index(int serial, ogg_int64_t granulepos, int64_t filepos) {
  int i, found = -1;
  
  for (i = 0; i < idx_num; i++)
    if (idx_serials[i] == serial) {
      found = i;
      break;
    }

  if (found == -1) {
    fprintf(stderr, "Internal error: add_to_index for a serial without " \
            "add_index for that serial.\n");
    exit(1);
  }

  idx_entries[i] = (idx_entry *)realloc(idx_entries[i], sizeof(idx_entry) *
                                        (idx_num_entries[i] + 1));
  if (idx_entries[i] == NULL)
    die("realloc");

  idx_entries[i][idx_num_entries[i]].granulepos = granulepos;
  idx_entries[i][idx_num_entries[i]].filepos = filepos;
  idx_num_entries[i]++;
}*/

int write_packet(packet_t *pack, filelist_t *file, StdIOCallback *out) {
  KaxCluster cluster;
  KaxCues dummy_cues;
  KaxClusterTimecode &timecode = GetChild<KaxClusterTimecode>(cluster);
  *(static_cast<EbmlUInteger *>(&timecode)) = 0;
  
  KaxBlockGroup &block_group = GetChild<KaxBlockGroup>(cluster);
  KaxBlock &block = GetChild<KaxBlock>(block_group);
  DataBuffer data = DataBuffer((binary *)pack->data, pack->length);
  KaxTrackEntry &track_entry =
    static_cast<KaxTrackEntry &>(*pack->source->track_entry);
  block.AddFrame(track_entry, pack->timestamp, data);

  cluster.Render(static_cast<StdIOCallback &>(*out), dummy_cues);

  if (verbose > 1)
    fprintf(stdout, "timestamp %llu, size %d, written packet for %s (%s)\n",
            pack->timestamp, pack->length, file->name,
            typeid(*pack->source).name());

  free(pack->data);
  free(pack);
  
  return 0;
}

int main(int argc, char **argv) {
  filelist_t *file, *winner;
  packet_t *pack;
  int result;

  nice(2);

  parse_args(argc, argv);
  
  /* let her rip! */
  while (1) {
    /* Step 1: make sure an ogg page is available for each input 
    ** as long we havne't already processed the last one
    */
    file = input;
    while (file) {
      if (file->status == EMOREDATA)
        file->status = file->reader->read();
      file = file->next;
    }

    file = input;
    while (file) {
      if (file->pack == NULL)
        file->pack = file->reader->get_packet();
      file = file->next;
    }

    /* Step 2: Pick the page with the lowest timestamp and 
    ** stuff it into the Matroska file
    */
    file = input;
    winner = file;
    file = file->next;
    while (file) {
      if (file->pack != NULL) {
        if (winner->pack == NULL)
          winner = file;
        else if (file->pack &&
                 (file->pack->timestamp < winner->pack->timestamp))
          winner = file;
      }
      file = file->next;
    }
    if (winner->pack != NULL)
      pack = winner->pack;
    else /* exit if there are no more packets */
      break;

    /* Step 3: Write out the winning packet */
/*    if (create_index && (mpage->index_serial != -1))
      add_to_index(mpage->index_serial, ogg_page_granulepos(page), ftell(out));
    if ((result = write_ogg_page(mpage, "", file)) != 0)
      exit(result);*/
    if ((result = write_packet(pack, winner, out)) != 0)
      exit(result);
    
    winner->pack = NULL;
    
    /* display some progress information */
    if (verbose >= 1)
      display_progress(0);
  }

  if (verbose == 1) {
    display_progress(1);
    fprintf(stdout, "\n");
  }
  
  file = input;
  while (file) {
    filelist_t *next = file->next;
    if (file->reader)
      delete file->reader;
    free(file);
    file = next;
  }
  
/*  if (create_index) {
    for (i = 0; i < idx_num; i++) {
      fprintf(stdout, "serial %d num_entries %d size %d\n", idx_serials[i],
              idx_num_entries[i], idx_num_entries[i] * sizeof(idx_entry));
      idx_packetizers[i]->process(&idx_entries[i][0], idx_num_entries[i]);
      while ((mpage = idx_packetizers[i]->get_page()) != NULL)
        if ((result = write_ogg_page(mpage, "", NULL)) != 0)
          exit(result);
      delete idx_packetizers[i];
    }
  }*/

  delete out;

  return 0;
}
