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
    \version \$Id: mkvmerge.cpp,v 1.33 2003/04/17 18:15:18 mosu Exp $
    \brief command line parameter parsing, looping, output handling
    \author Moritz Bunkus         <moritz @ bunkus.org>
*/

#include <errno.h>
#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#ifndef WIN32
#include <unistd.h>
#endif

#include <iostream>

#ifdef LIBEBML_GCC2
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
#include "KaxCues.h"
#include "KaxInfo.h"
#include "KaxInfoData.h"

#include "common.h"
#include "queue.h"
#include "r_ac3.h"
#include "r_avi.h"
#include "r_mp3.h"
#include "r_wav.h"
#ifdef HAVE_OGGVORBIS
#include "r_ogm.h"
#endif
#include "r_srt.h"
#include "r_matroska.h"

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
int max_blocks_per_cluster = 65535;
int max_ms_per_cluster = 1000;

float video_fps = -1.0;

cluster_helper_c *cluster_helper = NULL;
KaxSegment       *kax_segment;
KaxInfo          *kax_infos;
KaxTracks        *kax_tracks;
KaxTrackEntry    *kax_last_entry;
KaxCues          *kax_cues;
int               track_number = 0;

KaxDuration      *kax_duration;

StdIOCallback *out;

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
//    {"   ", TYPEMICRODVD, "MicroDVD text subtitles"},
//    {"idx", TYPEVOBSUB, "VobSub subtitles"},
   {"mp3", TYPEMP3, "MPEG1 layer III audio (CBR and VBR/ABR)"},
   {"ac3", TYPEAC3, "A/52 (aka AC3)"},
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
   {NULL,  -1,      NULL}};

static void usage(void) {
  fprintf(stdout,
    "mkvmerge -o out [global options] [options] <file1> [[options] <file2> ...]"
    "\n\n Global options:\n"
    "  -v, --verbose            verbose status\n"
    "  -q, --quiet              suppress status output\n"
    "  -o, --output out         Write to the file 'out'.\n"
    "  --cluster-length <n[ms]> Put at most n data blocks into each cluster.\n"
    "                           If the number is postfixed with 'ms' then\n"
    "                           put at most n milliseconds of data into each\n"
    "                           cluster.\n"
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
    "  -y, --sync <d[,o[/p]]>   Ssynchronize, delay the audio track by d ms.\n"
    "                           d > 0: Pad with silent samples.\n"
    "                           d < 0: Remove samples from the beginning.\n"
    "                           o/p: Adjust the timestamps by o/p to fix\n"
    "                           linear drifts. p defaults to 1000 if\n"
    "                           omitted. Both o and p can be floating point\n"
    "                           numbers.\n"
    "  -f, --fourcc <FOURCC>    Forces the FourCC to the specified value.\n"
    "                           Works only for video tracks.\n"
    "\n"
    " Other options:\n"
    "  -l, --list-types         Lists supported input file types.\n"
    "  -h, --help               Show this help.\n"
    "  -V, --version            Show version information.\n"
  );
}

static int get_type(char *filename) {
  FILE *f = fopen(filename, "r");
  int64_t size;
  
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
  else if (wav_reader_c::probe_file(f, size))
    return TYPEWAV;
#ifdef HAVE_OGGVORBIS
  else if (ogm_reader_c::probe_file(f, size))
    return TYPEOGM;
#endif // HAVE_OGGVORBIS
  else if (srt_reader_c::probe_file(f, size))
    return TYPESRT;
  else if (mp3_reader_c::probe_file(f, size))
    return TYPEMP3;
  else if (ac3_reader_c::probe_file(f, size))
    return TYPEAC3;
 else if (mkv_reader_c::probe_file(f, size))
   return TYPEMATROSKA;
//     else if (microdvd_reader_c::probe_file(f, size))
//     return TYPEMICRODVD;
//   else if (vobsub_reader_c::probe_file(f, size)) 
//     return TYPEVOBSUB;
//   else if (chapter_information_probe(f, size))
//     return TYPECHAPTERS;
  else
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

static unsigned char *parse_tracks(char *s) {
  char *c = s;
  char *nstart;
  int n, ntracks;
  unsigned char *tracks;
  
  nstart = NULL;
  tracks = NULL;
  ntracks = 0;
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
        tracks = (unsigned char *)realloc(tracks, ntracks + 2);
        if (tracks == NULL)
          die("malloc");
        tracks[ntracks] = (unsigned char)n;
        tracks[ntracks + 1] = 0;
        nstart = NULL;
        ntracks++;
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
    tracks = (unsigned char *)realloc(tracks, ntracks + 2);
    if (tracks == NULL)
      die("malloc");
    tracks[ntracks] = (unsigned char)n;
    tracks[ntracks + 1] = 0;
    nstart = NULL;
    ntracks++;
  }
  
  return tracks;
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

// static double parse_time(char *s) {
//   char *c, *a, *dot;
//   int num_colons;
//   double seconds;
  
//   dot = strchr(s, '.');
//   if (dot != NULL) {
//     *dot = 0;
//     dot++;
//   }
//   for (c = s, num_colons = 0; *c; c++) {
//     if (*c == ':')
//       num_colons++;
//     else if ((*c < '0') || (*c > '9')) {
//       fprintf(stderr, "Error: illegal character '%c' in time range.\n", *c);
//       exit(1);
//     }
//   }
//   if (num_colons > 2) {
//     fprintf(stderr, "Error: illegal time range: %s.\n", s);
//     exit(1);
//   }
//   if (num_colons == 0) {
//     seconds = strtod(s, NULL);
//     if (dot != NULL)
//       seconds += strtod(dot, NULL) / 1000.0;
//   }
//   for (a = s, c = s, seconds = 0; *c; c++) {
//     if (*c == ':') {
//       *c = 0;
//       seconds *= 60;
//       seconds += atoi(a);
//       a = c + 1;
//     }
//   }
//   seconds *= 60;
//   seconds += atoi(a);
  
//   if (dot != NULL)
//     seconds += strtod(dot, NULL) / 1000.0;
  
//   return seconds;
// }

static void render_head(StdIOCallback *out) {
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
  track_info_t     ti;
  int              i, j;
  int              noaudio, novideo, nosubs;
  filelist_t      *file;
  char            *s;

  noaudio = 0;
  novideo = 0;
  nosubs  = 0;

  memset(&ti, 0, sizeof(track_info_t));
  ti.async.linear = 1.0;

  // First parse options that either just print some infos and then exit
  // or that are needed right at the beginning.
  for (i = 1; i < argc; i++)
    if (!strcmp(argv[i], "-V") || !strcmp(argv[i], "--version")) {
      fprintf(stdout, "mkvmerge v" VERSION "\n");
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
      outfile = argv[i + 1];
      i++;
    } else if (!strcmp(argv[i], "-l") || !strcmp(argv[i], "--list-types")) {
      fprintf(stdout, "Known file types:\n  ext  description\n" \
              "  ---  --------------------------\n");
      for (j = 1; file_types[j].ext; j++)
        fprintf(stdout, "  %s  %s\n", file_types[j].ext, file_types[j].desc);
      exit(0);
    }

  if (outfile == NULL) {
    fprintf(stderr, "Error: no output files given.\n");
    exit(1);
  }

  // Open the output file.
  try {
    out = new StdIOCallback(outfile, MODE_CREATE);
  } catch (std::exception &ex) {
    fprintf(stderr, "Error: Couldn't open output file %s (%s).\n", outfile,
            strerror(errno));
    exit(1);
  }

  try {
    render_head(out);

    kax_cues = new KaxCues();
    kax_segment = new KaxSegment();

    kax_infos = &GetChild<KaxInfo>(*kax_segment);
    KaxTimecodeScale &time_scale = GetChild<KaxTimecodeScale>(*kax_infos);
    *(static_cast<EbmlUInteger *>(&time_scale)) = TIMECODE_SCALE;

    kax_duration = &GetChild<KaxDuration>(*kax_infos);
    *(static_cast<EbmlFloat *>(kax_duration)) = 0.0;

    kax_segment->Render(static_cast<StdIOCallback &>(*out));
  } catch (std::exception &ex) {
    fprintf(stderr, "Error: Could not render the file header.\n");
    exit(1);
  }

  kax_tracks = &GetChild<KaxTracks>(*kax_segment);
  kax_last_entry = NULL;
  
  for (i = 1; i < argc; i++) {

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
    else if (!strcmp(argv[i], "--cluster-length")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: --cluster-length lacks the length.\n");
        exit(1);
      }
      s = strstr("ms", argv[i + 1]);
      if (s != NULL) {
        *s = 0;
        max_ms_per_cluster = strtol(argv[i + 1], NULL, 10);
        if ((errno != 0) || (max_ms_per_cluster < 0) ||
            (max_ms_per_cluster > 65535)) {
          fprintf(stderr, "Error: Cluster length out of range (0..65535).\n");
          exit(1);
        }
        max_blocks_per_cluster = 65535;
      } else {
        max_blocks_per_cluster = strtol(argv[i + 1], NULL, 10);
        if ((errno != 0) || (max_blocks_per_cluster < 0) ||
            (max_blocks_per_cluster > 65535)) {
          fprintf(stderr, "Error: Cluster length out of range (0..65535).\n");
          exit(1);
        }
        max_ms_per_cluster = 65535;
      }
      i++;
    }

    // Options that apply to the next input file only.
    else if (!strcmp(argv[i], "-A") || !strcmp(argv[i], "--noaudio"))
      noaudio = 1;
    else if (!strcmp(argv[i], "-D") || !strcmp(argv[i], "--novideo"))
      novideo = 1;
    else if (!strcmp(argv[i], "-S") || !strcmp(argv[i], "--nosubs"))
      nosubs = 1;
    else if (!strcmp(argv[i], "-a") || !strcmp(argv[i], "--atracks")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: -a lacks the stream number(s).\n");
        exit(1);
      }
      if (ti.atracks != NULL)
        free(ti.atracks);
      ti.atracks = parse_tracks(argv[i + 1]);
      i++;
    } else if (!strcmp(argv[i], "-d") || !strcmp(argv[i], "--vtracks")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: -d lacks the stream number(s).\n");
        exit(1);
      }
      if (ti.vtracks != NULL)
        free(ti.vtracks);
      ti.vtracks = parse_tracks(argv[i + 1]);
      i++;
    } else if (!strcmp(argv[i], "-s") || !strcmp(argv[i], "--stracks")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: -s lacks the stream number(s).\n");
        exit(1);
      }
      if (ti.stracks != NULL)
        free(ti.stracks);
      ti.stracks = parse_tracks(argv[i + 1]);
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
    } else if (!strcmp(argv[i], "-y") || !strcmp(argv[i], "--sync")) {
      if ((i + 1) >= argc) {
        fprintf(stderr, "Error: -y lacks the audio delay.\n");
        exit(1);
      }
      parse_sync(argv[i + 1], &ti.async);
      i++;
    }

    // The argument is an input file.
    else {
      if ((ti.atracks != NULL) && noaudio) {
        fprintf(stderr, "Error: -A and -a used on the same source file.\n");
        exit(1);
      }
      if ((ti.vtracks != NULL) && novideo) {
        fprintf(stderr, "Error: -D and -d used on the same source file.\n");
        exit(1);
      }
      if ((ti.stracks != NULL) && nosubs) {
        fprintf(stderr, "Error: -S and -s used on the same source file.\n");
        exit(1);
      }
      if (noaudio) {
        ti.atracks = (unsigned char *)malloc(1);
        if (ti.atracks == NULL)
          die("malloc");
        *ti.atracks = 0;
      }
      if (novideo) {
        ti.vtracks = (unsigned char *)malloc(1);
        if (ti.vtracks == NULL)
          die("malloc");
        *ti.vtracks = 0;
      }
      if (nosubs) {
        ti.stracks = (unsigned char *)malloc(1);
        if (ti.stracks == NULL)
          die("malloc");
        *ti.stracks = 0;
      }
      file = (filelist_t *)malloc(sizeof(filelist_t));
      if (file == NULL)
        die("malloc");

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
      try {
        switch (file->type) {
          case TYPEMATROSKA:
            file->reader = new mkv_reader_c(&ti);
            break;
#ifdef HAVE_OGGVORBIS
          case TYPEOGM:
            file->reader = new ogm_reader_c(&ti);
            break;
#endif // HAVE_OGGVORBIS
          case TYPEAVI:
            if (ti.stracks != NULL)
              fprintf(stderr, "Warning: -t/-T are ignored for AVI files.\n");
            file->reader = new avi_reader_c(&ti);
            break;
          case TYPEWAV:
            if ((ti.atracks != NULL) || (ti.vtracks != NULL) ||
                (ti.stracks != NULL))
              fprintf(stderr, "Warning: -a/-A/-d/-D/-t/-T are ignored for " \
                      "WAVE files.\n");
            file->reader = new wav_reader_c(&ti);
            break;
          case TYPESRT:
            if ((ti.atracks != NULL) || (ti.vtracks != NULL) ||
                (ti.stracks != NULL))
              fprintf(stderr, "Warning: -a/-A/-d/-D/-t/-T are ignored for " \
                      "SRT files.\n");
            file->reader = new srt_reader_c(&ti);
            break;
          case TYPEMP3:
            if ((ti.atracks != NULL) || (ti.vtracks != NULL) ||
                (ti.stracks != NULL))
              fprintf(stderr, "Warning: -a/-A/-d/-D/-t/-T are ignored for " \
                      "MP3 files.\n");
            file->reader = new mp3_reader_c(&ti);
            break;
          case TYPEAC3:
            if ((ti.atracks != NULL) || (ti.vtracks != NULL) ||
                (ti.stracks != NULL))
              fprintf(stderr, "Warning: -a/-A/-d/-D/-t/-T are ignored for " \
                      "AC3 files.\n");
            file->reader = new ac3_reader_c(&ti);
            break;
//           case TYPECHAPTERS:
//             if (chapters != NULL) {
//               fprintf(stderr, "Error: only one chapter file allowed.\n");
//               exit(1);
//             }
//             chapters = chapter_information_read(file->name);
//             break;
//           case TYPEMICRODVD:
//             if ((atracks != NULL) || (vtracks != NULL) ||
//                 (stracks != NULL))
//               fprintf(stderr, "Warning: -a/-A/-d/-D/-t/-T are ignored for " \
//                       "MicroDVD files.\n");
//             file->reader = new microdvd_reader_c(file->name, &async);
//             break;
//           case TYPEVOBSUB:
//             if ((atracks != NULL) || (vtracks != NULL) ||
//                 (stracks != NULL))
//               fprintf(stderr, "Warning: -a/-A/-d/-D/-t/-T are ignored for " \
//                       "VobSub files.\n");
//             file->reader = new vobsub_reader_c(file->name, &async);
//             break;
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
      if (file->type != TYPECHAPTERS) {
        file->status = EMOREDATA;
        file->next = NULL;
        file->pack = NULL;

        add_file(file);
      } else
        free(file);

      noaudio = 0;
      novideo = 0;
      nosubs = 0;
      if (ti.atracks != NULL)
        free(ti.atracks);
      if (ti.vtracks != NULL)
        free(ti.vtracks);
      if (ti.stracks != NULL)
        free(ti.stracks);
      memset(&ti, 0, sizeof(track_info_t));
      ti.async.linear = 1.0;
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

static int write_packet(packet_t *pack) {
  int64_t timecode;

  cluster_helper->add_packet(pack);
  timecode = cluster_helper->get_timecode();

  if (((pack->timestamp - timecode) > max_ms_per_cluster) ||
      (cluster_helper->get_packet_count() > max_blocks_per_cluster) ||
      (cluster_helper->get_cluster_content_size() > 1500000)) {
    cluster_helper->render(out);
    cluster_helper->add_cluster(new KaxCluster());
  }

  return 1;
}

int main(int argc, char **argv) {
  filelist_t *file, *winner;
  packet_t *pack;

  nice(2);

  cluster_helper = new cluster_helper_c();
  cluster_helper->add_cluster(new KaxCluster());

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
    write_packet(pack);
    
    winner->pack = NULL;
    
    /* display some progress information */
    if (verbose >= 1)
      display_progress(0);
  }

  // Render all remaining packets (if there are any).
  if ((cluster_helper != NULL) && (cluster_helper->get_packet_count() > 0))
    cluster_helper->render(out);

  // Render the cues.
  kax_cues->Render(*static_cast<StdIOCallback *>(out));

  // Now re-render the kax_infos and fill in the biggest timecode
  // as the file's duration.
  *(static_cast<EbmlFloat *>(kax_duration)) =
    cluster_helper->get_max_timecode() * 1000000.0 / TIMECODE_SCALE;
  out->setFilePointer(kax_infos->GetElementPosition());
  kax_infos->Render(*out);

  delete cluster_helper;

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
  
  delete out;

  return 0;
}
